"""The terminal presentation module is not a dependency of unrelated features."""

from __future__ import annotations

import ast
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MODULES = ROOT / "Orlix" / "app_modules.bzl"
UNRELATED = ("OrlixFiles", "OrlixModels", "OrlixSSH", "OrlixSettings", "OrlixStats")
TERMINAL_UI = "Orlix/Features/TerminalSessions/UI/Tabs/ConnectionTabsView.swift"


def _assignment(name: str) -> dict:
    text = MODULES.read_text(encoding="utf-8")
    marker = f"{name} = "
    start = text.index(marker) + len(marker)
    if text[start] != "{":
        raise AssertionError(f"{name} does not start with a dict")
    depth = 0
    for index, char in enumerate(text[start:], start):
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                result = ast.literal_eval(text[start : index + 1])
                if not isinstance(result, dict):
                    raise AssertionError(f"{name} is not a dict")
                return result
    raise AssertionError(f"{name} dict is unterminated")


def _closure(deps: dict[str, list[str]], module: str) -> set[str]:
    seen = set()
    stack = [item.removeprefix(":") for item in deps[module]]
    while stack:
        item = stack.pop()
        if item in seen or item not in deps:
            continue
        seen.add(item)
        stack.extend(dep.removeprefix(":") for dep in deps[item])
    return seen


class AppSwiftModuleTests(unittest.TestCase):
    def test_terminal_presentation_is_not_a_dependency_of_unrelated_features(self) -> None:
        deps = _assignment("ORLIX_APP_MODULE_DEPS")
        extra = _assignment("_EXTRA_DEPS")
        for module, labels in extra.items():
            feature_deps = [label for label in labels if label.startswith(":Orlix")]
            deps[module] = list(deps[module]) + feature_deps
        for module in UNRELATED:
            self.assertNotIn(
                "OrlixTerminal",
                _closure(deps, module),
                f"{module} recompiles when terminal presentation changes",
            )
        self.assertIn("OrlixTerminal", _closure(deps, "OrlixComposition"))
        self.assertIn("OrlixTerminal", _closure(deps, "OrlixApp"))

    def test_feature_modules_do_not_form_a_cycle(self) -> None:
        deps = _assignment("ORLIX_APP_MODULE_DEPS")
        for module in deps:
            self.assertNotIn(module, _closure(deps, module), module)

    def test_terminal_ui_source_belongs_only_to_the_terminal_module(self) -> None:
        srcs = _assignment("ORLIX_APP_MODULE_SRCS")
        owners = [name for name, files in srcs.items() if TERMINAL_UI in files]
        self.assertEqual(owners, ["OrlixTerminal"])
        seen = []
        for files in srcs.values():
            seen.extend(files)
        self.assertEqual(len(seen), len(set(seen)))
        tree = {
            path.relative_to(ROOT / "Orlix").as_posix()
            for path in (ROOT / "Orlix" / "Orlix").rglob("*.swift")
        }
        tree.update(
            path.relative_to(ROOT / "Orlix").as_posix()
            for path in (ROOT / "Orlix" / "OrlixShared").glob("*.swift")
        )
        self.assertEqual(set(seen), tree)
