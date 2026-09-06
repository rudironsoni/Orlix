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

    def test_records_locked_buildset_digest(self) -> None:
        digest = "aa" * 32
        buildset = "cc" * 32
        with tempfile.TemporaryDirectory() as tmp:
            index = graph.write_graph(
                tmp,
                subjects={"uapi": digest},
                toolchain_digest="bb" * 32,
                profile="release",
                destination="iphonesimulator",
                buildset_digest=buildset,
            )
            self.assertEqual(index["buildset_digest"], buildset)
            payload = json.loads((Path(tmp) / "kernel-dependency.json").read_text(encoding="utf-8"))
            self.assertEqual(payload["buildset_digest"], buildset)

    def test_lock_unsigned_digest_mismatch_fails(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock_path = Path(tmp) / "artifacts.lock.json"
            lock_path.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "buildset": "dd" * 32,
                        "components": {
                            "uapi": {
                                "unsigned_digest": "aa" * 32,
                                "oci_digest": "sha256:" + ("ee" * 32),
                                "oci_reference": "localhost:5001/orlix/uapi@sha256:" + ("ee" * 32),
                            }
                        },
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            lock_subjects, buildset = graph.subjects_from_lock(str(lock_path))
            self.assertEqual(buildset, "dd" * 32)
            self.assertEqual(lock_subjects["uapi"], "aa" * 32)
            with self.assertRaises(graph.BindError):
                graph.merge_subjects(lock_subjects, {"uapi": "bb" * 32})

    def test_main_binds_lock_unsigned_digests(self) -> None:
        uapi = "aa" * 32
        mlibc = "bb" * 32
        rootfs = "cc" * 32
        buildset = "dd" * 32
        toolchain = "ee" * 32
        with tempfile.TemporaryDirectory() as tmp:
            lock_path = Path(tmp) / "artifacts.lock.json"
            lock_path.write_text(
                json.dumps(
                    {
                        "schema": 1,
                        "buildset": buildset,
                        "components": {
                            "uapi": {"unsigned_digest": uapi},
                            "mlibc": {"unsigned_digest": mlibc},
                            "rootfs": {"unsigned_digest": rootfs},
                        },
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            uapi_file = Path(tmp) / "uapi.sha256"
            mlibc_file = Path(tmp) / "mlibc.sha256"
            rootfs_file = Path(tmp) / "rootfs.sha256"
            toolchain_file = Path(tmp) / "toolchain.sha256"
            uapi_file.write_text(uapi + "\n", encoding="utf-8")
            mlibc_file.write_text(mlibc + "\n", encoding="utf-8")
            rootfs_file.write_text(rootfs + "\n", encoding="utf-8")
            toolchain_file.write_text(toolchain + "\n", encoding="utf-8")
            out = Path(tmp) / "proof"
            self.assertEqual(
                graph.main(
                    [
                        "--out",
                        str(out),
                        "--lock",
                        str(lock_path),
                        "--uapi-digest",
                        str(uapi_file),
                        "--mlibc-digest",
                        str(mlibc_file),
                        "--rootfs-digest",
                        str(rootfs_file),
                        "--toolchain-digest",
                        str(toolchain_file),
                        "--profile",
                        "release",
                        "--destination",
                        "iphonesimulator",
                    ]
                ),
                0,
            )
            index = json.loads((out / "index.json").read_text(encoding="utf-8"))
            self.assertFalse(index["complete"])
            self.assertEqual(index["buildset_digest"], buildset)
            self.assertEqual(index["reports"], ["kernel-dependency:blocked"])
            payload = json.loads((out / "kernel-dependency.json").read_text(encoding="utf-8"))
            self.assertEqual(payload["subject_digest"], uapi)
            self.assertEqual(payload["buildset_digest"], buildset)
            self.assertEqual(payload["result"], "blocked")

    def test_main_passes_kernel_dependency_only_with_evidence(self) -> None:
        uapi = "aa" * 32
        toolchain = "ee" * 32
        with tempfile.TemporaryDirectory() as tmp:
            uapi_file = Path(tmp) / "uapi.sha256"
            toolchain_file = Path(tmp) / "toolchain.sha256"
            evidence = Path(tmp) / "kernel-dependency.evidence"
            uapi_file.write_text(uapi + "\n", encoding="utf-8")
            toolchain_file.write_text(toolchain + "\n", encoding="utf-8")
            evidence.write_text("T _OrlixBoot\nT _arch_boot_entry\n", encoding="utf-8")
            out = Path(tmp) / "proof"
            self.assertEqual(
                graph.main(
                    [
                        "--out",
                        str(out),
                        "--uapi-digest",
                        str(uapi_file),
                        "--toolchain-digest",
                        str(toolchain_file),
                        "--evidence",
                        f"kernel-dependency={evidence}",
                    ]
                ),
                0,
            )
            index = json.loads((out / "index.json").read_text(encoding="utf-8"))
            self.assertFalse(index["complete"])
            self.assertEqual(index["reports"], ["kernel-dependency:pass", "kunit:blocked"])
            payload = json.loads((out / "kernel-dependency.json").read_text(encoding="utf-8"))
            self.assertEqual(payload["result"], "pass")
            self.assertEqual(payload["subject_digest"], uapi)
            kunit = json.loads((out / "kunit.json").read_text(encoding="utf-8"))
            self.assertEqual(kunit["result"], "blocked")

    def test_kunit_evidence_unblocks_kselftest_as_blocked(self) -> None:
        digest = "aa" * 32
        with tempfile.TemporaryDirectory() as tmp:
            kernel = Path(tmp) / "kernel-dependency.evidence"
            kunit = Path(tmp) / "kunit.evidence"
            kernel.write_text("T _OrlixBoot\n", encoding="utf-8")
            kunit.write_text("ok 1 - orlix-tcti-atomic-memory\n", encoding="utf-8")
            index = graph.write_graph(
                tmp,
                subjects={"uapi": digest, "mlibc": digest, "rootfs": digest, "app": digest},
                toolchain_digest="bb" * 32,
                profile="release",
                destination="iphonesimulator",
                evidence={
                    "kernel-dependency": str(kernel),
                    "kunit": str(kunit),
                },
            )
            self.assertFalse(index["complete"])
            self.assertEqual(
                index["reports"],
                ["kernel-dependency:pass", "kunit:pass", "kselftest:blocked"],
            )

    def test_kselftest_evidence_unblocks_orlixmlibc_as_blocked(self) -> None:
        digest = "aa" * 32
        with tempfile.TemporaryDirectory() as tmp:
            kernel = Path(tmp) / "kernel-dependency.evidence"
            kunit = Path(tmp) / "kunit.evidence"
            kselftest = Path(tmp) / "kselftest.evidence"
            kernel.write_text("T _OrlixBoot\n", encoding="utf-8")
            kunit.write_text("ok 1 - orlix-tcti-atomic-memory\n", encoding="utf-8")
            kselftest.write_text("TAP version 13\nok 1 installed Orlix kselftest list is readable\n", encoding="utf-8")
            index = graph.write_graph(
                tmp,
                subjects={"uapi": digest, "mlibc": digest, "rootfs": digest, "app": digest},
                toolchain_digest="bb" * 32,
                profile="release",
                destination="iphonesimulator",
                evidence={
                    "kernel-dependency": str(kernel),
                    "kunit": str(kunit),
                    "kselftest": str(kselftest),
                },
            )
            self.assertFalse(index["complete"])
            self.assertEqual(
                index["reports"],
                [
                    "kernel-dependency:pass",
                    "kunit:pass",
                    "kselftest:pass",
                    "orlixmlibc:blocked",
                ],
            )

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
