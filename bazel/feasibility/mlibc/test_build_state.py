"""Mlibc incremental state without compiling mlibc or running Meson."""

from pathlib import Path
import tempfile
import unittest

from bazel.feasibility.mlibc import build_state
from bazel.feasibility.mlibc.header_digest import followed_tree_digest


class HeaderDigestTests(unittest.TestCase):
    def test_followed_digest_ignores_symlink_spelling(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            relative = root / "relative"
            absolute = root / "absolute"
            relative.mkdir()
            absolute.mkdir()
            (relative / "real.h").write_bytes(b"same\n")
            (relative / "link.h").symlink_to("real.h")
            (absolute / "real.h").write_bytes(b"same\n")
            (absolute / "link.h").symlink_to(absolute / "real.h")
            self.assertEqual(followed_tree_digest(relative), followed_tree_digest(absolute))
            (absolute / "real.h").write_bytes(b"changed\n")
            self.assertNotEqual(followed_tree_digest(relative), followed_tree_digest(absolute))

    def test_directory_symlink_records_logical_paths(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            generic = root / "asm-generic"
            generic.mkdir()
            (generic / "unistd.h").write_bytes(b"syscall\n")
            (root / "asm").symlink_to("asm-generic")
            first = followed_tree_digest(root)
            (generic / "unistd.h").write_bytes(b"syscall changed\n")
            self.assertNotEqual(first, followed_tree_digest(root))


class MlibcStateTests(unittest.TestCase):
    def _prepare(self, work: Path, source: Path, uapi: Path, runtime: Path) -> None:
        build_state.prepare(work, source, [], {}, uapi, runtime)

    def _fixture(self, base: Path):
        source = base / "source"
        source.mkdir()
        (source / "meson.build").write_bytes(b"project('mlibc')\n")
        (source / "leaf.c").write_bytes(b"int leaf;\n")
        uapi = base / "uapi"
        (uapi / "include" / "linux").mkdir(parents=True)
        (uapi / "include" / "linux" / "unistd.h").write_bytes(b"int read(void);\n")
        runtime = base / "runtime.a"
        runtime.write_bytes(b"runtime\n")
        work = base / "work"
        work.mkdir()
        return source, uapi, runtime, work

    def test_source_edit_keeps_ninja_directory(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source, uapi, runtime, work = self._fixture(Path(temporary))
            self._prepare(work, source, uapi, runtime)
            self._prepare(work, source, uapi, runtime)
            ninja = work / "build" / "build.ninja"
            ninja.parent.mkdir()
            ninja.write_bytes(b"rule c_COMPILER\n")
            (source / "leaf.c").write_bytes(b"int leaf = 2;\n")
            self._prepare(work, source, uapi, runtime)
            self.assertEqual((work / "mlibc" / "leaf.c").read_bytes(), b"int leaf = 2;\n")
            self.assertEqual(ninja.read_bytes(), b"rule c_COMPILER\n")
            self.assertEqual((work / "inputs" / "configure-cache").read_text(), "keep")
            self.assertEqual(build_state.meson_setup_plan("keep", ninja.is_file()), "skip")

    def test_uapi_content_change_clears_meson_cache(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            source, uapi, runtime, work = self._fixture(Path(temporary))
            self._prepare(work, source, uapi, runtime)
            self._prepare(work, source, uapi, runtime)
            ninja = work / "build" / "build.ninja"
            ninja.parent.mkdir()
            ninja.write_bytes(b"build.ninja\n")
            before = (work / "inputs" / "followed-header.sha256").read_text()
            (uapi / "include" / "linux" / "unistd.h").write_bytes(b"int write(void);\n")
            self._prepare(work, source, uapi, runtime)
            self.assertEqual((work / "inputs" / "configure-cache").read_text(), "clear")
            self.assertNotEqual((work / "inputs" / "followed-header.sha256").read_text(), before)
            self.assertEqual(ninja.read_bytes(), b"build.ninja\n")
            self.assertEqual(build_state.meson_setup_plan("clear", True), "--reconfigure --clearcache")

    def test_meson_setup_is_skipped_only_when_graph_is_valid(self) -> None:
        self.assertEqual(build_state.meson_setup_plan("keep\n", True), "skip")
        self.assertEqual(build_state.meson_setup_plan("clear", True), "--reconfigure --clearcache")
        self.assertEqual(build_state.meson_setup_plan("keep", False), "")
        self.assertEqual(build_state.meson_setup_plan("clear", False), "")

    def test_resume_identity_does_not_include_mlibc_sources(self) -> None:
        identity = [path.name for path in build_state.resume_identity(
            Path("toolchain.json"), Path("compiler.json"), Path("sysroot.sh"), Path("launcher")
        )]
        self.assertEqual(
            identity,
            ["toolchain.json", "compiler.json", "sysroot.sh", "build_state.py", "build_state.py", "launcher"],
        )
        self.assertNotIn("leaf.c", identity)
        self.assertNotIn("meson.build", identity)

    def test_ninja_log_compiler_edges_ignore_stamp_files(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build_ninja = root / "build.ninja"
            build_ninja.write_text(
                "rule c_COMPILER\n"
                "  command = clang -c $in -o $out\n"
                "rule CUSTOM_COMMAND\n"
                "  command = touch $out\n"
                "build src/foo.c.o: c_COMPILER ../src/foo.c\n"
                "build src/stamp: CUSTOM_COMMAND src/foo.c.o\n",
                encoding="utf-8",
            )
            log = root / ".ninja_log"
            log.write_text(
                "# ninja log v5\n"
                "100\t200\t200\tsrc/foo.c.o\thash1\n"
                "100\t250\t250\tsrc/stamp\thash2\n"
                "300\t400\t400\tsrc/foo.c.o\thash3\n"
                "300\t450\t450\tsrc/stamp\thash4\n",
                encoding="utf-8",
            )
            self.assertEqual(build_state.ninja_log_end(log), 450)
            self.assertEqual(build_state.compiler_edges_since(log, build_ninja, 250), ["src/foo.c.o"])
            self.assertEqual(build_state.compiler_edges_since(log, build_ninja, 400), [])


if __name__ == "__main__":
    unittest.main()
