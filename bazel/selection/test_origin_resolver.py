#!/usr/bin/env python3
"""Closure, fail-closed paths, and explicit output-base proofs for the resolver."""

from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

from origin_resolver import (
    AUTO,
    PROMOTED,
    SOURCE,
    OriginError,
    acquire_plan,
    bazel_flags,
    resolve,
    uapi_digests_match,
    uapi_tree_digest,
)

BUILDSET = "ab" * 32
OTHER_BUILDSET = "cd" * 32
DIGEST = "12" * 32
COMPONENTS = frozenset(
    {
        "uapi",
        "mlibc",
        "rootfs",
        "kernel-release-iphoneos",
        "kernel-release-iphonesimulator",
        "kernel-development-iphoneos",
        "kernel-development-iphonesimulator",
    }
)


def _resolve(**kwargs):
    kwargs.setdefault("use_promoted_lock", True)
    kwargs.setdefault("lock_buildset", BUILDSET)
    kwargs.setdefault("lock_components", COMPONENTS)
    return resolve(**kwargs)


class OriginClosureTest(unittest.TestCase):
    def test_source_kernel_does_not_force_uapi_mlibc_or_rootfs(self) -> None:
        vector = _resolve(
            changed_paths=["OrlixKernel/Sources/ports/orlix/overlay/drivers/foo.c"],
        )
        self.assertEqual(vector.kernel, SOURCE)
        self.assertEqual(vector.uapi, PROMOTED)
        self.assertEqual(vector.mlibc, PROMOTED)
        self.assertEqual(vector.rootfs, PROMOTED)
        self.assertNotIn("kernel-release-iphonesimulator", acquire_plan(vector, profile="release", destination="iphonesimulator")["components"])

    def test_source_uapi_forces_kernel_mlibc_and_rootfs(self) -> None:
        vector = _resolve(
            origins={
                "kernel": PROMOTED,
                "uapi": SOURCE,
                "mlibc": AUTO,
                "rootfs": PROMOTED,
            },
            facts={
                "kernel_changed": False,
                "uapi_changed": True,
                "mlibc_changed": False,
                "rootfs_changed": False,
            },
        )
        self.assertEqual(vector.as_dict(), {name: SOURCE for name in ("kernel", "uapi", "mlibc", "rootfs")})

    def test_source_mlibc_forces_source_rootfs_only(self) -> None:
        vector = _resolve(
            changed_paths=["OrlixMLibC/Sources/string/memcpy.c"],
        )
        self.assertEqual(vector.mlibc, SOURCE)
        self.assertEqual(vector.rootfs, SOURCE)
        self.assertEqual(vector.kernel, PROMOTED)
        self.assertEqual(vector.uapi, PROMOTED)

    def test_promoted_mlibc_against_matching_source_uapi_stays_promoted(self) -> None:
        vector = _resolve(
            origins={
                "kernel": PROMOTED,
                "uapi": SOURCE,
                "mlibc": PROMOTED,
                "rootfs": PROMOTED,
            },
            selected_uapi_digest=DIGEST,
            mlibc_consumed_uapi_digest=DIGEST,
        )
        self.assertEqual(vector.uapi, SOURCE)
        self.assertEqual(vector.kernel, SOURCE)
        self.assertEqual(vector.rootfs, SOURCE)
        self.assertEqual(vector.mlibc, PROMOTED)

    def test_promoted_mlibc_against_source_uapi_fails_closed_without_match(self) -> None:
        with self.assertRaises(OriginError):
            _resolve(
                origins={
                    "kernel": SOURCE,
                    "uapi": SOURCE,
                    "mlibc": PROMOTED,
                    "rootfs": SOURCE,
                },
            )
        with self.assertRaises(OriginError):
            _resolve(
                origins={
                    "kernel": SOURCE,
                    "uapi": SOURCE,
                    "mlibc": PROMOTED,
                    "rootfs": SOURCE,
                },
                selected_uapi_digest=DIGEST,
                mlibc_consumed_uapi_digest="34" * 32,
            )

    def test_package_or_rootfs_policy_forces_rootfs_not_mlibc(self) -> None:
        package = _resolve(changed_paths=["OrlixCoreUtils/Sources/echo.c"])
        policy = _resolve(changed_paths=["OrlixOS/Sources/distribution/images.mk"])
        for vector in (package, policy):
            self.assertEqual(vector.rootfs, SOURCE)
            self.assertEqual(vector.mlibc, PROMOTED)
            self.assertEqual(vector.kernel, PROMOTED)
            self.assertEqual(vector.uapi, PROMOTED)

    def test_mixed_promoted_buildsets_fail(self) -> None:
        with self.assertRaises(OriginError):
            _resolve(
                requested_mode=PROMOTED,
                promoted_buildsets={"uapi": OTHER_BUILDSET},
            )

    def test_unmatched_promoted_kernel_fails_analysis(self) -> None:
        with self.assertRaises(OriginError) as caught:
            _resolve(
                component_mode=PROMOTED,
                profile="development",
                destination="iphoneos",
                lock_components=frozenset({"uapi", "mlibc", "rootfs", "kernel-release-iphonesimulator"}),
            )
        self.assertIn("unmatched promoted kernel", str(caught.exception))

    def test_unknown_path_fails_closed(self) -> None:
        with self.assertRaises(OriginError) as caught:
            _resolve(changed_paths=["mystery/not-a-product-file.c"])
        self.assertIn("unknown path", str(caught.exception))

    def test_lock_record_change_is_not_a_source_change(self) -> None:
        vector = _resolve(changed_paths=["artifacts.lock.json"])
        self.assertEqual(vector.as_dict(), {name: PROMOTED for name in ("kernel", "uapi", "mlibc", "rootfs")})
        self.assertEqual(vector.buildset, BUILDSET)

    def test_component_mode_source_stays_all_source(self) -> None:
        vector = resolve(
            component_mode=SOURCE,
            origins={name: PROMOTED for name in ("kernel", "uapi", "mlibc", "rootfs")},
            use_promoted_lock=True,
            lock_buildset=BUILDSET,
            lock_components=COMPONENTS,
        )
        self.assertEqual(vector.as_dict(), {name: SOURCE for name in ("kernel", "uapi", "mlibc", "rootfs")})
        plan = acquire_plan(vector, profile="release", destination="iphonesimulator")
        self.assertEqual(plan["components"], [])
        self.assertFalse(plan["delete_reconstruct_root"])
        self.assertFalse(plan["delete_siblings"])

    def test_acquire_lists_only_promoted_members_and_does_not_delete(self) -> None:
        vector = _resolve(changed_paths=["OrlixKernel/Sources/ports/orlix/kbuild/kernel-rules.mk"])
        plan = acquire_plan(vector, profile="release", destination="iphonesimulator")
        self.assertEqual(plan["components"], ["uapi", "mlibc", "rootfs"])
        self.assertFalse(plan["delete_reconstruct_root"])
        self.assertFalse(plan["delete_siblings"])

    def test_auto_flags_do_not_set_component_mode_auto(self) -> None:
        vector = _resolve(changed_paths=["docs/index.md"])
        flags = " ".join(bazel_flags(vector))
        self.assertNotIn("component_mode=auto", flags)
        self.assertNotIn("DEVELOPER_DIR", flags)
        self.assertIn("--//bazel/config:origin_kernel=promoted", flags)
        self.assertIn("--//bazel/config:use_promoted_lock=true", flags)


