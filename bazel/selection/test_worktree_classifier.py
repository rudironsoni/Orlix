from __future__ import annotations

import json
import os
import subprocess
import tempfile
import time
import unittest
from pathlib import Path

from origin_resolver import resolve
from worktree_classifier import ClassifierError, classify, classify_path, collect_paths


def _git(repo: Path, *args: str, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(repo), *args],
        check=check,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def _repo() -> Path:
    root = Path(tempfile.mkdtemp(prefix="orlix-classifier-"))
    _git(root, "init")
    _git(root, "config", "user.email", "orlix@example.test")
    _git(root, "config", "user.name", "Orlix")
    (root / "Orlix").mkdir()
    (root / "Orlix/App.swift").write_text("app\n", encoding="utf-8")
    _git(root, "add", "Orlix/App.swift")
    _git(root, "commit", "-m", "base")
    sha = _git(root, "rev-parse", "HEAD").stdout.strip()
    return root, sha


class PathClassTests(unittest.TestCase):
    def test_ownership(self) -> None:
        self.assertEqual(classify_path("Orlix/Sources/App.swift"), "app")
        self.assertEqual(classify_path("OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/idle.c"), "kernel_impl")
        self.assertEqual(classify_path("include/uapi/linux/unistd.h"), "uapi_sensitive")
        self.assertEqual(classify_path("bazel/feasibility/kernel/kernel_uapi.bzl"), "uapi_sensitive")
        self.assertEqual(classify_path("OrlixMLibC/Sources/string/memcpy.c"), "mlibc")
        self.assertEqual(classify_path("OrlixCoreUtils/Sources/true.c"), "rootfs")
        self.assertEqual(classify_path("bazel/config/BUILD.bazel"), "toolchain")
        self.assertEqual(classify_path("bazel/feasibility/kernel/kernel_macho.bzl"), "toolchain")
        self.assertEqual(classify_path("bazel/feasibility/kernel/kernel_uapi.bzl"), "uapi_sensitive")
        self.assertEqual(classify_path("bazel/feasibility/mlibc/mlibc_sysroot.bzl"), "mlibc")
        self.assertEqual(classify_path("Build/Bazel/output-base/x"), "disposable")
        self.assertEqual(
            classify_path("OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/include/asm/unistd.h"),
            "kernel_impl",
        )

    def test_unknown_path_fails_closed(self) -> None:
        with self.assertRaises(ClassifierError):
            classify_path("mystery/new.c")


