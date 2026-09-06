from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import bind
import graph


class GraphTests(unittest.TestCase):
    def test_blocks_without_hosted_evidence(self) -> None:
        digest = "aa" * 32
        toolchain = "bb" * 32
        with tempfile.TemporaryDirectory() as tmp:
            index = graph.write_graph(
                tmp,
                subjects={
                    "uapi": digest,
                    "mlibc": digest,
                    "rootfs": digest,
                    "app": digest,
                },
                toolchain_digest=toolchain,
                profile="release",
                destination="iphonesimulator",
            )
            self.assertFalse(index["complete"])
            self.assertEqual(index["reports"], ["kernel-dependency:blocked"])
            payload = json.loads((Path(tmp) / "kernel-dependency.json").read_text(encoding="utf-8"))
            self.assertEqual(payload["result"], "blocked")
            self.assertEqual(payload["subject_digest"], digest)

    def test_pass_without_evidence_file_fails(self) -> None:
        digest = "aa" * 32
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(bind.BindError):
                graph.write_graph(
                    tmp,
                    subjects={
                        "uapi": digest,
                        "mlibc": digest,
                        "rootfs": digest,
                        "app": digest,
                    },
                    toolchain_digest="bb" * 32,
                    profile="release",
                    destination="iphonesimulator",
                    evidence={"kernel-dependency": str(Path(tmp) / "missing.txt")},
                )


if __name__ == "__main__":
    unittest.main()
