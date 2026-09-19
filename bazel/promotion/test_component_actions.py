from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

import component_actions


def _registry(root: Path) -> Path:
    path = root / "components.json"
    path.write_text(
        json.dumps(
            {
                "components": [
                    {"name": "uapi", "label": "//bazel/feasibility/kernel:uapi"},
                    {"name": "mlibc", "label": "//bazel/feasibility/mlibc:sysroot"},
                ]
            }
        )
        + "\n",
        encoding="utf-8",
    )
    return path


class ComponentActionTests(unittest.TestCase):
    def test_counts_component_actions_and_ignores_others(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            registry = _registry(root)
            log = root / "execution.json"
            log.write_text(
                "\n".join(
                    json.dumps(entry)
                    for entry in (
                        {"targetLabel": "//bazel/feasibility/kernel:uapi", "mnemonic": "Kbuild"},
                        {"targetLabel": "//Orlix:Orlix", "mnemonic": "SwiftCompile"},
                        {"targetLabel": "//bazel/feasibility/mlibc:sysroot", "mnemonic": "Meson"},
                    )
                )
                + "\n",
                encoding="utf-8",
            )
            payload = component_actions.count_component_actions(log, registry)
            self.assertEqual(payload["total_actions"], 3)
            self.assertEqual(payload["component_actions"], 2)
            self.assertEqual(payload["components"]["//bazel/feasibility/kernel:uapi"], 1)

    def test_promoted_run_with_zero_component_actions(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            registry = _registry(root)
            log = root / "execution.json"
            log.write_text(
                json.dumps([{"targetLabel": "//Orlix:Orlix", "mnemonic": "SwiftCompile"}]) + "\n",
                encoding="utf-8",
            )
            payload = component_actions.count_component_actions(log, registry)
            self.assertEqual(payload["component_actions"], 0)
            self.assertEqual(payload["total_actions"], 1)

    def test_missing_log_is_an_error(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            registry = _registry(root)
            with self.assertRaises(component_actions.ComponentActionError):
                component_actions.count_component_actions(root / "absent.json", registry)


if __name__ == "__main__":
    unittest.main()