class WorktreeClassifierTests(unittest.TestCase):
    def tearDown(self) -> None:
        pass

    def test_clean_baseline_is_app_empty(self) -> None:
        repo, sha = _repo()
        result = classify(repo, sha)
        self.assertEqual(result.facts["kernel_changed"], False)
        self.assertFalse(result.probe_uapi)
        self.assertEqual(result.classes, ())

    def test_committed_app_delta(self) -> None:
        repo, sha = _repo()
        (repo / "Orlix/App.swift").write_text("app2\n", encoding="utf-8")
        _git(repo, "add", "Orlix/App.swift")
        _git(repo, "commit", "-m", "app")
        result = classify(repo, sha)
        self.assertEqual(result.classes, ("app",))
        self.assertFalse(result.probe_uapi)

    def test_staged_unstaged_untracked(self) -> None:
        repo, sha = _repo()
        (repo / "Orlix/Staged.swift").write_text("s\n", encoding="utf-8")
        _git(repo, "add", "Orlix/Staged.swift")
        (repo / "Orlix/App.swift").write_text("dirty\n", encoding="utf-8")
        (repo / "Orlix/Untracked.swift").write_text("u\n", encoding="utf-8")
        result = classify(repo, sha)
        self.assertEqual(result.classes, ("app",))
        self.assertIn("Orlix/Staged.swift", result.paths)
        self.assertIn("Orlix/App.swift", result.paths)
        self.assertIn("Orlix/Untracked.swift", result.paths)

    def test_kernel_impl_does_not_probe(self) -> None:
        repo, sha = _repo()
        path = repo / "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/idle.c"
        path.parent.mkdir(parents=True)
        path.write_text("int idle = 1;\n", encoding="utf-8")
        result = classify(repo, sha)
        self.assertTrue(result.facts["kernel_changed"])
        self.assertFalse(result.facts["uapi_changed"])
        self.assertFalse(result.probe_uapi)
        lock = repo / "artifacts.lock.json"
        lock.write_text(json.dumps({"schema": 2, "buildset": "aa" * 32, "components": {}}) + "\n", encoding="utf-8")
        vector = resolve(requested_mode="auto", facts=result.facts, lock_path=lock)
        self.assertEqual(vector.kernel, "source")
        self.assertEqual(vector.uapi, "promoted")

    def test_uapi_sensitive_requests_probe(self) -> None:
        repo, sha = _repo()
        path = repo / "include/uapi/linux/unistd.h"
        path.parent.mkdir(parents=True)
        path.write_text("uapi\n", encoding="utf-8")
        result = classify(repo, sha)
        self.assertTrue(result.probe_uapi)
        self.assertTrue(result.facts["kernel_changed"])

    def test_mlibc_change(self) -> None:
        repo, sha = _repo()
        path = repo / "OrlixMLibC/Sources/string/memcpy.c"
        path.parent.mkdir(parents=True)
        path.write_text("memcpy\n", encoding="utf-8")
        result = classify(repo, sha)
        lock = repo / "artifacts.lock.json"
        lock.write_text(json.dumps({"schema": 2, "buildset": "aa" * 32, "components": {}}) + "\n", encoding="utf-8")
        vector = resolve(requested_mode="auto", facts=result.facts, lock_path=lock)
        self.assertEqual(vector.mlibc, "source")
        self.assertEqual(vector.rootfs, "source")
        self.assertEqual(vector.kernel, "promoted")

    def test_delete_and_rename(self) -> None:
        repo, sha = _repo()
        gone = repo / "Orlix/Gone.swift"
        gone.write_text("g\n", encoding="utf-8")
        _git(repo, "add", "Orlix/Gone.swift")
        _git(repo, "commit", "-m", "gone")
        sha2 = _git(repo, "rev-parse", "HEAD").stdout.strip()
        _git(repo, "rm", "Orlix/Gone.swift")
        result = classify(repo, sha2)
        self.assertIn("Orlix/Gone.swift", result.paths)
        src = repo / "Orlix/App.swift"
        dest = repo / "OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/kernel/moved.c"
        dest.parent.mkdir(parents=True)
        _git(repo, "mv", "Orlix/App.swift", str(dest.relative_to(repo)))
        renamed = classify(repo, sha2)
        self.assertIn("app", renamed.classes)
        self.assertIn("kernel_impl", renamed.classes)

    def test_missing_source_sha_and_unavailable_commit(self) -> None:
        repo, sha = _repo()
        with self.assertRaises(ClassifierError):
            classify(repo, "0" * 40)
        with self.assertRaises(ClassifierError):
            collect_paths(repo, "short")

    def test_git_failure(self) -> None:
        with self.assertRaises(ClassifierError):
            classify(Path("/tmp"), "ab" * 20)

    def test_lock_record_does_not_force_source(self) -> None:
        repo, sha = _repo()
        lock = repo / "artifacts.lock.json"
        lock.write_text("{}\n", encoding="utf-8")
        _git(repo, "add", "artifacts.lock.json")
        _git(repo, "commit", "-m", "lock")
        proved = _git(repo, "rev-parse", "HEAD").stdout.strip()
        lock.write_text(json.dumps({"schema": 2, "buildset": "aa" * 32, "components": {}}) + "\n", encoding="utf-8")
        _git(repo, "add", "artifacts.lock.json")
        _git(repo, "commit", "-m", "apply lock")
        result = classify(repo, proved)
        self.assertEqual(result.classes, ())
        self.assertFalse(any(result.facts.values()))
        vector = resolve(requested_mode="auto", facts=result.facts, lock_path=lock)
        self.assertEqual(vector.kernel, "promoted")
        self.assertEqual(vector.uapi, "promoted")
        self.assertEqual(vector.mlibc, "promoted")
        self.assertEqual(vector.rootfs, "promoted")
        self.assertEqual(vector.buildset, "aa" * 32)

    def test_disposable_path_is_ignored(self) -> None:
        repo, sha = _repo()
        junk = repo / "Build/Bazel/proof/noise.txt"
        junk.parent.mkdir(parents=True)
        junk.write_text("noise\n", encoding="utf-8")
        result = classify(repo, sha)
        self.assertEqual(result.classes, ())
        self.assertFalse(result.probe_uapi)


class ClassifierTimingTests(unittest.TestCase):
    def test_no_probe_classifier_is_under_one_second(self) -> None:
        repo, sha = _repo()
        (repo / "Orlix/App.swift").write_text("timed\n", encoding="utf-8")
        started = time.perf_counter()
        classify(repo, sha)
        elapsed = time.perf_counter() - started
        self.assertLess(elapsed, 1.0)


if __name__ == "__main__":
    unittest.main()
