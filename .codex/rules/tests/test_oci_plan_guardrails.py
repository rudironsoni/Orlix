#!/usr/bin/env python3

from pathlib import Path
import unittest


REPO_ROOT = Path(__file__).resolve().parents[3]
PLAN = REPO_ROOT / "docs/plans/active/oci-derived-environments-virtio-plane/PLAN.md"


class OCIDerivedPlanGuardrailTests(unittest.TestCase):
    def test_plan_forbids_custom_orlix_package_manager(self):
        text = PLAN.read_text()

        self.assertIn("Orlix must not invent or ship a parallel package-management system", text)
        self.assertIn("No Orlix package manager", text)
        self.assertIn("No Orlix package abstraction over OCI image contents", text)

    def test_plan_routes_package_operations_to_distro_native_tools(self):
        text = PLAN.read_text()

        self.assertIn("Package operations inside an imported environment belong to the distro", text)
        for package_manager in ("apk", "apt", "dnf", "rpm", "pacman"):
            with self.subTest(package_manager=package_manager):
                self.assertIn(f"`{package_manager}`", text)

    def test_registry_pull_outputs_oci_layout_not_orlix_package_index(self):
        text = PLAN.read_text()

        self.assertIn("Registry pull, when implemented, must produce a verified OCI layout input", text)
        self.assertIn("must not introduce an\n  Orlix package index", text)


if __name__ == "__main__":
    unittest.main()
