from __future__ import annotations

import hashlib
import json
import tempfile
import unittest
from pathlib import Path

import bind
import graph
from test_bind import SUBJECT, TOOLCHAIN, write_evidence


class GraphTests(unittest.TestCase):
    def test_blocks_without_hosted_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            index = graph.write_graph(
                tmp, subjects={"kernel": SUBJECT}, toolchain_digest=TOOLCHAIN,
                profile="release", destination="iphonesimulator", buildset_digest=SUBJECT,
            )
            self.assertFalse(index["complete"])
            self.assertEqual(index["reports"], ["kernel-dependency:blocked"])
            self.assertEqual(index["buildset_digest"], SUBJECT)
            report = json.loads((Path(tmp) / "kernel-dependency.json").read_text())
            self.assertEqual(report["subject_digest"], SUBJECT)

    def test_select_matching_live_digest_records_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            live = Path(tmp) / "sysroot.sha256"
            mismatch = Path(tmp) / "mismatch.json"
            live.write_text("bb" * 32)
            with self.assertRaisesRegex(bind.BindError, "does not match"):
                graph.select_matching_live_digest({"mlibc": SUBJECT}, "mlibc", str(live), str(mismatch))
            self.assertEqual(json.loads(mismatch.read_text())["live_digest"], "bb" * 32)
            live.write_text(SUBJECT)
            self.assertEqual(graph.select_matching_live_digest({"mlibc": SUBJECT}, "mlibc", str(live)), str(live))

    def test_lock_unsigned_digest_mismatch_fails(self) -> None:
        with self.assertRaises(bind.BindError):
            graph.merge_subjects({"mlibc": SUBJECT}, {"mlibc": "bb" * 32})

    def test_complete_graph_binds_evidence_and_report_digests(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            markers = {
                "kernel-dependency": "T _OrlixBoot\nT _arch_boot_entry\n",
                "kunit": "ORLIX-KSELFTEST-END\n",
                "kselftest": "ORLIX-KSELFTEST-END\n",
                "orlixmlibc": "ORLIX-MLIBC-TEST-END\n",
                "syscall-uapi": "ORLIX-KSELFTEST-END\n",
                "posix-shell": "ORLIX-COREUTILS-TEST-END failures=0 skips=0 total=1\n",
            }
            evidence = {
                tier: str(write_evidence(root, tier, markers.get(tier, "** TEST SUCCEEDED **\n"), SUBJECT))
                for tier in bind.TIERS
            }
            out = root / "reports"
            self.assertEqual(graph.main([
                "--out", str(out), "--uapi-digest", SUBJECT, "--kernel-digest", SUBJECT,
                "--mlibc-digest", SUBJECT, "--rootfs-digest", SUBJECT, "--app-digest", SUBJECT,
                "--toolchain-digest", TOOLCHAIN, "--buildset-digest", SUBJECT,
                *[arg for tier, path in evidence.items() for arg in ("--evidence", f"{tier}={path}")],
            ]), 0)
            self.assertTrue(json.loads((out / "index.json").read_text())["complete"])
            prior = []
            for tier in bind.TIERS:
                path = out / f"{tier}.json"
                report = json.loads(path.read_text())
                self.assertEqual(report["prerequisite_digests"], prior)
                self.assertIn("evidence_digest", report["evidence"])
                prior.append(hashlib.sha256(path.read_bytes()).hexdigest())

    def test_invalid_evidence_cannot_pass(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for change in ("raw", "identity", "failed", "tampered", "missing_end"):
                with self.subTest(change=change):
                    evidence = write_evidence(root, "kernel-dependency", "T _OrlixBoot\nT _arch_boot_entry\n")
                    payload = json.loads(evidence.read_text())
                    if change == "raw":
                        evidence.write_text("T _OrlixBoot\nT _arch_boot_entry\n")
                    elif change == "tampered":
                        (root / "artifact").write_bytes(b"changed")
                    else:
                        if change == "identity":
                            payload["profile"] = "development"
                        elif change == "failed":
                            payload["exit_code"] = 1
                        else:
                            log = root / "kernel-dependency.log"
                            log.write_text("started\n")
                            payload["log_digest"] = hashlib.sha256(log.read_bytes()).hexdigest()
                        evidence.write_text(json.dumps(payload))
                    with self.assertRaises(bind.BindError):
                        graph.write_graph(
                            str(root / "reports"), subjects={"kernel": SUBJECT}, toolchain_digest=TOOLCHAIN,
                            profile="release", destination="iphonesimulator",
                            evidence={"kernel-dependency": str(evidence)},
                        )


if __name__ == "__main__":
    unittest.main()
