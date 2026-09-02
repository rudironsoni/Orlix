from __future__ import annotations

import tempfile
import unittest
from pathlib import Path

import ios15_simulator_gate as gate


WEAK_APPINTENTS = """
06CC5CB4306A6DE83F12DE5D /* AppIntents.framework in Frameworks */ = {isa = PBXBuildFile; fileRef = D7395D22D427A34214C60A4D /* AppIntents.framework */; settings = {ATTRIBUTES = (Weak, ); }; };
6926E116EA625F0FAF015225 /* AppIntents.framework in Frameworks */ = {isa = PBXBuildFile; fileRef = D7395D22D427A34214C60A4D /* AppIntents.framework */; settings = {ATTRIBUTES = (Weak, ); }; };
"""

REQUIRED_APPINTENTS = """
06CC5CB4306A6DE83F12DE5D /* AppIntents.framework in Frameworks */ = {isa = PBXBuildFile; fileRef = D7395D22D427A34214C60A4D /* AppIntents.framework */; };
"""

REQUIRED_OTOOL = """
          cmd LC_LOAD_DYLIB
      cmdsize 88
         name /System/Library/Frameworks/AppIntents.framework/AppIntents (offset 24)
          cmd LC_LOAD_WEAK_DYLIB
      cmdsize 88
         name /System/Library/Frameworks/ActivityKit.framework/ActivityKit (offset 24)
"""

WEAK_OTOOL = """
          cmd LC_LOAD_WEAK_DYLIB
      cmdsize 88
         name /System/Library/Frameworks/AppIntents.framework/AppIntents (offset 24)
          cmd LC_LOAD_WEAK_DYLIB
      cmdsize 88
         name /System/Library/Frameworks/ActivityKit.framework/ActivityKit (offset 24)
"""


class IOS15SimulatorGateTests(unittest.TestCase):
    def write_pbxproj(self, text: str) -> Path:
        directory = Path(tempfile.mkdtemp())
        path = directory / "project.pbxproj"
        path.write_text(text, encoding="utf-8")
        return path

    def test_weak_appintents_project_passes(self) -> None:
        gate.validate_generated_project(self.write_pbxproj(WEAK_APPINTENTS))

    def test_required_appintents_project_fails(self) -> None:
        with self.assertRaises(gate.GateError) as error:
            gate.validate_generated_project(self.write_pbxproj(REQUIRED_APPINTENTS))
        self.assertIn("required-linked", str(error.exception))

    def test_required_appintents_binary_fails(self) -> None:
        app = Path(tempfile.mkdtemp()) / "Orlix.app"
        app.mkdir()
        (app / "Orlix.debug.dylib").write_bytes(b"fake")
        with self.assertRaises(gate.GateError) as error:
            gate.validate_simulator_app(
                app,
                otool_output_by_binary={"Orlix.debug.dylib": REQUIRED_OTOOL},
            )
        self.assertIn("required-loads", str(error.exception))

    def test_weak_appintents_binary_passes(self) -> None:
        app = Path(tempfile.mkdtemp()) / "Orlix.app"
        app.mkdir()
        (app / "Orlix.debug.dylib").write_bytes(b"fake")
        gate.validate_simulator_app(
            app,
            otool_output_by_binary={"Orlix.debug.dylib": WEAK_OTOOL},
        )
