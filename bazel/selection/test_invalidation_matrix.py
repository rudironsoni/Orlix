#!/usr/bin/env python3
"""Permanent invalidation contract. Expectations live in invalidation-matrix.json."""

from __future__ import annotations

import json
import subprocess
import tempfile
import unittest
from pathlib import Path

from origin_resolver import resolve
from worktree_classifier import ClassifierError, classify, classify_path

ROOT = Path(__file__).resolve().parent
MATRIX = json.loads((ROOT / "invalidation-matrix.json").read_text(encoding="utf-8"))
SPECIMENS = json.loads((ROOT / "invalidation_specimens.json").read_text(encoding="utf-8"))


def _git(repo: Path, *args: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", "-C", str(repo), *args],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def _lock(path: Path) -> Path:
    path.write_text(
        json.dumps({"schema": 2, "buildset": "aa" * 32, "components": {}}) + "\n",
        encoding="utf-8",
    )
    return path


def _base_repo() -> tuple[Path, str]:
    root = Path(tempfile.mkdtemp(prefix="orlix-invalidation-"))
    _git(root, "init")
    _git(root, "config", "user.email", "orlix@example.test")
    _git(root, "config", "user.name", "Orlix")
    (root / "Orlix").mkdir()
    (root / "Orlix/App.swift").write_text("app\n", encoding="utf-8")
    _git(root, "add", "Orlix/App.swift")
    _git(root, "commit", "-m", "base")
    sha = _git(root, "rev-parse", "HEAD").stdout.strip()
    return root, sha


def _place(repo: Path, rel: str, text: str = "x\n") -> Path:
    path = repo / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


def _apply_form(repo: Path, sha: str, rel: str, form: str) -> None:
    if form == "untracked":
        _place(repo, rel)
        return
    if form == "unstaged":
        if rel == "Orlix/App.swift" or rel.startswith("Orlix/"):
            path = repo / rel
            if not path.exists():
                _place(repo, rel)
                _git(repo, "add", rel)
                _git(repo, "commit", "-m", "add")
            path.write_text("dirty\n", encoding="utf-8")
            return
        _place(repo, rel)
        _git(repo, "add", rel)
        _git(repo, "commit", "-m", "add")
        (repo / rel).write_text("dirty\n", encoding="utf-8")
        return
    if form == "staged":
        _place(repo, rel)
        _git(repo, "add", rel)
        return
    if form == "committed":
        _place(repo, rel)
        _git(repo, "add", rel)
        _git(repo, "commit", "-m", "delta")
        return
    if form == "delete":
        gone = _place(repo, rel, "gone\n")
        _git(repo, "add", rel)
        _git(repo, "commit", "-m", "add")
        _git(repo, "rm", rel)
        return
    raise AssertionError(f"unknown git form {form}")


class MatrixOwnershipTests(unittest.TestCase):
    def test_matrix_owns_required_classes(self) -> None:
        ids = {case["id"] for case in MATRIX["cases"]}
        for required in (
            "A-app-only",
            "B-kernel-impl",
            "C-uapi-equal",
            "D-uapi-changed",
            "E-mlibc",
            "F-package",
            "G-rootfs-assembly",
            "H-equivalent-origin",
            "I-disposable",
            "J-toolchain",
            "J-unknown",
            "cache-eviction",
        ):
            self.assertIn(required, ids)
        self.assertFalse(MATRIX["cache_eviction_is_invalidation"])


class OriginContractTests(unittest.TestCase):
    def test_path_cases_resolve(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            lock = _lock(Path(tmp) / "artifacts.lock.json")
            for case in MATRIX["cases"]:
                if case.get("fail_closed"):
                    with self.assertRaises(ClassifierError):
                        classify_path(case["paths"][0])
                    continue
                if "paths" not in case or "expected_origins" not in case:
                    continue
                if case.get("facts_after_probe") is not None:
                    vector = resolve(
                        requested_mode="auto",
                        facts=case["facts_after_probe"],
                        lock_path=lock,
                    )
                    self.assertEqual(case["expected_origins"], vector.as_dict(), case["id"])
                    continue
                kinds = [classify_path(path) for path in case["paths"]]
                if case["id"] == "I-disposable":
                    self.assertEqual(kinds, ["disposable"])
                facts = {
                    "kernel_changed": any(kind in ("kernel_impl", "uapi_sensitive") for kind in kinds),
                    "uapi_changed": False,
                    "mlibc_changed": "mlibc" in kinds,
                    "rootfs_changed": "rootfs" in kinds,
                }
                if "toolchain" in kinds:
                    facts = {
                        "kernel_changed": True,
                        "uapi_changed": True,
                        "mlibc_changed": True,
                        "rootfs_changed": True,
                    }
                probe = "uapi_sensitive" in kinds
                self.assertEqual(case.get("probe_uapi"), probe, case["id"])
                vector = resolve(requested_mode="auto", facts=facts, lock_path=lock)
                self.assertEqual(case["expected_origins"], vector.as_dict(), case["id"])

    def test_git_forms_do_not_change_origins(self) -> None:
        for case in MATRIX["cases"]:
            forms = case.get("git_forms") or []
            if not forms or "paths" not in case:
                continue
            rel = case["paths"][0]
            for form in forms:
                repo, sha = _base_repo()
                _apply_form(repo, sha, rel, form)
                result = classify(repo, sha)
                if form != "delete":
                    self.assertIn(rel, result.paths, f"{case['id']} {form}")
                self.assertEqual(result.probe_uapi, case.get("probe_uapi", False), f"{case['id']} {form}")

    def test_rename_across_ownership(self) -> None:
        case = next(item for item in MATRIX["cases"] if item["id"] == "rename-across")
        repo, sha = _base_repo()
        dest = repo / case["rename"]["to"]
        dest.parent.mkdir(parents=True, exist_ok=True)
        _git(repo, "mv", "Orlix/App.swift", case["rename"]["to"])
        result = classify(repo, sha)
        self.assertIn("app", result.classes)
        self.assertIn("kernel_impl", result.classes)
        self.assertFalse(result.probe_uapi)


class ProductBoundaryTests(unittest.TestCase):
    def test_app_shell_is_not_an_implementation_input(self) -> None:
        repo = ROOT.parents[1]
        build = (repo / "Orlix" / "BUILD.bazel").read_text(encoding="utf-8")
        implementation = build.split("_ORLIX_IMPLEMENTATION_SRCS", 1)[1].split("_ORLIX_APP_SRCS", 1)[0]
        app = build.split("_ORLIX_APP_SRCS", 1)[1].split("swift_library(", 1)[0]
        self.assertIn("Orlix/Core/**/*.swift", implementation)
        self.assertIn("Orlix/Features/**/*.swift", implementation)
        self.assertNotIn("Orlix/App/**/*.swift", implementation)
        self.assertIn("Orlix/App/Localization/AppLanguage.swift", implementation)
        self.assertIn("Orlix/App/**/*.swift", app)
        self.assertNotIn("Orlix/Core/**/*.swift", app)
        self.assertIn("Orlix/App/Localization/AppLanguage.swift", app)
        shell = repo / "Orlix/Orlix/App/ContentView.swift"
        self.assertTrue(shell.is_file())
        self.assertIn("import OrlixImplementation", shell.read_text(encoding="utf-8"))
        boundary = (repo / "bazel/selection/boundary.bzl").read_text(encoding="utf-8")
        self.assertNotIn("abi_manifest = origin.abi_manifest", boundary)
        self.assertNotIn("file_manifest = origin.file_manifest", boundary)
        self.assertNotIn("payload_metadata = origin.payload_metadata", boundary)
        self.assertNotIn("source_input_digest = origin.source_input_digest", boundary)
        self.assertIn('abi_manifest = abi', boundary)
        self.assertIn('file_manifest = file_manifest', boundary)


class SpecimenTests(unittest.TestCase):
    def test_equivalent_origin_action_keys(self) -> None:
        eq = SPECIMENS["equivalent_origin"]
        self.assertEqual(
            eq["OrlixAppLibrary.SwiftCompile"]["source"],
            eq["OrlixAppLibrary.SwiftCompile"]["promoted"],
        )
        self.assertEqual(
            eq["kernel_composition.OrlixKernelComposition"]["source"],
            eq["kernel_composition.OrlixKernelComposition"]["promoted"],
        )
        self.assertEqual(eq["swiftcompile_pairs_equal"], 32)

    def test_promoted_app_has_no_guest_source(self) -> None:
        self.assertEqual(SPECIMENS["promoted_app"]["guest_source_mnemonics"], [])
        self.assertIn("selected_macho", SPECIMENS["promoted_app"]["selected_macho_coordinate"])

    def test_hybrid_kernel_keeps_userspace_promoted(self) -> None:
        present = " ".join(SPECIMENS["hybrid-kernel"]["present"])
        absent = SPECIMENS["hybrid-kernel"]["absent"]
        self.assertIn("OrlixKernelMachOArchive", present)
        self.assertIn("OrlixPromotedUapi", present)
        self.assertIn("OrlixPromotedMlibc", present)
        self.assertIn("OrlixPromotedRootfs", present)
        for name in absent:
            self.assertNotIn(name + ":", present)
            self.assertNotIn(name, SPECIMENS["hybrid-kernel"]["present"])

    def test_hybrid_mlibc_keeps_kernel_uapi_promoted(self) -> None:
        present = " ".join(SPECIMENS["hybrid-mlibc"]["present"])
        self.assertIn("OrlixMLibCSysroot", present)
        self.assertIn("OrlixRootfs", present)
        self.assertIn("OrlixPromotedKernel", present)
        self.assertIn("OrlixPromotedUapi", present)
        self.assertNotIn("OrlixKernelMachOArchive", present)

    def test_package_true_does_not_enter_coreutils(self) -> None:
        loc = SPECIMENS["package-locality"]
        self.assertIn("true.c", loc["true_inputs_include"][0])
        self.assertFalse(loc["coreutils_has_true_c"])
        self.assertIn("selected_sysroot", loc["true_consumes"])
        self.assertIn("selected_uapi", loc["coreutils_consumes"])

    def test_cache_eviction_is_not_invalidation(self) -> None:
        self.assertTrue(SPECIMENS["cache_eviction"]["guest_identities_unchanged"])
        self.assertTrue(SPECIMENS["cache_eviction"]["action_key_independent_of_cache"])
        self.assertFalse(MATRIX["cache_eviction_is_invalidation"])


if __name__ == "__main__":
    unittest.main()
