from pathlib import Path
import json
import os
import tempfile
import unittest
from unittest.mock import patch

from bazel.feasibility.packages import build_state


class PackageStateTests(unittest.TestCase):
    def test_source_edits_preserve_upstream_generated_files_and_build_state(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            source, work = base / "source", base / "work"
            source.mkdir()
            work.mkdir()
            (source / "configure").write_bytes(b"upstream configure\n")
            (source / "lib").mkdir()
            config_header = source / "lib/config.hin"
            config_header.write_bytes(b"#undef HAVE_FEATURE\n")
            leaf = source / "leaf.c"
            leaf.write_bytes(b"int value = 1;\n")
            arguments = [str(source / "configure")]
            for name in ("headers", "uapi", "libraries"):
                tree = base / name
                tree.mkdir()
                (tree / "input").write_bytes(b"dependency\n")
                arguments.append(str(tree))
            runtime = base / "runtime.a"
            runtime.write_bytes(b"runtime\n")
            arguments.append(str(runtime))
            build_state.prepare(work, arguments)
            (work / "build").mkdir()
            (work / "build/Makefile").write_bytes(b"configured\n")
            (work / "build/leaf.o").write_bytes(b"compiled\n")
            generated = work / "src/configure"
            generated.write_bytes(b"upstream regenerated configure\n")
            generated.chmod(0o444)
            before = generated.stat().st_mtime_ns
            build_state.prepare(work, arguments)
            self.assertEqual(generated.stat().st_mtime_ns, before)
            self.assertEqual((work / "inputs/configure-required").read_text(), "0")
            timestamp = leaf.stat().st_mtime_ns
            leaf.write_bytes(b"int value = 2;\n")
            os.utime(leaf, ns=(timestamp, timestamp))
            build_state.prepare(work, arguments)
            self.assertEqual((work / "src/leaf.c").read_bytes(), leaf.read_bytes())
            self.assertEqual(generated.read_bytes(), b"upstream regenerated configure\n")
            self.assertEqual((work / "build/leaf.o").read_bytes(), b"compiled\n")
            self.assertEqual((work / "inputs/configure-required").read_text(), "0")
            runtime.write_bytes(b"updated runtime\n")
            build_state.prepare(work, arguments)
            self.assertEqual((work / "inputs/configure-required").read_text(), "1")
            build_state.prepare(work, arguments)
            self.assertEqual((work / "inputs/configure-required").read_text(), "0")
            config_header.write_bytes(b"#undef HAVE_OTHER_FEATURE\n")
            build_state.prepare(work, arguments)
            self.assertEqual((work / "inputs/configure-required").read_text(), "1")
            build_state.prepare(work, arguments)
            (source / "config.h.in").write_bytes(b"#undef BASH_FEATURE\n")
            build_state.prepare(work, arguments)
            self.assertEqual((work / "inputs/configure-required").read_text(), "1")
            in_tree = base / "in-tree"
            in_tree.mkdir()
            build_state.prepare(in_tree, arguments, in_tree=True)
            (in_tree / "build/Makefile").write_bytes(b"configured\n")
            (in_tree / "build/leaf.o").write_bytes(b"compiled\n")
            generated = in_tree / "build/configure"
            generated.write_bytes(b"upstream regenerated configure\n")
            generated.chmod(0o444)
            before = generated.stat().st_mtime_ns
            leaf.write_bytes(b"int value = 3;\n")
            os.utime(leaf, ns=(timestamp, timestamp))
            build_state.prepare(in_tree, arguments, in_tree=True)
            self.assertEqual(generated.stat().st_mtime_ns, before)
            self.assertEqual((in_tree / "build/leaf.c").read_bytes(), leaf.read_bytes())
            self.assertEqual((in_tree / "build/leaf.o").read_bytes(), b"compiled\n")
            self.assertEqual((in_tree / "inputs/configure-required").read_text(), "0")
            (source / "config").mkdir()
            (source / "config/ltmain.sh").write_text("upstream libtool generator")
            build_state.prepare(in_tree, arguments, in_tree=True)
            self.assertEqual((in_tree / "inputs/configure-required").read_text(), "1")

    def test_package_name_cannot_escape_local_state(self):
        with self.assertRaisesRegex(ValueError, "invalid package state name"):
            build_state.run("../bash", "", "", "", [])

    def test_cross_package_state_is_rejected_before_build(self):
        with tempfile.TemporaryDirectory() as temporary:
            base = Path(temporary)
            identity = base / "tool"
            identity.write_text("same tools and build rules")
            roots = {name: base / "orlix-package-state" / name / "aarch64-linux-gnu"
                     for name in ("bash", "coreutils")}
            with patch.object(Path, "cwd", return_value=base / "execroot/_main"), \
                 patch.dict(os.environ, {"ORLIX_PACKAGE_INCREMENTAL": "1"}), \
                 patch.object(build_state, "prepare"), \
                 patch.object(build_state.subprocess, "call", return_value=0) as upstream:
                for package, root in roots.items():
                    self.assertEqual(build_state.run(package, str(identity), str(identity), str(identity), []), 0)
                    record = json.loads((root / "build-state.json").read_text())
                    self.assertEqual(record["compatibility"], {"package": package, "target": "aarch64-linux-gnu"})
                self.assertNotEqual(roots["bash"].resolve(), roots["coreutils"].resolve())
                self.assertFalse(os.path.samefile(roots["bash"] / "build.lock", roots["coreutils"] / "build.lock"))
                records = {name: (root / "build-state.json").read_bytes() for name, root in roots.items()}
                for producer, consumer in (("coreutils", "bash"), ("bash", "coreutils")):
                    with self.subTest(producer=producer, consumer=consumer):
                        root = roots[consumer]
                        (root / "build-state.json").write_bytes(records[producer])
                        upstream.reset_mock()
                        with self.assertRaisesRegex(RuntimeError, f"package state mismatch: {producer} != {consumer}"):
                            build_state.run(consumer, str(identity), str(identity), str(identity), [])
                        upstream.assert_not_called()
                        self.assertEqual((root / "build-state.json").read_bytes(), records[producer])
