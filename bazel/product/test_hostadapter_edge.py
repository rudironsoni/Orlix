"""HostAdapter composition edge from nm fixtures."""

from __future__ import annotations

import json
import tempfile
import unittest
from pathlib import Path

from hostadapter_edge import (
    ARCHIVE_ORDER,
    BOOT_ENTRY,
    FRAMEWORKS,
    TRAP_CALLBACK,
    HostAdapterEdgeError,
    check_product_document,
    composition_document,
    composition_edge,
    main,
    parse_nm,
)

ROOT = Path(__file__).resolve().parents[2]
DIGEST = "2a4697e1928fd6a33edb08cefe13cdd6c84a621bc9a0f0c694e303b1758b486c"
HOST = "ab" * 32
BOOT = "cd" * 32

LINUX_NM = """\
OrlixKernel.a(boot.o):
0000000000000000 T _arch_boot_entry
                 U _memcpy
                 U _orlix_host_time_monotonic_ns
                 U _orlix_host_user_trap_install
_orlix_host_user_unmap_pages_serialized T 0 16
"""

HOST_NM = """\
_orlix_host_time_monotonic_ns t 0 8
_orlix_host_user_trap_install t 100 32
_orlix_host_resources_register_root_image T 200 40
_orlix_host_directory_read_entry t 240 12
_memcpy T 0 4
"""

BOOT_NM = """\
                 U _arch_boot_entry
                 U _orlix_host_boot_progress_note
_orlix_boot_handoff T 0 20
"""


def _edge(**overrides: object) -> dict[str, object]:
    values = {
        "linux_nm": LINUX_NM,
        "host_nm": HOST_NM,
        "boot_nm": BOOT_NM,
        "archive_order": list(ARCHIVE_ORDER),
        "frameworks": list(FRAMEWORKS),
        "link_paths": [
            "bazel-out/ios/OrlixKernel.a",
            "bazel-out/ios/libOrlixHostAdapter.a",
            "bazel-out/ios/libOrlixKernelBoot.a",
        ],
    }
    values.update(overrides)
    return composition_edge(**values)


class HostAdapterEdgeTests(unittest.TestCase):
    def test_posix_and_bsd_nm_lines_parse(self) -> None:
        parsed = parse_nm("_foo T 10 4\n0000000000000000 U _bar\narchive(member.o):\n")
        self.assertEqual(parsed["_foo"], {"T"})
        self.assertEqual(parsed["_bar"], {"U"})

    def test_edge_keeps_host_imports_and_drops_libc(self) -> None:
        edge = _edge()
        self.assertEqual(
            edge["undefined_kernel_symbols"],
            ["_orlix_host_time_monotonic_ns", "_orlix_host_user_trap_install"],
        )
        self.assertNotIn("_memcpy", edge["undefined_kernel_symbols"])
        self.assertEqual(edge["boot_entry"], BOOT_ENTRY)
        self.assertEqual(edge["callbacks"], [TRAP_CALLBACK])
        self.assertIn("_orlix_host_resources_register_root_image", edge["resource_lookup"])
        self.assertIn("_orlix_host_directory_read_entry", edge["resource_lookup"])
        self.assertEqual(edge["symbol_visibility"]["_orlix_host_time_monotonic_ns"], "private")
        self.assertEqual(edge["archive_order"], ARCHIVE_ORDER)
        self.assertEqual(edge["frameworks"], FRAMEWORKS)
        self.assertEqual(edge["guest_link_dependencies"], [])

    def test_missing_host_export_boot_entry_or_guest_path_fails(self) -> None:
        with self.assertRaisesRegex(HostAdapterEdgeError, "does not define"):
            _edge(host_nm=HOST_NM.replace("_orlix_host_user_trap_install t 100 32\n", ""))
        with self.assertRaisesRegex(HostAdapterEdgeError, "must define"):
            _edge(linux_nm=LINUX_NM.replace("0000000000000000 T _arch_boot_entry\n", ""))
        with self.assertRaisesRegex(HostAdapterEdgeError, "must reference"):
            _edge(boot_nm=BOOT_NM.replace("                 U _arch_boot_entry\n", ""))
        with self.assertRaisesRegex(HostAdapterEdgeError, "guest archive"):
            _edge(link_paths=["OrlixMLibC/Sources/libc.a"])
        with self.assertRaisesRegex(HostAdapterEdgeError, "archive order"):
            _edge(archive_order=["OrlixHostAdapter", "OrlixKernel.a", "OrlixKernelBoot"])
        with self.assertRaisesRegex(HostAdapterEdgeError, "framework"):
            _edge(frameworks=["Foundation"])

    def test_document_records_the_edge_and_null_buildset(self) -> None:
        text = composition_document(
            linux_digest=DIGEST,
            host_digest=HOST,
            boot_digest=BOOT,
            edge=_edge(),
        )
        document = json.loads(text)
        check_product_document(document)
        self.assertIn('"buildset": null', text)
        self.assertNotIn('"undefined_kernel_symbols": []', text)
        self.assertNotIn(DIGEST, document["undefined_kernel_symbols"])
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "composition.json"
            path.write_text(text, encoding="utf-8")
            self.assertEqual(main(["--check", str(path)]), 0)
            empty = text.replace(
                '"undefined_kernel_symbols": [\n    "_orlix_host_time_monotonic_ns",\n    "_orlix_host_user_trap_install"\n  ]',
                '"undefined_kernel_symbols": []',
            )
            path.write_text(empty, encoding="utf-8")
            with self.assertRaisesRegex(HostAdapterEdgeError, "non-empty"):
                main(["--check", str(path)])

    def test_rule_uses_nm_evidence_for_the_product_composition(self) -> None:
        composition = (ROOT / "bazel/product/composition.bzl").read_text(encoding="utf-8")
        implementation = composition.split("def _kernel_composition_impl", 1)[1].split(
            "orlix_kernel_composition = rule", 1
        )[0]
        self.assertIn("_edge_tool", implementation)
        self.assertIn("llvm-nm", implementation)
        self.assertIn("hostadapter_edge.py", composition)
        self.assertNotIn('"undefined_kernel_symbols": []', implementation)
        self.assertIn('"symbol_edge": false', implementation)
        product = (ROOT / "bazel/product/BUILD.bazel").read_text(encoding="utf-8")
        target = product.split('name = "kernel_composition"', 1)[1].split("orlix_kernel_composition_stamp(", 1)[0]
        self.assertIn("record_symbol_edge = True", target)
        self.assertIn('hostadapter_library = "//OrlixHostAdapter/Sources:OrlixHostAdapter"', target)
        self.assertIn('boot_library = "//OrlixKernel/Sources:OrlixKernelBoot"', target)
        probe = product.split('name = "action_identity_composition"', 1)[1].split(
            "orlix_kernel_composition_stamp(", 1
        )[0]
        self.assertNotIn("record_symbol_edge = True", probe)
        makefile = (ROOT / "make/bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn("hostadapter_edge.py --check", makefile)
        self.assertNotIn('"undefined_kernel_symbols": []', makefile)


if __name__ == "__main__":
    unittest.main()
