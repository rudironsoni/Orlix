#!/usr/bin/env python3
"""Regression tests for product_initcall_reorder.py as binary transformation code.

Every test builds a synthetic 64-bit Mach-O relocatable object in memory, so
the rewriter is exercised without a toolchain. The fixtures cover the exact
defect class under repair: symbols must be classified by their owning section
index, never by numeric address range alone.
"""

from __future__ import annotations

import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from product_initcall_reorder import ReorderError, reorder

MH_MAGIC_64 = 0xFEEDFACF
N_SECT = 0x0E
N_EXT = 0x01
LOCAL_SECT = N_SECT
GLOBAL_SECT = N_SECT | N_EXT


def build_object(sections, symbols):
    """Build a minimal MH_OBJECT.

    sections: list of (segment, section, size, [(reloc_address, reloc_info)]).
    symbols: list of (name, n_type, n_sect_1based, n_value).
    Returns object bytes.
    """
    nsects = len(sections)
    header_size = 32 + (72 + 80 * nsects) + 24
    blobs = []
    cursor = header_size
    section_rows = []
    for segment, section, size, relocs in sections:
        data_off = cursor
        blobs.append((data_off, bytes(size)))
        cursor += size
        reloc_off = cursor
        reloc_blob = b"".join(struct.pack("<iI", addr, info) for addr, info in relocs)
        blobs.append((reloc_off, reloc_blob))
        cursor += len(reloc_blob)
        section_rows.append((segment, section, size, data_off, reloc_off, len(relocs)))
    names = sorted({name for name, _, _, _ in symbols})
    strtab = b"\0" + b"".join(name.encode() + b"\0" for name in names)
    stroff = cursor
    symoff = cursor + len(strtab)
    strx = {}
    offset = 1
    for name in names:
        strx[name] = offset
        offset += len(name.encode()) + 1
    symtab = b"".join(
        struct.pack("<IBBHq", strx[name], n_type, n_sect, 0, n_value)
        for name, n_type, n_sect, n_value in symbols
    )

    out = bytearray()
    out += struct.pack("<IIIIIIII", MH_MAGIC_64, 0x0100000C, 0, 1, 2, 72 + 80 * nsects + 16, 0, 0)
    out += struct.pack(
        "<II16sQQQQII II".replace(" ", ""),
        0x19, 72 + 80 * nsects,
        b"__DATA", 0, 0, 0, 0, 0, 0, nsects, 0,
    )
    addr = 0x1000
    for (segment, section, size, data_off, reloc_off, nreloc) in section_rows:
        out += struct.pack(
            "<16s16sQQIIIIIIII",
            section.encode(), segment.encode(), addr, size, data_off, 3,
            reloc_off, nreloc, 0, 0, 0, 0,
        )
        addr += size
    out += struct.pack("<IIIIII", 0x2, 24, symoff, len(symbols), stroff, len(strtab))
    assert len(out) == header_size, (len(out), header_size)
    image = bytearray(header_size)
    image[: len(out)] = out
    for blob_off, blob in blobs:
        image[blob_off : blob_off + len(blob)] = blob
    image += strtab + symtab
    return bytes(image)


def section_addr(image, segment, section):
    ncmds = struct.unpack_from("<I", image, 16)[0]
    offset = 32
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", image, offset)
        if cmd == 0x19:
            nsects = struct.unpack_from("<I", image, offset + 64)[0]
            so = offset + 72
            for _i in range(nsects):
                sect, seg, addr, size = struct.unpack_from("<16s16sQQ", image, so)
                if seg.rstrip(b"\0").decode() == segment and sect.rstrip(b"\0").decode() == section:
                    return addr, size
                so += 80
        offset += cmdsize
    raise AssertionError(f"section {segment},{section} missing")


def read_relocs(image, segment, section):
    ncmds = struct.unpack_from("<I", image, 16)[0]
    offset = 32
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", image, offset)
        if cmd == 0x19:
            nsects = struct.unpack_from("<I", image, offset + 64)[0]
            so = offset + 72
            for _i in range(nsects):
                fields = struct.unpack_from("<16s16sQQIIIIIIII", image, so)
                if fields[1].rstrip(b"\0").decode() == segment and fields[0].rstrip(b"\0").decode() == section:
                    return [
                        struct.unpack_from("<iI", image, fields[6] + i * 8)
                        for i in range(fields[7])
                    ]
                so += 80
        offset += cmdsize
    raise AssertionError("relocs missing")


