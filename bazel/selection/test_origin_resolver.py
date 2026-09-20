#!/usr/bin/env python3
from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from origin_resolver import (
    AUTO,
    OriginError,
    PROMOTED,
    SOURCE,
    acquire_components,
    resolve,
)


def _lock(path: Path, buildset: str = "aa" * 32) -> Path:
    path.write_text(
        json.dumps({"schema": 2, "buildset": buildset, "components": {"uapi": {}}}) + "\n",
        encoding="utf-8",
    )
    return path


class OriginResolverTests(unittest.TestCase):
    def test_source_forcing_resolves_all_source(self) -> None:
        vector = resolve(requested_mode=SOURCE)
        self.assertEqual(
            {"kernel": SOURCE, "uapi": SOURCE, "mlibc": SOURCE, "rootfs": SOURCE},
            vector.as_dict(),
        )
        self.assertEqual([], acquire_components(vector, profile="release", destination="iphonesimulator"))

    def test_promoted_forcing_uses_one_lock(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock = _lock(Path(tmp) / "artifacts.lock.json")
            vector = resolve(requested_mode=PROMOTED, lock_path=lock)
        self.assertEqual(
            {"kernel": PROMOTED, "uapi": PROMOTED, "mlibc": PROMOTED, "rootfs": PROMOTED},
            vector.as_dict(),
        )
        self.assertEqual("aa" * 32, vector.buildset)
        self.assertEqual(
            ["uapi", "mlibc", "rootfs", "kernel-release-iphonesimulator"],
            acquire_components(vector, profile="release", destination="iphonesimulator"),
        )

    def test_kernel_source_rest_promoted(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock = _lock(Path(tmp) / "artifacts.lock.json")
            vector = resolve(
                requested_mode=AUTO,
                facts={
                    "kernel_changed": True,
                    "uapi_changed": False,
                    "mlibc_changed": False,
                    "rootfs_changed": False,
                },
                lock_path=lock,
            )
        self.assertEqual(
            {"kernel": SOURCE, "uapi": PROMOTED, "mlibc": PROMOTED, "rootfs": PROMOTED},
            vector.as_dict(),
        )
        self.assertEqual(
            ["uapi", "mlibc", "rootfs"],
            acquire_components(vector, profile="release", destination="iphonesimulator"),
        )

    def test_uapi_source_closes_downstream_and_kernel(self) -> None:
        vector = resolve(
            requested_mode=AUTO,
            facts={
                "kernel_changed": False,
                "uapi_changed": True,
                "mlibc_changed": False,
                "rootfs_changed": False,
            },
        )
        self.assertEqual(
            {"kernel": SOURCE, "uapi": SOURCE, "mlibc": SOURCE, "rootfs": SOURCE},
            vector.as_dict(),
        )

    def test_mlibc_source_closes_rootfs(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock = _lock(Path(tmp) / "artifacts.lock.json")
            vector = resolve(
                requested_mode=AUTO,
                facts={
                    "kernel_changed": False,
                    "uapi_changed": False,
                    "mlibc_changed": True,
                    "rootfs_changed": False,
                },
                lock_path=lock,
            )
        self.assertEqual(
            {"kernel": PROMOTED, "uapi": PROMOTED, "mlibc": SOURCE, "rootfs": SOURCE},
            vector.as_dict(),
        )
        names = acquire_components(vector, profile="release", destination="iphonesimulator")
        self.assertEqual(["uapi", "kernel-release-iphonesimulator"], names)
        self.assertNotIn("mlibc", names)
        self.assertNotIn("rootfs", names)

    def test_invalid_promoted_mlibc_against_mismatched_uapi(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock = _lock(Path(tmp) / "artifacts.lock.json")
            with self.assertRaises(OriginError) as raised:
                resolve(
                    requested_mode=AUTO,
                    vector={
                        "kernel": SOURCE,
                        "uapi": SOURCE,
                        "mlibc": PROMOTED,
                        "rootfs": SOURCE,
                    },
                    lock_path=lock,
                    selected_uapi_digest="11" * 32,
                    mlibc_consumed_uapi_digest="22" * 32,
                )
        self.assertIn("consumed_uapi_digest", str(raised.exception))

    def test_same_lock_enforced(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock = _lock(Path(tmp) / "artifacts.lock.json", "bb" * 32)
            with self.assertRaises(OriginError) as raised:
                resolve(
                    requested_mode=PROMOTED,
                    lock_path=lock,
                    promoted_buildsets=["cc" * 32],
                )
        self.assertIn("same locked buildset", str(raised.exception))

    def test_unused_kernel_variants_not_acquired(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock = _lock(Path(tmp) / "artifacts.lock.json")
            names = acquire_components(
                resolve(requested_mode=PROMOTED, lock_path=lock),
                profile="release",
                destination="iphonesimulator",
            )
        self.assertEqual(
            ["uapi", "mlibc", "rootfs", "kernel-release-iphonesimulator"],
            names,
        )
        self.assertNotIn("kernel-release-iphoneos", names)
        self.assertNotIn("kernel-development-iphoneos", names)
        self.assertNotIn("kernel-development-iphonesimulator", names)

    def test_hybrid_kernel_source_does_not_acquire_kernel(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock = _lock(Path(tmp) / "artifacts.lock.json")
            names = acquire_components(
                resolve(
                    requested_mode=AUTO,
                    vector={
                        "kernel": SOURCE,
                        "uapi": PROMOTED,
                        "mlibc": PROMOTED,
                        "rootfs": PROMOTED,
                    },
                    lock_path=lock,
                ),
                profile="release",
                destination="iphonesimulator",
            )
        self.assertEqual(["uapi", "mlibc", "rootfs"], names)

    def test_auto_without_facts_stays_coarse_and_closed(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock = _lock(Path(tmp) / "artifacts.lock.json")
            vector = resolve(
                requested_mode=AUTO,
                changed_paths=["Orlix/Sources/App.swift"],
                lock_schema=2,
                lock_path=lock,
            )
        self.assertEqual(
            {"kernel": PROMOTED, "uapi": PROMOTED, "mlibc": PROMOTED, "rootfs": PROMOTED},
            vector.as_dict(),
        )
        closed = resolve(requested_mode=AUTO, changed_paths=["OrlixKernel/Sources/x.c"], lock_schema=2)
        self.assertEqual(
            {"kernel": SOURCE, "uapi": SOURCE, "mlibc": SOURCE, "rootfs": SOURCE},
            closed.as_dict(),
        )
        unknown = resolve(requested_mode=AUTO, changed_paths=[], lock_schema=2)
        self.assertEqual(SOURCE, unknown.kernel)

    def test_incomplete_facts_fail_closed(self) -> None:
        with self.assertRaises(OriginError):
            resolve(requested_mode=AUTO, facts={"kernel_changed": True})

    def test_does_not_rewrite_invalid_vector(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock = _lock(Path(tmp) / "artifacts.lock.json")
            with self.assertRaises(OriginError) as raised:
                resolve(
                    requested_mode=AUTO,
                    vector={
                        "kernel": SOURCE,
                        "uapi": SOURCE,
                        "mlibc": PROMOTED,
                        "rootfs": SOURCE,
                    },
                    lock_path=lock,
                    selected_uapi_digest="11" * 32,
                    mlibc_consumed_uapi_digest="22" * 32,
                )
        self.assertIn("consumed_uapi_digest", str(raised.exception))

    def test_matching_uapi_digest_allows_promoted_mlibc(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock = _lock(Path(tmp) / "artifacts.lock.json")
            vector = resolve(
                requested_mode=AUTO,
                vector={
                    "kernel": SOURCE,
                    "uapi": SOURCE,
                    "mlibc": PROMOTED,
                    "rootfs": SOURCE,
                },
                lock_path=lock,
                selected_uapi_digest="11" * 32,
                mlibc_consumed_uapi_digest="11" * 32,
            )
        self.assertEqual(PROMOTED, vector.mlibc)
        self.assertEqual(SOURCE, vector.uapi)


if __name__ == "__main__":
    unittest.main()
