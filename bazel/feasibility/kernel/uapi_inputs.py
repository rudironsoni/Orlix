"""Inputs that Linux 6.12.105 ARCH=arm64 headers_install actually reads.

The kernel rule mirrors these prefixes. headers_install is a no-dot-config
target: UAPI headers, Kbuild scripts, the arm64 Makefile and syscall tables.
Kernel, mm, fs, and driver .c files are not inputs.
"""

from __future__ import annotations

TREE_PREFIXES = (
    "include/uapi/",
    "arch/arm64/include/uapi/",
    "arch/arm64/tools/",
    "scripts/",
)
EXACT = (
    "Makefile",
    "Kbuild",
    "arch/arm64/Makefile",
    "arch/arm64/kernel/Makefile.syscalls",
)


def linux_relative(path: str) -> str:
    marker = "orlix_linux_source/"
    index = path.find(marker)
    if index >= 0:
        path = path[index + len(marker):]
    if path.startswith("./"):
        path = path[2:]
    return path


def headers_install_source(path: str) -> bool:
    relative = linux_relative(path)
    if relative.startswith(TREE_PREFIXES):
        return True
    if relative.endswith((".c", ".S")):
        return False
    return relative in EXACT
