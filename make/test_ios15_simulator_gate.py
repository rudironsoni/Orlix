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
                nm_output_by_binary={"Orlix.debug.dylib": ""},
            )
        self.assertIn("required-loads", str(error.exception))

    def test_weak_appintents_binary_passes(self) -> None:
        app = Path(tempfile.mkdtemp()) / "Orlix.app"
        app.mkdir()
        (app / "Orlix.debug.dylib").write_bytes(b"fake")
        gate.validate_simulator_app(
            app,
            otool_output_by_binary={"Orlix.debug.dylib": WEAK_OTOOL},
            nm_output_by_binary={"Orlix.debug.dylib": ""},
        )

    def test_macho_arch_name_undefined_fails(self) -> None:
        app = Path(tempfile.mkdtemp()) / "Orlix.app"
        app.mkdir()
        (app / "Orlix.debug.dylib").write_bytes(b"fake")
        with self.assertRaises(gate.GateError) as error:
            gate.validate_simulator_app(
                app,
                otool_output_by_binary={"Orlix.debug.dylib": WEAK_OTOOL},
                nm_output_by_binary={"Orlix.debug.dylib": "U _macho_arch_name_for_cpu_type\n"},
            )
        self.assertIn("_macho_arch_name_for_cpu_type", str(error.exception))

    def test_project_disables_debug_dylib(self) -> None:
        project = Path(__file__).resolve().parents[1] / "project.yml"
        text = project.read_text(encoding="utf-8")
        self.assertIn("ENABLE_DEBUG_DYLIB: NO", text)
        self.assertIn("macho_arch_name.c", text)

    def test_hostadapter_defines_macho_arch_name_for_ios15(self) -> None:
        src = (
            Path(__file__).resolve().parents[1]
            / "OrlixHostAdapter/Sources/OrlixHostAdapter/execution/macho_arch_name.c"
        )
        text = src.read_text(encoding="utf-8")
        self.assertIn("macho_arch_name_for_cpu_type", text)
        self.assertIn("macho_cpu_type_for_arch_name", text)

    def test_kernel_archive_script_puts_lld_on_path(self) -> None:
        project = Path(__file__).resolve().parents[1] / "project.yml"
        text = project.read_text(encoding="utf-8")
        self.assertIn("/opt/homebrew/opt/lld/bin", text)
        self.assertIn("Build OrlixKernel Archive", text)

    def test_orlixos_makefile_unexports_apple_sdkroot(self) -> None:
        root = Path(__file__).resolve().parents[1]
        for rel in ("OrlixOS/Makefile", "OrlixCoreUtils/Makefile", "OrlixMLibC/Makefile"):
            text = (root / rel).read_text(encoding="utf-8")
            self.assertIn("unexport SDKROOT", text, rel)
        embed = (root / "project.yml").read_text(encoding="utf-8")
        self.assertIn("unset SDKROOT", embed)
        toolchain = (root / "OrlixOS/Sources/make/toolchain.mk").read_text(encoding="utf-8")
        self.assertIn('export SDKROOT="$$ORLIXOS_MLIBC_SYSROOT"', toolchain)
        self.assertIn("-isystem \"$$ORLIXOS_MLIBC_SYSROOT/usr/include\"", toolchain)
        linux_features = (root / "OrlixOS/Sources/make/linux-feature-packages.mk").read_text(encoding="utf-8")
        self.assertIn('export SDKROOT="$$sysroot"', linux_features)
        self.assertIn("-isystem \"$$sysroot/usr/include\"", linux_features)

    def test_ios15_gate_removes_xcresult_before_each_xcodebuild(self) -> None:
        makefile = Path(__file__).resolve().parents[1] / "Makefile"
        text = makefile.read_text(encoding="utf-8")
        self.assertGreaterEqual(text.count('rm -rf "$$result_bundle"'), 2)
        self.assertGreaterEqual(text.count("ENABLE_DEBUG_DYLIB=NO"), 2)
