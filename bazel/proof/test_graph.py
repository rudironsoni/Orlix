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

    def test_kernel_subject_uses_locked_profile_destination_identity(self) -> None:
        identity = "ab" * 32
        other = "cd" * 32
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            lock = root / "lock.json"
            lock.write_text(json.dumps({
                "schema": 2,
                "buildset": SUBJECT,
                "components": {
                    "kernel-release-iphonesimulator": {"unsigned_digest": identity},
                    "kernel-development-iphoneos": {"unsigned_digest": other},
                },
            }))
            subjects, buildset = graph.subjects_from_lock(
                str(lock), profile="release", destination="iphonesimulator"
            )
            self.assertEqual(subjects["kernel"], identity)
            self.assertEqual(buildset, SUBJECT)
            device = graph.subjects_from_lock(str(lock), profile="development", destination="iphoneos")[0]
            self.assertEqual(device["kernel"], other)
            live = root / "kernel.artifact-identity-v2.sha256"
            mismatch = root / "kernel-live-mismatch.json"
            live.write_text(other + "\n")
            with self.assertRaisesRegex(bind.BindError, "does not match"):
                graph.select_matching_live_digest(subjects, "kernel", str(live), str(mismatch))
            recorded = json.loads(mismatch.read_text())
            self.assertEqual(recorded["component"], "kernel")
            self.assertEqual(recorded["live_digest"], other)
            self.assertEqual(recorded["lock_unsigned_digest"], identity)
            live.write_text(identity + "\n")
            self.assertEqual(
                graph.select_matching_live_digest(subjects, "kernel", str(live), str(mismatch)),
                str(live),
            )
            with self.assertRaisesRegex(bind.BindError, "kernel-release-iphoneos"):
                graph.subjects_from_lock(str(lock), profile="release", destination="iphoneos")
            with self.assertRaisesRegex(bind.BindError, "locked profile and destination"):
                graph.subjects_from_lock(str(lock), profile="release", destination="macos")

    def test_matching_kernel_identity_stays_blocked_without_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            lock = root / "lock.json"
            lock.write_text(json.dumps({
                "buildset": TOOLCHAIN,
                "components": {
                    "kernel-release-iphonesimulator": {"unsigned_digest": SUBJECT},
                },
            }))
            digest = root / "kernel.artifact-identity-v2.sha256"
            digest.write_text(SUBJECT + "\n")
            out = root / "reports"
            self.assertEqual(graph.main([
                "--out", str(out),
                "--lock", str(lock),
                "--uapi-digest", SUBJECT,
                "--kernel-digest", str(digest),
                "--toolchain-digest", TOOLCHAIN,
                "--profile", "release",
                "--destination", "iphonesimulator",
            ]), 0)
            report = json.loads((out / "kernel-dependency.json").read_text())
            self.assertEqual(report["subject_digest"], SUBJECT)
            self.assertEqual(report["result"], "blocked")
            self.assertFalse(json.loads((out / "index.json").read_text())["complete"])

    def test_uapi_and_mlibc_use_locked_artifact_identity(self) -> None:
        uapi = "11" * 32
        mlibc = "22" * 32
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            lock = root / "lock.json"
            lock.write_text(json.dumps({
                "buildset": TOOLCHAIN,
                "components": {
                    "uapi": {"unsigned_digest": uapi},
                    "mlibc": {"unsigned_digest": mlibc},
                    "kernel-release-iphonesimulator": {"unsigned_digest": SUBJECT},
                },
            }))
            subjects = graph.subjects_from_lock(
                str(lock), profile="release", destination="iphonesimulator"
            )[0]
            self.assertEqual(subjects["uapi"], uapi)
            self.assertEqual(subjects["mlibc"], mlibc)
            uapi_live = root / "uapi.artifact-identity-v2.sha256"
            mlibc_live = root / "sysroot.artifact-identity-v2.sha256"
            kernel_live = root / "kernel.artifact-identity-v2.sha256"
            uapi_live.write_text(("ab" * 32) + "\n")
            mlibc_live.write_text(mlibc + "\n")
            kernel_live.write_text(SUBJECT + "\n")
            uapi_mismatch = root / "uapi-live-mismatch.json"
            mlibc_mismatch = root / "mlibc-live-mismatch.json"
            with self.assertRaisesRegex(bind.BindError, "uapi"):
                graph.select_matching_live_digest(subjects, "uapi", str(uapi_live), str(uapi_mismatch))
            self.assertEqual(json.loads(uapi_mismatch.read_text())["lock_unsigned_digest"], uapi)
            uapi_live.write_text(uapi + "\n")
            mlibc_live.write_text(("cd" * 32) + "\n")
            with self.assertRaisesRegex(bind.BindError, "mlibc"):
                graph.select_matching_live_digest(subjects, "mlibc", str(mlibc_live), str(mlibc_mismatch))
            recorded = json.loads(mlibc_mismatch.read_text())
            self.assertEqual(recorded["component"], "mlibc")
            self.assertEqual(recorded["lock_unsigned_digest"], mlibc)
            mlibc_live.write_text(mlibc + "\n")
            self.assertIsNone(graph.select_matching_live_digest(
                subjects, "mlibc", str(root / "missing-sysroot.artifact-identity-v2.sha256")
            ))
            evidence = []
            for tier, marker in (
                ("kernel-dependency", "Run /init as init process\n"),
                ("kunit", "ORLIX-KSELFTEST-END\n"),
                ("kselftest", "ORLIX-KSELFTEST-END\n"),
            ):
                path = write_evidence(root, tier, marker, TOOLCHAIN)
                evidence.extend(["--evidence", f"{tier}={path}"])
            out = root / "reports"
            self.assertEqual(graph.main([
                "--out", str(out),
                "--lock", str(lock),
                "--uapi-digest", str(uapi_live),
                "--kernel-digest", str(kernel_live),
                "--mlibc-digest", str(mlibc_live),
                "--toolchain-digest", TOOLCHAIN,
                "--profile", "release",
                "--destination", "iphonesimulator",
                *evidence,
            ]), 0)
            index = json.loads((out / "index.json").read_text())
            self.assertEqual(index["reports"], [
                "kernel-dependency:pass",
                "kunit:pass",
                "kselftest:pass",
                "orlixmlibc:blocked",
            ])
            report = json.loads((out / "orlixmlibc.json").read_text())
            self.assertEqual(report["subject_digest"], mlibc)
            self.assertEqual(report["owner"], "OrlixMLibC")
            self.assertEqual(report["result"], "blocked")
            self.assertFalse((out / "syscall-uapi.json").exists())
            with self.assertRaisesRegex(bind.BindError, "uapi"):
                graph.main([
                    "--out", str(root / "rejected-uapi"),
                    "--lock", str(lock),
                    "--uapi-digest", "ab" * 32,
                    "--kernel-digest", SUBJECT,
                    "--mlibc-digest", mlibc,
                    "--toolchain-digest", TOOLCHAIN,
                    "--profile", "release",
                    "--destination", "iphonesimulator",
                ])
            with self.assertRaisesRegex(bind.BindError, "mlibc"):
                graph.main([
                    "--out", str(root / "rejected-mlibc"),
                    "--lock", str(lock),
                    "--uapi-digest", uapi,
                    "--kernel-digest", SUBJECT,
                    "--mlibc-digest", "cd" * 32,
                    "--toolchain-digest", TOOLCHAIN,
                    "--profile", "release",
                    "--destination", "iphonesimulator",
                ])

    def test_lock_rejects_kernel_identity_from_another_variant(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            lock = root / "lock.json"
            lock.write_text(json.dumps({
                "components": {
                    "kernel-release-iphonesimulator": {"unsigned_digest": SUBJECT},
                },
            }))
            with self.assertRaisesRegex(bind.BindError, "does not match"):
                graph.main([
                    "--out", str(root / "reports"),
                    "--lock", str(lock),
                    "--uapi-digest", SUBJECT,
                    "--kernel-digest", "cd" * 32,
                    "--toolchain-digest", TOOLCHAIN,
                    "--profile", "release",
                    "--destination", "iphonesimulator",
                ])

    def test_complete_graph_binds_evidence_and_report_digests(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            markers = {
                "kernel-dependency": "Run /init as init process\n",
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
                    evidence = write_evidence(root, "kernel-dependency", "Run /init as init process\n")
                    payload = json.loads(evidence.read_text())
                    if change == "raw":
                        evidence.write_text("Run /init as init process\n")
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
