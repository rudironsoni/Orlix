"""Fail loud when the iOS 15 app required-loads later-system Apple frameworks."""

from __future__ import annotations

import re
import subprocess
from pathlib import Path

FORBIDDEN_REQUIRED_FRAMEWORKS = (
    "AppIntents.framework",
    "ActivityKit.framework",
)

FORBIDDEN_UNDEFINED_SYMBOLS = (
    "_macho_arch_name_for_cpu_type",
    "_macho_cpu_type_for_arch_name",
    "_macho_arch_name_for_mach_header",
)


class GateError(RuntimeError):
    pass


def fail(message: str) -> None:
    raise GateError(message)


def validate_generated_project(pbxproj: Path) -> None:
    try:
        text = pbxproj.read_text(encoding="utf-8")
    except OSError as error:
        fail(f"cannot read generated project {pbxproj}: {error}")

    entries = re.findall(
        r"/\* AppIntents\.framework in Frameworks \*/ = \{[^}]+\};",
        text,
    )
    weak_ldflag = "-weak_framework" in text and "AppIntents" in text
    if not entries and not weak_ldflag:
        fail("generated project is missing AppIntents.framework in Frameworks")
    required = [entry for entry in entries if "ATTRIBUTES = (Weak" not in entry]
    if required:
        fail(
            "AppIntents.framework is required-linked; iOS 15 cannot load it:\n"
            + "\n".join(required)
        )


def required_dylib_names(otool_output: str) -> list[str]:
    names: list[str] = []
    required = False
    for line in otool_output.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0] == "cmd":
            required = parts[1] == "LC_LOAD_DYLIB"
            continue
        if required and len(parts) >= 2 and parts[0] == "name":
            names.append(parts[1])
    return names


def undefined_symbol_names(nm_output: str) -> list[str]:
    names: list[str] = []
    for line in nm_output.splitlines():
        stripped = line.strip()
        if not stripped.startswith("U "):
            continue
        parts = stripped.split()
        if len(parts) >= 2:
            names.append(parts[-1])
    return names


def validate_simulator_app(
    app: Path,
    otool_output_by_binary: dict[str, str] | None = None,
    nm_output_by_binary: dict[str, str] | None = None,
) -> None:
    if not app.is_dir():
        fail(f"missing iOS 15 simulator app: {app}")

    binaries = [path for path in (app / "Orlix", app / "Orlix.debug.dylib") if path.is_file()]
    if not binaries:
        fail(f"missing Mach-O in {app}")

    bad: list[str] = []
    for binary in binaries:
        if otool_output_by_binary is None:
            try:
                output = subprocess.check_output(
                    ["otool", "-l", str(binary)],
                    text=True,
                    stderr=subprocess.STDOUT,
                )
            except (OSError, subprocess.CalledProcessError) as error:
                fail(f"cannot inspect {binary}: {error}")
        else:
            output = otool_output_by_binary.get(binary.name)
            if output is None:
                fail(f"missing otool output for {binary.name}")
        for name in required_dylib_names(output):
            if any(fragment in name for fragment in FORBIDDEN_REQUIRED_FRAMEWORKS):
                bad.append(f"{binary.name} required-loads {name}")
        if nm_output_by_binary is None:
            try:
                nm_output = subprocess.check_output(
                    ["nm", "-u", str(binary)],
                    text=True,
                    stderr=subprocess.STDOUT,
                )
            except (OSError, subprocess.CalledProcessError) as error:
                fail(f"cannot nm {binary}: {error}")
        else:
            nm_output = nm_output_by_binary.get(binary.name)
            if nm_output is None:
                fail(f"missing nm output for {binary.name}")
        for symbol in undefined_symbol_names(nm_output):
            if symbol in FORBIDDEN_UNDEFINED_SYMBOLS:
                bad.append(f"{binary.name} undefined {symbol} is missing on iOS 15.5 libSystem")
    if bad:
        fail(
            "iOS 15 cannot required-load later-system frameworks:\n"
            + "\n".join(bad)
        )