def symbol_values(image):
    ncmds = struct.unpack_from("<I", image, 16)[0]
    offset = 32
    symtab = None
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", image, offset)
        if cmd == 0x2:
            symtab = struct.unpack_from("<IIII", image, offset + 8)
        offset += cmdsize
    symoff, nsyms, stroff, strsize = symtab
    strtab = image[stroff : stroff + strsize]
    values = {}
    for i in range(nsyms):
        n_strx, _n_type, _n_sect, _n_desc, n_value = struct.unpack_from("<IBBHq", image, symoff + i * 16)
        end = strtab.find(b"\0", n_strx)
        values[strtab[n_strx:end].decode()] = n_value
    return values


def write_case(tmp, image, order_lines):
    obj = Path(tmp) / "case.o"
    obj.write_bytes(image)
    order = Path(tmp) / "order.txt"
    order.write_text("\n".join(order_lines) + "\n", encoding="utf-8")
    return str(obj), str(order)


class SchedClassRewriteTests(unittest.TestCase):
    def test_moves_classes_relocations_and_locals(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            # __DATA,__sched_class is section index 1; classes out of order.
            # Distinct reloc identities prove each relocation follows its range.
            image = build_object(
                [("__DATA", "__sched_class", 32, [(0, 0x100), (16, 0x200)])],
                [
                    ("_fair_sched_class", GLOBAL_SECT, 1, 0x1000),
                    ("_stop_sched_class", GLOBAL_SECT, 1, 0x1010),
                    ("fair_helper", LOCAL_SECT, 1, 0x1008),
                ],
            )
            obj, order = write_case(tmp, image, ["_stop_sched_class", "_fair_sched_class"])
            payload = reorder(obj, order)
            self.assertTrue(payload["changed"])
            self.assertEqual(payload["sched"], {"changed": True, "classes": 2})
            values = symbol_values(Path(obj).read_bytes())
            self.assertEqual(values["_stop_sched_class"], 0x1000)
            self.assertEqual(values["_fair_sched_class"], 0x1010)
            self.assertEqual(values["fair_helper"], 0x1018)
            relocs = read_relocs(Path(obj).read_bytes(), "__DATA", "__sched_class")
            self.assertEqual(relocs, [(16, 0x100), (0, 0x200)])

    def test_foreign_section_symbol_numerically_inside_range_is_untouched(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            # __DATA,__other is section index 2; its symbol value falls inside
            # the __sched_class address range but must not be rewritten.
            image = build_object(
                [
                    ("__DATA", "__sched_class", 16, [(0, 0)]),
                    ("__DATA", "__other", 16, []),
                ],
                [
                    ("_stop_sched_class", GLOBAL_SECT, 1, 0x1000),
                    ("_other_anchor", GLOBAL_SECT, 2, 0x1008),
                ],
            )
            obj, order = write_case(tmp, image, ["_stop_sched_class"])
            reorder(obj, order)
            values = symbol_values(Path(obj).read_bytes())
            self.assertEqual(values["_other_anchor"], 0x1008)
            self.assertEqual(values["_stop_sched_class"], 0x1000)

    def test_out_of_range_relocation_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            image = build_object(
                [("__DATA", "__sched_class", 16, [(64, 0)])],
                [("_stop_sched_class", GLOBAL_SECT, 1, 0x1000)],
            )
            obj, order = write_case(tmp, image, ["_stop_sched_class"])
            with self.assertRaises(ReorderError):
                reorder(obj, order)

    def test_missing_unexpected_and_duplicate_classes_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            image = build_object(
                [("__DATA", "__sched_class", 16, [(0, 0)])],
                [("_stop_sched_class", GLOBAL_SECT, 1, 0x1000)],
            )
            obj, order = write_case(tmp, image, ["_stop_sched_class", "_fair_sched_class"])
            with self.assertRaises(ReorderError):
                reorder(obj, order)
            obj, order = write_case(tmp, image, [])
            with self.assertRaises(ReorderError):
                reorder(obj, order)

    def test_already_correct_order_is_idempotent(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            image = build_object(
                [("__DATA", "__sched_class", 32, [(0, 0), (16, 0)])],
                [
                    ("_stop_sched_class", GLOBAL_SECT, 1, 0x1000),
                    ("_fair_sched_class", GLOBAL_SECT, 1, 0x1010),
                ],
            )
            obj, order = write_case(tmp, image, ["_stop_sched_class", "_fair_sched_class"])
            first = reorder(obj, order)
            self.assertFalse(first["changed"])
            before = Path(obj).read_bytes()
            second = reorder(obj, order)
            self.assertFalse(second["changed"])
            self.assertEqual(Path(obj).read_bytes(), before)


class InitcallRewriteTests(unittest.TestCase):
    def test_entries_and_stubs_reorder_together(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            image = build_object(
                [("__DATA", "__initcalls", 24, [(0, 0), (8, 0), (16, 0)])],
                [
                    ("___initcall____late_fn", LOCAL_SECT, 1, 0x1000),
                    ("___initcall____early_fn", LOCAL_SECT, 1, 0x1008),
                    ("___initcall____third_fn", LOCAL_SECT, 1, 0x1010),
                    ("___initcall_start", GLOBAL_SECT, 1, 0x1000),
                ],
            )
            obj, order = write_case(
                tmp,
                image,
                [
                    "___initcall_start",
                    "___initcall____early_fn",
                    "___initcall____late_fn",
                    "___initcall____third_fn",
                ],
            )
            payload = reorder(obj, order)
            self.assertTrue(payload["changed"])
            values = symbol_values(Path(obj).read_bytes())
            self.assertEqual(values["___initcall____early_fn"], 0x1000)
            self.assertEqual(values["___initcall____late_fn"], 0x1008)
            self.assertEqual(values["___initcall____third_fn"], 0x1010)
            self.assertEqual(values["___initcall_start"], 0x1000)
            relocs = read_relocs(Path(obj).read_bytes(), "__DATA", "__initcalls")
            self.assertEqual(sorted(addr for addr, _ in relocs), [0, 8, 16])
            again = reorder(obj, order)
            self.assertFalse(again["changed"])

    def test_foreign_section_entry_like_symbol_is_ignored(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            image = build_object(
                [
                    ("__DATA", "__initcalls", 8, [(0, 0)]),
                    ("__DATA", "__data", 8, []),
                ],
                [
                    ("___initcall____real_fn", LOCAL_SECT, 1, 0x1000),
                    ("___initcall____foreign_fn", LOCAL_SECT, 2, 0x1008),
                ],
            )
            obj, order = write_case(tmp, image, ["___initcall____real_fn"])
            payload = reorder(obj, order)
            self.assertEqual(payload["entries"], 1)
            values = symbol_values(Path(obj).read_bytes())
            self.assertEqual(values["___initcall____foreign_fn"], 0x1008)

    def test_missing_and_unexpected_entries_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            image = build_object(
                [("__DATA", "__initcalls", 8, [(0, 0)])],
                [("___initcall____real_fn", LOCAL_SECT, 1, 0x1000)],
            )
            obj, order = write_case(tmp, image, ["___initcall____real_fn", "___initcall____ghost_fn"])
            with self.assertRaises(ReorderError):
                reorder(obj, order)
            obj, order = write_case(tmp, image, ["___initcall____other_fn"])
            with self.assertRaises(ReorderError):
                reorder(obj, order)

    def test_malformed_entry_relocation_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            image = build_object(
                [("__DATA", "__initcalls", 8, [(4, 0)])],
                [("___initcall____real_fn", LOCAL_SECT, 1, 0x1000)],
            )
            obj, order = write_case(tmp, image, ["___initcall____real_fn"])
            with self.assertRaises(ReorderError):
                reorder(obj, order)

    def test_non_multiple_of_eight_section_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            image = build_object(
                [("__DATA", "__initcalls", 12, [(0, 0)])],
                [("___initcall____real_fn", LOCAL_SECT, 1, 0x1000)],
            )
            obj, order = write_case(tmp, image, ["___initcall____real_fn"])
            with self.assertRaises(ReorderError):
                reorder(obj, order)


if __name__ == "__main__":
    unittest.main()
