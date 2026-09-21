from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from auto_preflight import _read_digest, run_preflight
from worktree_classifier import ClassifierError, Classification


class AutoPreflightTests(unittest.TestCase):
    def test_forcing_modes_skip_classifier(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock = Path(tmp) / "artifacts.lock.json"
            lock.write_text(json.dumps({"schema": 2, "buildset": "aa" * 32, "components": {"uapi": {}}}) + "\n")
            # invalid lock components; source forcing must not need lock components for all-source
        body = run_preflight(Path("."), Path("artifacts.lock.json"), requested_mode="source", profile="release", destination="iphonesimulator")
        self.assertEqual(body["origins"]["kernel"], "source")
        self.assertNotIn("classification", body)

    def test_equal_digest_keeps_userspace_promoted(self) -> None:
        classification = Classification(
            facts={"kernel_changed": True, "uapi_changed": False, "mlibc_changed": False, "rootfs_changed": False},
            probe_uapi=True,
            classes=("uapi_sensitive",),
            paths=("include/uapi/linux/unistd.h",),
        )
        with mock.patch("auto_preflight._lock_source_sha", return_value="ab" * 20), \
             mock.patch("auto_preflight.classify", return_value=classification), \
             mock.patch("auto_preflight._promoted_uapi_semantic_digest", return_value="11" * 32), \
             mock.patch("auto_preflight.probe_source_uapi", return_value="11" * 32), \
             mock.patch("auto_preflight.resolve") as resolver:
            from origin_resolver import OriginVector
            resolver.return_value = OriginVector("auto", "source", "promoted", "promoted", "promoted", "ff" * 32)
            body = run_preflight(Path("."), Path("artifacts.lock.json"), requested_mode="auto", profile="release", destination="iphonesimulator")
        facts = resolver.call_args.kwargs["facts"]
        self.assertFalse(facts["uapi_changed"])
        self.assertEqual(body["origins"]["uapi"], "promoted")

    def test_different_digest_closes_userspace(self) -> None:
        classification = Classification(
            facts={"kernel_changed": True, "uapi_changed": False, "mlibc_changed": False, "rootfs_changed": False},
            probe_uapi=True,
            classes=("uapi_sensitive",),
            paths=("include/uapi/linux/unistd.h",),
        )
        with mock.patch("auto_preflight._lock_source_sha", return_value="ab" * 20), \
             mock.patch("auto_preflight.classify", return_value=classification), \
             mock.patch("auto_preflight._promoted_uapi_semantic_digest", return_value="11" * 32), \
             mock.patch("auto_preflight.probe_source_uapi", return_value="22" * 32):
            body = run_preflight(Path("."), Path("artifacts.lock.json"), requested_mode="auto", profile="release", destination="iphonesimulator")
        self.assertEqual(body["origins"]["uapi"], "source")
        self.assertEqual(body["origins"]["mlibc"], "source")

    def test_missing_promoted_digest_fails_closed(self) -> None:
        classification = Classification(
            facts={"kernel_changed": True, "uapi_changed": False, "mlibc_changed": False, "rootfs_changed": False},
            probe_uapi=True,
            classes=("uapi_sensitive",),
            paths=("include/uapi/linux/unistd.h",),
        )
        with mock.patch("auto_preflight._lock_source_sha", return_value="ab" * 20), \
             mock.patch("auto_preflight.classify", return_value=classification), \
             mock.patch("auto_preflight._promoted_uapi_semantic_digest", side_effect=ClassifierError("missing promoted UAPI semantic digest")):
            with self.assertRaises(ClassifierError):
                run_preflight(Path("."), Path("artifacts.lock.json"), requested_mode="auto", profile="release", destination="iphonesimulator")

    def test_probe_failure_fails_closed(self) -> None:
        classification = Classification(
            facts={"kernel_changed": True, "uapi_changed": False, "mlibc_changed": False, "rootfs_changed": False},
            probe_uapi=True,
            classes=("uapi_sensitive",),
            paths=("include/uapi/linux/unistd.h",),
        )
        with mock.patch("auto_preflight._lock_source_sha", return_value="ab" * 20), \
             mock.patch("auto_preflight.classify", return_value=classification), \
             mock.patch("auto_preflight._promoted_uapi_semantic_digest", return_value="11" * 32), \
             mock.patch("auto_preflight.probe_source_uapi", side_effect=ClassifierError("UAPI probe build failure")):
            with self.assertRaises(ClassifierError):
                run_preflight(Path("."), Path("artifacts.lock.json"), requested_mode="auto", profile="release", destination="iphonesimulator")

    def test_app_only_does_not_probe(self) -> None:
        classification = Classification(
            facts={"kernel_changed": False, "uapi_changed": False, "mlibc_changed": False, "rootfs_changed": False},
            probe_uapi=False,
            classes=("app",),
            paths=("Orlix/App.swift",),
        )
        with mock.patch("auto_preflight._lock_source_sha", return_value="ab" * 20), \
             mock.patch("auto_preflight.classify", return_value=classification), \
             mock.patch("auto_preflight.probe_source_uapi") as probe, \
             mock.patch("auto_preflight._promoted_uapi_semantic_digest") as promoted:
            body = run_preflight(Path("."), Path("artifacts.lock.json"), requested_mode="auto", profile="release", destination="iphonesimulator")
        probe.assert_not_called()
        promoted.assert_not_called()
        self.assertEqual(body["origins"]["kernel"], "promoted")
        self.assertFalse(body["classification"]["probe_uapi"])

    def test_digest_parse_failure(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "uapi.sha256"
            path.write_text("not-a-digest\n", encoding="utf-8")
            with self.assertRaises(ClassifierError):
                _read_digest(path)


if __name__ == "__main__":
    unittest.main()