class ExplicitOutputBaseTest(unittest.TestCase):
    def test_uapi_equality_uses_explicit_output_base(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            left = root / "left-output-base"
            right = root / "right-output-base"
            for base in (left, right):
                header = base / "headers" / "include" / "linux"
                header.mkdir(parents=True)
                (header / "unistd.h").write_text("same\n", encoding="utf-8")
            self.assertTrue(uapi_digests_match(left, right, "headers"))
            (right / "headers" / "include" / "linux" / "unistd.h").write_text("changed\n", encoding="utf-8")
            self.assertFalse(uapi_digests_match(left, right, "headers"))

    def test_uapi_byte_change_forces_source_closure(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source-output-base"
            promoted = root / "promoted-output-base"
            for base, text in ((source, "new\n"), (promoted, "old\n")):
                header = base / "headers" / "include" / "linux"
                header.mkdir(parents=True)
                (header / "unistd.h").write_text(text, encoding="utf-8")
            vector = _resolve(
                changed_paths=["include/uapi/linux/unistd.h"],
                source_output_base=source,
                promoted_output_base=promoted,
            )
            self.assertEqual(vector.as_dict(), {name: SOURCE for name in ("kernel", "uapi", "mlibc", "rootfs")})

    def test_matching_uapi_bytes_do_not_force_source(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "source-output-base"
            promoted = root / "promoted-output-base"
            for base in (source, promoted):
                header = base / "headers" / "include" / "linux"
                header.mkdir(parents=True)
                (header / "unistd.h").write_text("same\n", encoding="utf-8")
            vector = _resolve(
                changed_paths=["arch/arm64/include/uapi/asm/unistd.h"],
                source_output_base=source,
                promoted_output_base=promoted,
            )
            self.assertEqual(vector.uapi, PROMOTED)
            self.assertEqual(vector.kernel, PROMOTED)
            self.assertEqual(vector.mlibc, PROMOTED)
            self.assertEqual(vector.rootfs, PROMOTED)

    def test_uapi_probe_without_output_base_fails_closed(self) -> None:
        with self.assertRaises(OriginError) as caught:
            _resolve(changed_paths=["bazel/feasibility/kernel/kernel_uapi.bzl"])
        self.assertIn("explicit output base", str(caught.exception))

    def test_uapi_equality_refuses_bazel_bin(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            output_base = root / "output-base"
            headers = output_base / "headers"
            headers.mkdir(parents=True)
            (headers / "unistd.h").write_text("bytes\n", encoding="utf-8")
            bazel_bin = root / "bazel-bin"
            bazel_bin.symlink_to(output_base, target_is_directory=True)
            with self.assertRaises(OriginError) as linked:
                uapi_tree_digest(bazel_bin, "headers")
            self.assertIn("bazel-bin", str(linked.exception))
            with self.assertRaises(OriginError) as nested:
                uapi_tree_digest(output_base, "bazel-bin/headers")
            self.assertIn("bazel-bin", str(nested.exception))
            digest = uapi_tree_digest(output_base, "headers")
            self.assertEqual(len(digest), 64)


if __name__ == "__main__":
    unittest.main()
