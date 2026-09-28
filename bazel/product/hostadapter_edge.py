"""HostAdapter composition edge from nm output.

The Linux archive's host-shaped undefined symbols are the edge. HostAdapter
must define each one. Guest mlibc, Coreutils, and libc archives are not link
inputs. This module does not invoke a compiler or a wrapper Makefile.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

ARCHIVE_ORDER = ["OrlixKernel.a", "OrlixHostAdapter", "OrlixKernelBoot"]
FRAMEWORKS = ["CoreFoundation", "Foundation"]
BOOT_ENTRY = "_arch_boot_entry"
TRAP_CALLBACK = "_orlix_host_user_trap_install"
GUEST_MARKERS = ("OrlixMLibC", "OrlixCoreUtils", "libc.a", "libcoreutils")
_NM_TYPES = set("ABCDGIRSTUVabcdgirstuvwW")


class HostAdapterEdgeError(ValueError):
    pass


def _hex_token(token: str) -> bool:
    return bool(token) and all(character in "0123456789abcdefABCDEF" for character in token)


def parse_nm(text: str) -> dict[str, set[str]]:
    symbols: dict[str, set[str]] = {}
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.endswith(":"):
            continue
        parts = line.split()
        name = ""
        kind = ""
        if (
            len(parts) >= 2
            and len(parts[1]) == 1
            and parts[1] in _NM_TYPES
            and not _hex_token(parts[0])
        ):
            name, kind = parts[0], parts[1]
        elif len(parts) >= 2 and len(parts[-2]) == 1 and parts[-2] in _NM_TYPES:
            name, kind = parts[-1], parts[-2]
        else:
            continue
        symbols.setdefault(name, set()).add(kind)
    return symbols


def _host_shaped(name: str) -> bool:
    bare = name.lstrip("_")
    return bare.startswith("orlix_host_") or bare.startswith("OrlixHost")


def _defined(kinds: set[str]) -> bool:
    return any(kind not in {"U", "u"} for kind in kinds)


def _undefined_only(symbols: dict[str, set[str]], name: str) -> bool:
    kinds = symbols.get(name, set())
    return bool(kinds) and not _defined(kinds)


def _visibility(kinds: set[str]) -> str:
    defined = {kind for kind in kinds if kind not in {"U", "u"}}
    if any(kind.isupper() for kind in defined):
        return "global"
    if defined:
        return "private"
    raise HostAdapterEdgeError("symbol visibility requires a definition")


def _reject_guest(paths: list[str]) -> None:
    for path in paths:
        for marker in GUEST_MARKERS:
            if marker in path:
                raise HostAdapterEdgeError(f"guest archive is not a HostAdapter link input: {path}")


def composition_edge(
    *,
    linux_nm: str,
    host_nm: str,
    boot_nm: str,
    archive_order: list[str],
    frameworks: list[str],
    link_paths: list[str],
) -> dict[str, object]:
    if archive_order != ARCHIVE_ORDER:
        raise HostAdapterEdgeError("archive order must be OrlixKernel.a, OrlixHostAdapter, OrlixKernelBoot")
    if frameworks != FRAMEWORKS:
        raise HostAdapterEdgeError("framework visibility must be CoreFoundation and Foundation")
    _reject_guest(link_paths)
    linux = parse_nm(linux_nm)
    host = parse_nm(host_nm)
    boot = parse_nm(boot_nm)
    if not _defined(linux.get(BOOT_ENTRY, set())):
        raise HostAdapterEdgeError(f"Linux archive must define {BOOT_ENTRY}")
    if not _undefined_only(boot, BOOT_ENTRY):
        raise HostAdapterEdgeError(f"boot archive must reference {BOOT_ENTRY}")
    undefined = sorted(name for name in linux if _host_shaped(name) and _undefined_only(linux, name))
    if not undefined:
        raise HostAdapterEdgeError("Linux archive has no host-shaped undefined symbols")
    exports = sorted(name for name in host if _host_shaped(name) and _defined(host[name]))
    missing = [name for name in undefined if name not in set(exports)]
    if missing:
        raise HostAdapterEdgeError("HostAdapter does not define " + ", ".join(missing))
    callbacks = sorted(name for name in undefined if "trap" in name)
    if TRAP_CALLBACK not in callbacks:
        raise HostAdapterEdgeError(f"callback edge requires {TRAP_CALLBACK}")
    resource_lookup = sorted(
        name for name in exports if "resource" in name.lower() or "directory" in name.lower()
    )
    if not resource_lookup:
        raise HostAdapterEdgeError("HostAdapter exports no resource-lookup symbols")
    return {
        "undefined_kernel_symbols": undefined,
        "hostadapter_exports": exports,
        "boot_entry": BOOT_ENTRY,
        "callbacks": callbacks,
        "resource_lookup": resource_lookup,
        "archive_order": list(ARCHIVE_ORDER),
        "frameworks": list(FRAMEWORKS),
        "symbol_visibility": {name: _visibility(host[name]) for name in undefined},
        "guest_link_dependencies": [],
    }


def composition_document(
    *,
    linux_digest: str,
    host_digest: str,
    boot_digest: str,
    edge: dict[str, object],
) -> str:
    for label, digest in (
        ("linux_archive", linux_digest),
        ("hostadapter", host_digest),
        ("boot", boot_digest),
    ):
        if len(digest) != 64 or any(character not in "0123456789abcdef" for character in digest):
            raise HostAdapterEdgeError(f"{label} digest must be 64 lowercase hex characters")
    if edge.get("guest_link_dependencies") != []:
        raise HostAdapterEdgeError("guest link dependencies must stay empty")
    document = {
        "component": "OrlixKernel-composition",
        "linux_archive": linux_digest,
        "hostadapter": host_digest,
        "boot": boot_digest,
        "linked_symbol": edge["boot_entry"],
        "undefined_kernel_symbols": edge["undefined_kernel_symbols"],
        "hostadapter_exports": edge["hostadapter_exports"],
        "boot_entry": edge["boot_entry"],
        "callbacks": edge["callbacks"],
        "resource_lookup": edge["resource_lookup"],
        "archive_order": edge["archive_order"],
        "frameworks": edge["frameworks"],
        "symbol_visibility": edge["symbol_visibility"],
        "guest_link_dependencies": [],
        "buildset": None,
        "xcframework": None,
        "wrapper_makefile": False,
        "symbol_edge": True,
    }
    return json.dumps(document, indent=2) + "\n"


def check_product_document(document: dict[str, object]) -> None:
    if document.get("symbol_edge") is not True:
        raise HostAdapterEdgeError("product composition must record the symbol edge")
    if document.get("buildset") is not None:
        raise HostAdapterEdgeError("source composition must keep buildset null")
    if document.get("xcframework") is not None:
        raise HostAdapterEdgeError("composition must not claim an xcframework")
    if document.get("wrapper_makefile") is not False:
        raise HostAdapterEdgeError("composition must not invoke a wrapper Makefile")
    undefined = document.get("undefined_kernel_symbols")
    exports = document.get("hostadapter_exports")
    if not isinstance(undefined, list) or not undefined:
        raise HostAdapterEdgeError("undefined_kernel_symbols must be a non-empty list")
    if not isinstance(exports, list) or not set(undefined).issubset(exports):
        raise HostAdapterEdgeError("every undefined kernel symbol must be a HostAdapter export")
    if any(not _host_shaped(name) for name in undefined):
        raise HostAdapterEdgeError("undefined kernel symbols must be host-shaped")
    if document.get("boot_entry") != BOOT_ENTRY or document.get("linked_symbol") != BOOT_ENTRY:
        raise HostAdapterEdgeError(f"boot entry must be {BOOT_ENTRY}")
    callbacks = document.get("callbacks")
    if not isinstance(callbacks, list) or TRAP_CALLBACK not in callbacks or not set(callbacks).issubset(undefined):
        raise HostAdapterEdgeError("callbacks must be trap imports from the kernel edge")
    resources = document.get("resource_lookup")
    if not isinstance(resources, list) or not resources or not set(resources).issubset(exports):
        raise HostAdapterEdgeError("resource lookup must be HostAdapter exports")
    if document.get("archive_order") != ARCHIVE_ORDER:
        raise HostAdapterEdgeError("archive order drifted")
    if document.get("frameworks") != FRAMEWORKS:
        raise HostAdapterEdgeError("framework visibility drifted")
    if document.get("guest_link_dependencies") != []:
        raise HostAdapterEdgeError("guest link dependencies must stay empty")
    visibility = document.get("symbol_visibility")
    if not isinstance(visibility, dict) or set(visibility) != set(undefined):
        raise HostAdapterEdgeError("symbol visibility must cover the kernel edge")
    if any(value not in {"global", "private"} for value in visibility.values()):
        raise HostAdapterEdgeError("symbol visibility must be global or private")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--check")
    parser.add_argument("--linux-nm")
    parser.add_argument("--host-nm")
    parser.add_argument("--boot-nm")
    parser.add_argument("--linux-digest")
    parser.add_argument("--host-digest")
    parser.add_argument("--boot-digest")
    parser.add_argument("--archive-order", action="append", default=[])
    parser.add_argument("--framework", action="append", default=[])
    parser.add_argument("--link-path", action="append", default=[])
    parser.add_argument("--out")
    args = parser.parse_args(argv)
    if args.check:
        check_product_document(json.loads(Path(args.check).read_text(encoding="utf-8")))
        return 0
    required = (
        args.linux_nm,
        args.host_nm,
        args.boot_nm,
        args.linux_digest,
        args.host_digest,
        args.boot_digest,
        args.out,
    )
    if not all(required):
        raise HostAdapterEdgeError("symbol edge requires nm text, digests, and an output path")
    edge = composition_edge(
        linux_nm=Path(args.linux_nm).read_text(encoding="utf-8"),
        host_nm=Path(args.host_nm).read_text(encoding="utf-8"),
        boot_nm=Path(args.boot_nm).read_text(encoding="utf-8"),
        archive_order=args.archive_order,
        frameworks=args.framework,
        link_paths=args.link_path,
    )
    Path(args.out).write_text(
        composition_document(
            linux_digest=args.linux_digest,
            host_digest=args.host_digest,
            boot_digest=args.boot_digest,
            edge=edge,
        ),
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
