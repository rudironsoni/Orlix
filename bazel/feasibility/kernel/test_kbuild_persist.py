from __future__ import annotations

import os
import fcntl
import stat
import subprocess
import tarfile
import tempfile
import unittest
import sys
from pathlib import Path

import kbuild_persist as persist
from bazel.content_digest import tree_digest

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "OrlixKernel/Sources/ports/orlix/kbuild"))
import source_state


class KernelStateTests(unittest.TestCase):
    def test_source_updates_keep_unchanged_files_and_match_clean_preparation(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            linux, isa, port = root / "linux", root / "isa", root / "port"
            overlay = root / "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/example.c"
            overlay.parent.mkdir(parents=True)
            overlay.write_bytes(b"int value = 1;\n")
            for name, data in {"include/uapi/asm-generic/types.h": b"types\n", "include/value.h": b"old\n"}.items():
                path = linux / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
            isa.mkdir()
            (isa / "manifest").write_bytes(b"isa\n")
            config = root / "config"
            config.write_bytes(b"CONFIG_PAGE_SIZE_4KB=y\n")
            patch = root / "source.patch"
            patch.write_text("--- a/include/value.h\n+++ b/include/value.h\n@@ -1 +1 @@\n-old\n+new\n")
            def prepare(destination):
                return source_state.prepare(linux, destination, [overlay], [patch], config, isa, "release", root)
            prepare(port)
            before = {name: path.stat().st_mtime_ns for name, path in source_state._files(port).items()}
            self.assertEqual(prepare(port)["changed"], 0)
            self.assertEqual(before, {name: path.stat().st_mtime_ns for name, path in source_state._files(port).items()})
            old_time = overlay.stat().st_mtime_ns
            overlay.write_bytes(b"int value = 2;\n")
            os.utime(overlay, ns=(old_time, old_time))
            self.assertEqual(prepare(port)["changed"], 1)
            self.assertEqual((port / "include/value.h").read_bytes(), b"new\n")
            clean = root / "clean"
            prepare(clean)
            self.assertEqual(tree_digest(port), tree_digest(clean))
            execution = root / "execroot/_main"
            execution.mkdir(parents=True)
            state = root / "orlix-kernel-state/release/iphonesimulator"
            state.mkdir(parents=True)
            script = root / "build.sh"
            marker = root / "started"
            script.write_text('printf started > "$1"\n')
            environment = {**os.environ, "PROFILE": "release", "ORLIX_KERNEL_ARCHIVE_PLATFORMS": "iphonesimulator", "ORLIX_KERNEL_INCREMENTAL": "1", "PYTHONPATH": os.pathsep.join([str(Path(source_state.__file__).parent), str(Path(__file__).resolve().parents[3])])}
            with (state / "build.lock").open("w") as lock:
                fcntl.flock(lock, fcntl.LOCK_EX)
                child = subprocess.Popen([sys.executable, "-B", "-c", "import source_state,sys; sys.exit(source_state.run_locked(sys.argv[1],sys.argv[2:]))", str(script), str(marker)], cwd=execution, env=environment)
                try:
                    with self.assertRaises(subprocess.TimeoutExpired):
                        child.wait(timeout=0.2)
                    self.assertFalse(marker.exists())
                finally:
                    fcntl.flock(lock, fcntl.LOCK_UN)
                    self.assertEqual(child.wait(timeout=5), 0)
            self.assertEqual(marker.read_text(), "started")

    def test_corrupt_build_bytes_cannot_authorize_incremental_reuse(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            identity = root / "toolchain"
            identity.write_text("compiler flags and build rules\n")
            source_state.resume(root, [identity])
            output = root / "OrlixKernel/build/release/kernel.o"
            output.parent.mkdir(parents=True)
            output.write_bytes(b"correct object\n")
            source_state.record(root)
            source_state.resume(root, [identity])
            self.assertTrue(output.exists())
            source_state.record(root)
            timestamp = output.stat().st_mtime_ns
            output.write_bytes(b"corrupt object\n")
            os.utime(output, ns=(timestamp, timestamp))
            source_state.resume(root, [identity])
            self.assertFalse(output.exists())
            output.parent.mkdir(parents=True)
            output.write_bytes(b"correct object\n")
            source_state.record(root)
            os.utime(output, ns=(timestamp, timestamp + 3600 * 10**9))
            source_state.resume(root, [identity])
            self.assertFalse(output.exists())
            (root / "build-state.json").mkdir()
            (root / "build-identity").unlink()
            (root / "build-identity").mkdir()
            source_state.resume(root, [identity])
            external = root / "unrelated"
            external.write_bytes(b"unchanged\n")
            (root / "build-state.json").symlink_to(external)
            (root / "build-identity").unlink()
            (root / "build-identity").symlink_to(external)
            source_state.resume(root, [identity])
            self.assertEqual(external.read_bytes(), b"unchanged\n")
            (root / "build-state.json").write_text("[]")
            source_state.resume(root, [identity])
            redirected = root / "redirected"
            redirected.mkdir()
            link = root / "OrlixKernel/src"
            link.symlink_to(redirected)
            with self.assertRaises(ValueError):
                source_state.sync({"file": b"data"}, link / "linux", root)
            self.assertEqual(list(redirected.iterdir()), [])


class KbuildArchiveTests(unittest.TestCase):
    def test_archive_bytes_do_not_depend_on_source_timestamps(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "source"
            root.mkdir()
            source = root / "header.h"
            source.write_bytes(b"#define VALUE 1\n")
            first, second = Path(tmp) / "first.tar", Path(tmp) / "second.tar"
            persist.write_archive(str(root), str(first))
            os.utime(source, (1000, 1000))
            persist.write_archive(str(root), str(second))
            self.assertEqual(first.read_bytes(), second.read_bytes())
            self.assertEqual(source.stat().st_mtime, 1000)

    def test_publish_and_extract_cli_fixture(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "source"
            root.mkdir()
            (root / "manifest").write_text("manifest\n", encoding="utf-8")
            (root / "source_manifest.def").write_text("source\n", encoding="utf-8")
            (root / "manifest").chmod(0o755)
            archive, pin = Path(tmp) / "tables.tar.gz", Path(tmp) / "tables.sha256"
            extracted = Path(tmp) / "extracted"
            extracted.mkdir()
            members_file = Path(tmp) / "members"
            members = ["manifest", "source_manifest.def"]
            members_file.write_text("\n".join(members) + "\n", encoding="utf-8")
            self.assertEqual(
                persist.main(
                    [
                        "publish",
                        str(root),
                        str(archive),
                        str(pin),
                        *members,
                    ]
                ),
                0,
            )
            first_archive = archive.read_bytes()
            first_pin = pin.read_text(encoding="utf-8")
            os.utime(root / "manifest", (1000, 1000))
            second_archive = Path(tmp) / "second.tar.gz"
            second_pin = Path(tmp) / "second.sha256"
            self.assertEqual(
                persist.main(
                    [
                        "publish",
                        str(root),
                        str(second_archive),
                        str(second_pin),
                        *members,
                    ]
                ),
                0,
            )
            self.assertEqual(second_archive.read_bytes(), first_archive)
            self.assertEqual(
                second_pin.read_text(encoding="utf-8"),
                f"{first_pin.split()[0]}  {second_archive.name}\n",
            )
            self.assertEqual(
                persist.main(
                    [
                        "extract",
                        str(archive),
                        str(pin),
                        str(extracted),
                        str(members_file),
                    ]
                ),
                0,
            )
            with tarfile.open(archive, "r:gz") as published:
                self.assertEqual(published.getnames(), members)
                self.assertEqual(published.getmember("manifest").mode & 0o777, 0o644)
            self.assertEqual((extracted / "manifest").read_text(), "manifest\n")
            self.assertEqual((extracted / "source_manifest.def").read_text(), "source\n")

    def test_extract_rejects_invalid_pin_or_traversal_before_writes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "source"
            root.mkdir()
            (root / "manifest").write_text("manifest\n", encoding="utf-8")
            archive, pin = Path(tmp) / "tables.tar.gz", Path(tmp) / "tables.sha256"
            members_file = Path(tmp) / "members"
            output = Path(tmp) / "extracted"
            self.assertEqual(
                persist.main(
                    ["publish", str(root), str(archive), str(pin), "manifest"]
                ),
                0,
            )
            pin.write_text(f"{'0' * 64}  {archive.name}\n", encoding="utf-8")
            members_file.write_text("manifest\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                persist.main(
                    ["extract", str(archive), str(pin), str(output), str(members_file)]
                )
            self.assertFalse(output.exists())

            persist.write_pin(str(archive), str(pin))
            members_file.write_text("../escape\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                persist.main(
                    ["extract", str(archive), str(pin), str(output), str(members_file)]
                )
            self.assertFalse(output.exists())
            self.assertFalse((Path(tmp) / "escape").exists())


class ContentDigestTests(unittest.TestCase):
    def test_digest_changes_for_paths_modes_contents_and_symlink_targets(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp) / "tree"
            root.mkdir()
            source = root / "header.h"
            source.write_text("#define VALUE 1\n", encoding="utf-8")
            link = root / "current"
            link.symlink_to("header.h")

            original = tree_digest(root)
            source.write_text("#define VALUE 2\n", encoding="utf-8")
            self.assertNotEqual(original, tree_digest(root))

            source.chmod(stat.S_IRUSR | stat.S_IWUSR)
            mode_changed = tree_digest(root)
            source.chmod(stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP)
            mode_updated = tree_digest(root)
            self.assertNotEqual(mode_changed, mode_updated)

            link.unlink()
            link.symlink_to("missing.h")
            symlink_changed = tree_digest(root)
            self.assertNotEqual(mode_updated, symlink_changed)

            source.rename(root / "renamed.h")
            self.assertNotEqual(symlink_changed, tree_digest(root))


class HeadersInstallTests(unittest.TestCase):
    def test_removed_headers_cannot_survive_installed_or_staging_state(self) -> None:
        repository = Path(__file__).resolve().parents[3]
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "linux"
            build = root / "build"
            output = build / "OrlixMLibC/kernel-headers/release"
            kbuild = build / "OrlixKernel/build/release-uapi-arm64"
            staging = kbuild / "usr/include"
            for headers in (source / "include/uapi", output / "include", staging):
                (headers / "linux").mkdir(parents=True)
                (headers / "linux/removed.h").write_text("obsolete\n")
            (source / "include/uapi/linux/removed.h").unlink()
            (source / "include/uapi/linux/current.h").write_text("#define CURRENT 1\n")
            (source / ".orlix-port-profile").write_text("old profile\n")
            (output / ".orlix-headers-ready").write_text("old output\n")
            os.utime(source / ".orlix-port-profile", (1000, 1000))
            os.utime(output / ".orlix-headers-ready", (2000, 2000))
            state = kbuild / "upstream-state"
            state.write_bytes(b"retain upstream incremental state\n")
            (source / "Makefile").write_text(
                ".PHONY: headers_install\nheaders_install:\n"
                '\t@test ! -e "$(INSTALL_HDR_PATH)/include/linux/removed.h"\n'
                '\t@test ! -e "$(O)/usr/include/linux/removed.h"\n'
                '\t@mkdir -p "$(O)/usr/include/linux" "$(INSTALL_HDR_PATH)/include"\n'
                '\t@cp include/uapi/linux/current.h "$(O)/usr/include/linux/current.h"\n'
                '\t@cp -R "$(O)/usr/include/." "$(INSTALL_HDR_PATH)/include/"\n'
            )
            result = subprocess.run(
                ["gmake", "-f", "OrlixKernel/Sources/ports/orlix/kbuild/kernel-rules.mk",
                 "-o", "__prepare-port", "__headers-install", "PROFILE=release",
                 f"ORLIX_BUILD_ROOT={build}", f"ORLIX_KERNEL_PORT_DIR={source}"],
                cwd=repository, capture_output=True, text=True, timeout=30,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for headers in (output / "include", staging):
                self.assertFalse((headers / "linux/removed.h").exists())
                self.assertEqual(tree_digest(headers), tree_digest(source / "include/uapi"))
            self.assertEqual(state.read_bytes(), b"retain upstream incremental state\n")


if __name__ == "__main__":
    unittest.main()
