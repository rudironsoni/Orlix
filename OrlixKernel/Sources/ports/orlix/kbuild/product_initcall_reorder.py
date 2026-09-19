#!/usr/bin/env python3
"""Reorder merged Mach-O __initcalls entries to the expected Linux order.

The relocatable product link merges every Linux initcall level section into
one __DATA,__initcalls section and the product Xcode linker no longer honors
order files, so entries from objects that register initcalls at more than one
level cannot be placed by input order alone. This tool permutes the merged
section's 8-byte entries, their relocations, and the entry symbols to match
the expected order, and repositions level-start boundary labels ahead of their
level's first entry. Never invents entries; fails loudly on any structure it
does not recognize.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import struct
import sys
import tempfile
from pathlib import Path

MH_MAGIC_64 = 0xFEEDFACF
LC_SEGMENT_64 = 0x19
LC_SYMTAB = 0x2
SECTION_HEADER = struct.Struct("<16s16sQQIIIIIIII")
NLIST_64 = struct.Struct("<IBBHq")
N_SECT = 0xE
N_EXT = 0x1


class ReorderError(ValueError):
    pass


def _mach_o_sections_and_symtab(data: bytes):
    magic = struct.unpack_from("<I", data, 0)[0]
    if magic != MH_MAGIC_64:
        raise ReorderError(f"not a 64-bit Mach-O object: magic {magic:#x}")
    ncmds = struct.unpack_from("<I", data, 16)[0]
    offset = 32
    sections = []
    symtab = None
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from("<II", data, offset)
        if cmd == LC_SEGMENT_64:
            nsects = struct.unpack_from("<I", data, offset + 64)[0]
            so = offset + 72
            for _i in range(nsects):
                sect, seg, addr, size, offset_, _align, reloff, nreloc, _flags, _r1, _r2, _r3 = SECTION_HEADER.unpack_from(data, so)
                sections.append(
                    {
                        "section": sect.rstrip(b"\0").decode(),
                        "segment": seg.rstrip(b"\0").decode(),
                        "addr": addr,
                        "size": size,
                        "offset": offset_,
                        "reloff": reloff,
                        "nreloc": nreloc,
                    }
                )
                so += SECTION_HEADER.size
        elif cmd == LC_SYMTAB:
            symoff, nsyms, stroff, strsize = struct.unpack_from("<IIII", data, offset + 8)
            symtab = {"symoff": symoff, "nsyms": nsyms, "stroff": stroff, "strsize": strsize}
        offset += cmdsize
    if symtab is None:
        raise ReorderError("object has no symbol table")
    return sections, symtab


def _load_symbols(data: bytes, symtab: dict):
    strtab = data[symtab["stroff"] : symtab["stroff"] + symtab["strsize"]]
    symbols = []
    for i in range(symtab["nsyms"]):
        n_strx, n_type, n_sect, _n_desc, n_value = NLIST_64.unpack_from(data, symtab["symoff"] + i * 16)
        end = strtab.find(b"\0", n_strx)
        symbols.append({"index": i, "name": strtab[n_strx:end].decode(), "type": n_type, "sect": n_sect, "value": n_value})
    return symbols


def reorder(object_path: str, order_path: str) -> dict:
    data = bytearray(Path(object_path).read_bytes())
    original = bytes(data)
    sections, symtab = _mach_o_sections_and_symtab(bytes(data))
    wanted = [line.strip() for line in Path(order_path).read_text(encoding="utf-8").splitlines() if line.strip()]
    if not wanted:
        raise ReorderError("empty expected order")
    target = next((s for s in sections if (s["segment"], s["section"]) == ("__DATA", "__initcalls")), None)
    symbols = _load_symbols(bytes(data), symtab)
    initcalls_sect_indexes = {
        index for index, section in enumerate(sections, start=1)
        if (section["segment"], section["section"]) == ("__DATA", "__initcalls")
    }
    if target is None:
        if any(s["name"].startswith("___initcall____") for s in symbols):
            raise ReorderError("object holds initcall entries but no __DATA,__initcalls section")
        entries_now = []
        stubs_now = []
        entry_count = 0
    else:
        if target["size"] % 8:
            raise ReorderError("__initcalls section is not a multiple of 8 bytes")
        entry_count = target["size"] // 8
        inside = [
            s for s in symbols
            if (s["type"] & 0x0E) == N_SECT and s["sect"] in initcalls_sect_indexes
        ]
        inside.sort(key=lambda s: s["value"])
        entries_now = [
            s for s in inside
            if s["name"].startswith("___initcall____") and not s["type"] & N_EXT
        ]
        stubs_now = [
            s for s in inside
            if not s["name"].startswith("___initcall____")
            and s["type"] & N_EXT
            and not re.fullmatch(r"ltmp[0-9]+", s["name"])
        ]
    entries_wanted = [name for name in wanted if name.startswith("___initcall____")]
    stubs_wanted = [
        name for name in wanted
        if name.startswith("___initcall") and not name.startswith("___initcall____")
    ]
    sched_wanted = [name for name in wanted if not name.startswith("___initcall")]
    if target:
        valid_offsets = {s["value"] - target["addr"] for s in entries_now}
        for i in range(target["nreloc"]):
            base = target["reloff"] + i * 8
            r_address, _r_info = struct.unpack_from("<iI", data, base)
            if r_address < 0 or r_address >= target["size"] or r_address % 8:
                raise ReorderError(f"__initcalls relocation at {r_address} is not an 8-byte entry offset")
            if r_address not in valid_offsets:
                raise ReorderError(f"unexpected __initcalls relocation at {r_address}")
    if [s["name"] for s in entries_now] == entries_wanted and not sched_wanted:
        return {"changed": False, "entries": entry_count, "stubs": len(stubs_now)}

    if not entries_now and entries_wanted:
        raise ReorderError("expected initcall entries but the object holds none")
    if entries_now and len(entries_now) != entry_count:
        raise ReorderError(f"__initcalls holds {entry_count} entries but {len(entries_now)} entry symbols")
    by_name = {s["name"]: s for s in entries_now}
    unexpected = [s["name"] for s in entries_now if s["name"] not in wanted]
    if unexpected:
        raise ReorderError(f"object holds initcall entries absent from the expected order: {unexpected[:4]}")
    missing = [name for name in wanted if name.startswith("___initcall____") and name not in by_name]
    if missing:
        raise ReorderError(f"expected initcall entries missing from the object: {missing[:4]}")

    old_offsets = [s["value"] - target["addr"] for s in entries_now] if target else []
    old_to_new = {by_name[name]["value"] - target["addr"]: position * 8 for position, name in enumerate(entries_wanted)} if target else {}
    if any(offset % 8 for offset in old_to_new):
        raise ReorderError("entry offsets are not 8-byte aligned with the expected order")

    if target:
        section_base = target["offset"]
        reordered = bytearray(target["size"])
        for old_offset, new_offset in old_to_new.items():
            reordered[new_offset : new_offset + 8] = data[section_base + old_offset : section_base + old_offset + 8]
        data[section_base : section_base + target["size"]] = reordered

        for i in range(target["nreloc"]):
            base = target["reloff"] + i * 8
            r_address, r_info = struct.unpack_from("<iI", data, base)
            if r_address < 0 or r_address >= target["size"] or r_address % 8:
                raise ReorderError(f"__initcalls relocation at {r_address} is not an 8-byte entry offset")
            new_address = old_to_new.get(r_address)
            if new_address is None:
                raise ReorderError(f"unexpected __initcalls relocation at {r_address}")
            struct.pack_into("<iI", data, base, new_address, r_info)

    stub_targets = {}
    for position, name in enumerate(wanted):
        if name not in stubs_wanted:
            continue
        following = next(
            (entry for entry in wanted[position + 1 :] if entry in entries_wanted),
            None,
        )
        stub_targets[name] = (
            target["addr"] + old_to_new[by_name[following]["value"] - target["addr"]]
            if following is not None
            else target["addr"] + target["size"]
        )
    if set(stub_targets) != {s["name"] for s in stubs_now}:
        raise ReorderError(
            "expected order boundary labels do not match the object's initcall stubs: "
            f"expected {sorted(stub_targets)} found {sorted(s['name'] for s in stubs_now)}"
        )
    if target:
        for symbol in inside:
            base = symtab["symoff"] + symbol["index"] * 16
            if symbol["name"].startswith("___initcall____"):
                struct.pack_into("<q", data, base + 8, target["addr"] + old_to_new[symbol["value"] - target["addr"]])
            elif re.fullmatch(r"ltmp[0-9]+", symbol["name"]):
                continue
            else:
                struct.pack_into("<q", data, base + 8, stub_targets[symbol["name"]])

    sched_payload = {"changed": False, "classes": 0}
    if sched_wanted:
        sched_payload = _reorder_sched_class(data, sections, symtab, sched_wanted)
    else:
        sched_section = next((s for s in sections if (s["segment"], s["section"]) == ("__DATA", "__sched_class")), None)
        if sched_section is not None:
            sched_symbols = _load_symbols(bytes(data), symtab)
            sched_index = next(
                index for index, section in enumerate(sections, start=1)
                if (section["segment"], section["section"]) == ("__DATA", "__sched_class")
            )
            sched_inside = [
                s for s in sched_symbols
                if (s["type"] & 0x0E) == N_SECT
                and s["name"].endswith("_sched_class")
                and s["sect"] == sched_index
                and sched_section["addr"] <= s["value"] < sched_section["addr"] + sched_section["size"]
            ]
            if sched_inside:
                raise ReorderError(
                    "linked object holds sched classes but the expected order lists none: "
                    f"{sorted(s['name'] for s in sched_inside)[:4]}"
                )

    if bytes(data) == original:
        return {"changed": False, "entries": entry_count, "stubs": len(stub_targets), "sched": sched_payload}
    with tempfile.NamedTemporaryFile(delete=False, dir=os.path.dirname(os.path.abspath(object_path))) as handle:
        handle.write(bytes(data))
        temp_path = handle.name
    os.replace(temp_path, object_path)
    return {"changed": True, "entries": entry_count, "stubs": len(stub_targets), "sched": sched_payload}


def _reorder_sched_class(data: bytearray, sections: dict, symtab: dict, sched_wanted: list) -> dict:
    sched_section = next((s for s in sections if (s["segment"], s["section"]) == ("__DATA", "__sched_class")), None)
    if sched_section is None:
        raise ReorderError("expected sched class order but the object has no __DATA,__sched_class section")
    sched_index = next(
        index for index, section in enumerate(sections, start=1)
        if (section["segment"], section["section"]) == ("__DATA", "__sched_class")
    )
    symbols = _load_symbols(bytes(data), symtab)
    classes_now = [
        s for s in symbols
        if (s["type"] & 0x0E) == N_SECT
        and s["type"] & N_EXT
        and s["name"].endswith("_sched_class")
        and s["sect"] == sched_index
        and sched_section["addr"] <= s["value"] < sched_section["addr"] + sched_section["size"]
    ]
    classes_now.sort(key=lambda s: s["value"])
    if sorted(s["name"] for s in classes_now) != sorted(sched_wanted):
        raise ReorderError(
            "expected sched class order does not match the object's class set: "
            f"object {sorted(s['name'] for s in classes_now)} expected {sorted(sched_wanted)}"
        )
    by_name = {s["name"]: s for s in classes_now}
    boundaries = [s["value"] for s in classes_now] + [sched_section["addr"] + sched_section["size"]]
    ranges = []
    for position, symbol in enumerate(classes_now):
        start = symbol["value"] - sched_section["addr"]
        end = boundaries[position + 1] - sched_section["addr"]
        ranges.append((symbol["name"], start, end))
    for i in range(sched_section["nreloc"]):
        base = sched_section["reloff"] + i * 8
        r_address, _r_info = struct.unpack_from("<iI", data, base)
        if r_address < 0 or r_address >= sched_section["size"]:
            raise ReorderError(f"__sched_class relocation at {r_address} escapes the section")
        if not any(start <= r_address < end for _, start, end in ranges):
            raise ReorderError(f"__sched_class relocation at {r_address} belongs to no sched class range")
    if [s["name"] for s in classes_now] == sched_wanted:
        return {"changed": False, "classes": len(classes_now)}

    old_to_new = {}
    new_cursor = 0
    for name in sched_wanted:
        for class_name, start, end in ranges:
            if class_name == name:
                old_to_new[(start, end)] = (new_cursor, new_cursor + (end - start))
                new_cursor += end - start
                break
        else:
            raise ReorderError(f"expected sched class missing from the object: {name}")

    section_base = sched_section["offset"]
    reordered = bytearray(sched_section["size"])
    for (start, end), (new_start, new_end) in old_to_new.items():
        reordered[new_start:new_end] = data[section_base + start : section_base + end]
    data[section_base : section_base + sched_section["size"]] = reordered

    for i in range(sched_section["nreloc"]):
        base = sched_section["reloff"] + i * 8
        r_address, r_info = struct.unpack_from("<iI", data, base)
        if r_address < 0 or r_address >= sched_section["size"]:
            raise ReorderError(f"__sched_class relocation at {r_address} escapes the section")
        for (start, end), (new_start, new_end) in old_to_new.items():
            if start <= r_address < end:
                struct.pack_into("<iI", data, base, new_start + (r_address - start), r_info)
                break
        else:
            raise ReorderError(f"__sched_class relocation at {r_address} belongs to no sched class range")

    for symbol in symbols:
        if (symbol["type"] & 0x0E) != N_SECT:
            continue
        if symbol["sect"] != sched_index:
            continue
        offset = symbol["value"] - sched_section["addr"]
        for (start, end), (new_start, _new_end) in old_to_new.items():
            if start <= offset < end:
                base = symtab["symoff"] + symbol["index"] * 16
                struct.pack_into("<q", data, base + 8, sched_section["addr"] + new_start + (offset - start))
                break
    return {"changed": True, "classes": len(classes_now)}


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--object", required=True)
    parser.add_argument("--order", required=True)
    args = parser.parse_args(argv)
    payload = reorder(args.object, args.order)
    print(json.dumps(payload, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
