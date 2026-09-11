from pathlib import Path
import os
import tempfile
import unittest

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
