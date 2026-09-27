"""Contract spec for the product invalidation matrix.

This is not a timed build and not a product build. It checks the encoded
table: the accepted Swift layout is one module, and the named edits do not
rebuild the closures that must stay reusable.
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent
REQUIRED_ROW_IDS = (
    "swift_app_shell",
    "terminal_herdr_ui",
    "orlix_engine",
    "orlix_bootloader",
    "orlix_host_adapter",
    "kernel_or_tcti_uapi_unchanged",
    "installed_uapi_bytes",
    "mlibc",
    "one_guest_package",
    "rootfs_policy",
    "proof_lock_or_build_noise",
    "toolchain_or_module_bazel",
    "cache_eviction",
)
REQUIRED_LINUX_FOUNDATIONS = {
    "compiler_runtime",
    "coreutils",
    "guest_packages",
    "installed_uapi",
    "kernel",
    "mlibc",
    "rootfs",
    "tcti",
}
REQUIRED_COMPILE_ACTIONS = {
    "apple_swift_compile",
    "compiler_runtime",
    "coreutils_compilation",
    "guest_package_compile",
    "kbuild_objects",
    "ninja_closure",
}


def _load(name: str) -> dict:
    return json.loads((ROOT / name).read_text(encoding="utf-8"))


def _rows(matrix: dict) -> dict[str, dict]:
    return {row["id"]: row for row in matrix["rows"]}


def accepted_effect(matrix: dict, row: dict) -> dict:
    layout = matrix["swift_layout"]["accepted"]
    if layout in row:
        return row[layout]
    return row["effect"]


class InvalidationMatrixTests(unittest.TestCase):
    def setUp(self) -> None:
        self.matrix = _load("invalidation-matrix.json")
        self.specimens = _load("invalidation_specimens.json")
        self.rows = _rows(self.matrix)

    def test_spec_is_not_timed_proof(self) -> None:
        self.assertEqual(self.matrix["kind"], "orlix-invalidation-matrix")
        self.assertEqual(self.matrix["proof"], "contract-spec")
        self.assertIs(self.matrix["timed_runs"], False)
        self.assertEqual(self.specimens["proof"], "contract-spec")
        self.assertIs(self.specimens["timed_runs"], False)

    def test_table_covers_every_edit(self) -> None:
        self.assertEqual(tuple(self.rows), REQUIRED_ROW_IDS)
        self.assertEqual(
            [item["row"] for item in self.specimens["specimens"]],
            list(REQUIRED_ROW_IDS),
        )

    def test_accepted_swift_layout_is_one_module(self) -> None:
        layout = self.matrix["swift_layout"]
        self.assertEqual(layout["accepted"], "single_module")
        self.assertEqual(layout["module"], "Orlix")
        self.assertEqual(self.specimens["layout"], "single_module")
        self.assertEqual(
            layout["split_modules"],
            ["app_shell", "implementation", "terminal_herdr_ui"],
        )
        app_shell = self.rows["swift_app_shell"]
        terminal = self.rows["terminal_herdr_ui"]
        self.assertIn("app_shell_module", app_shell["split"]["rebuilds"])
        self.assertIn("implementation_module", app_shell["split"]["stays_reusable"])
        self.assertIn("terminal_module", terminal["split"]["rebuilds"])
        self.assertIn("implementation_module", terminal["split"]["stays_reusable"])
        self.assertIn("Herdr is not an Engine or OrlixKit input", terminal["notes"])

    def test_terminal_and_app_shell_rebuild_one_module_without_linux(self) -> None:
        foundations = set(self.matrix["linux_foundations"])
        self.assertTrue(REQUIRED_LINUX_FOUNDATIONS <= foundations)
        modules = set()
        for row_id in ("swift_app_shell", "terminal_herdr_ui"):
            effect = accepted_effect(self.matrix, self.rows[row_id])
            modules.update(effect["rebuilds"])
            self.assertIn("orlix_swift_module", effect["rebuilds"])
            self.assertNotIn("app_shell_module", effect["rebuilds"])
            self.assertNotIn("terminal_module", effect["rebuilds"])
            self.assertNotIn("implementation_module", effect["rebuilds"])
            self.assertIn("engine", effect["stays_reusable"])
            self.assertTrue(foundations.isdisjoint(effect["rebuilds"]))
        self.assertIn("orlix_swift_module", modules)
        self.assertTrue(modules.isdisjoint(foundations))

    def test_kernel_or_tcti_with_unchanged_uapi_does_not_rebuild_mlibc(self) -> None:
        effect = accepted_effect(self.matrix, self.rows["kernel_or_tcti_uapi_unchanged"])
        self.assertIn("kbuild_objects", effect["rebuilds"])
        self.assertIn("kernel_products", effect["rebuilds"])
        self.assertNotIn("mlibc", effect["rebuilds"])
        self.assertIn("mlibc", effect["stays_reusable"])
        self.assertIn("apple_swift_compile", effect["stays_reusable"])

    def test_mlibc_does_not_rebuild_the_kernel(self) -> None:
        effect = accepted_effect(self.matrix, self.rows["mlibc"])
        self.assertIn("ninja_closure", effect["rebuilds"])
        self.assertIn("rootfs", effect["rebuilds"])
        self.assertNotIn("kernel", effect["rebuilds"])
        self.assertIn("kernel", effect["stays_reusable"])
        self.assertIn("compiler_runtime", effect["stays_reusable"])
        self.assertIn("bootloader", effect["stays_reusable"])
        self.assertIn("host_adapter", effect["stays_reusable"])

    def test_one_guest_package_does_not_rebuild_kernel_or_mlibc(self) -> None:
        effect = accepted_effect(self.matrix, self.rows["one_guest_package"])
        self.assertEqual(set(effect["rebuilds"]), {"guest_package", "rootfs"})
        for token in ("kernel", "mlibc", "mlibc_compilation", "other_guest_packages"):
            self.assertNotIn(token, effect["rebuilds"])
            self.assertIn(token, effect["stays_reusable"])

    def test_rootfs_policy_does_not_rebuild_compilers_or_packages(self) -> None:
        effect = accepted_effect(self.matrix, self.rows["rootfs_policy"])
        self.assertEqual(set(effect["rebuilds"]), {"resource_packaging", "rootfs_assembly"})
        for token in (
            "compiler_runtime",
            "coreutils_compilation",
            "guest_package_compile",
            "kernel",
            "mlibc",
        ):
            self.assertNotIn(token, effect["rebuilds"])
            self.assertIn(token, effect["stays_reusable"])

    def test_proof_lock_and_build_noise_does_not_rebuild_compiles(self) -> None:
        effect = accepted_effect(self.matrix, self.rows["proof_lock_or_build_noise"])
        compile_actions = set(self.matrix["compile_actions"])
        self.assertTrue(REQUIRED_COMPILE_ACTIONS <= compile_actions)
        self.assertEqual(effect["rebuilds"], ["evidence"])
        self.assertTrue(compile_actions.isdisjoint(effect["rebuilds"]))
        self.assertEqual(set(effect["stays_reusable"]), compile_actions)

    def test_toolchain_fails_closed_and_eviction_is_not_invalidation(self) -> None:
        toolchain = self.rows["toolchain_or_module_bazel"]
        self.assertIs(toolchain["fail_closed"], True)
        toolchain_effect = accepted_effect(self.matrix, toolchain)
        self.assertEqual(toolchain_effect["rebuilds"], ["affected_closure"])
        self.assertEqual(toolchain_effect["stays_reusable"], [])
        eviction = accepted_effect(self.matrix, self.rows["cache_eviction"])
        self.assertEqual(eviction["rebuilds"], [])
        self.assertIs(eviction["eviction_is_not_invalidation"], True)
        self.assertIn(
            "eviction is not invalidation",
            self.rows["cache_eviction"]["notes"][0],
        )

    def test_host_adapter_keeps_guest_producers_unless_the_interface_changes(self) -> None:
        row = self.rows["orlix_host_adapter"]
        effect = accepted_effect(self.matrix, row)
        self.assertIn("guest_producers", effect["stays_reusable"])
        self.assertTrue(any("interface" in note for note in row["notes"]))

    def test_specimens_match_the_accepted_effects(self) -> None:
        for specimen in self.specimens["specimens"]:
            effect = accepted_effect(self.matrix, self.rows[specimen["row"]])
            rebuilds = set(effect["rebuilds"])
            self.assertEqual(set(specimen["rebuilds"]), rebuilds)
            self.assertTrue(set(specimen["must_not_rebuild"]).isdisjoint(rebuilds))
            if specimen["must_not_rebuild"] and specimen["row"] != "cache_eviction":
                self.assertTrue(set(specimen["must_not_rebuild"]) <= set(effect["stays_reusable"]))


if __name__ == "__main__":
    unittest.main()
