from __future__ import annotations

import ctypes
import json
import tempfile
import unittest
from pathlib import Path

import compact_execution_log as compact


def _varint(value: int) -> bytes:
    out = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        if value:
            out.append(byte | 0x80)
        else:
            out.append(byte)
            return bytes(out)


def _delimited(field: int, payload: bytes) -> bytes:
    return _varint((field << 3) | 2) + _varint(len(payload)) + payload


def _string(field: int, text: str) -> bytes:
    return _delimited(field, text.encode("utf-8"))


def _entry(payload: bytes) -> bytes:
    return _varint(len(payload)) + payload


class CompactExecutionLogTest(unittest.TestCase):
    def test_spawn_projection_matches_json_execution_fields(self) -> None:
        spawn = b"".join(
            (
                _string(1, "/usr/bin/clang"),
                _string(7, "//bazel/feasibility/kernel:uapi"),
                _string(8, "OrlixLinuxHeadersInstall"),
                _varint((9 << 3) | 0) + _varint(0),
                _string(10, ""),
                _string(11, "disk cache hit"),
                _varint((12 << 3) | 0) + _varint(1),
                _delimited(16, _string(1, "abc")),
            )
        )
        other = _entry(_delimited(2, _string(1, "SHA256")))
        blob = other + _entry(_delimited(7, spawn))
        records = compact.spawn_records(blob)
        self.assertEqual(
            records,
            [
                {
                    "targetLabel": "//bazel/feasibility/kernel:uapi",
                    "mnemonic": "OrlixLinuxHeadersInstall",
                    "runner": "disk cache hit",
                    "status": "",
                    "exitCode": 0,
                    "cacheHit": True,
                }
            ],
        )

    def test_zstd_round_trip_writes_json_lines(self) -> None:
        library = compact._library()
        if library is None:
            self.skipTest("libzstd is not available")
        library.ZSTD_compress.argtypes = [
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.c_int,
        ]
        library.ZSTD_compress.restype = ctypes.c_size_t
        library.ZSTD_compressBound.argtypes = [ctypes.c_size_t]
        library.ZSTD_compressBound.restype = ctypes.c_size_t
        spawn = _string(7, "//pkg:coreutils") + _string(8, "OrlixGuestPackage") + _string(11, "local")
        raw = _entry(_delimited(7, spawn))
        source = ctypes.create_string_buffer(raw)
        bound = library.ZSTD_compressBound(len(raw))
        output = ctypes.create_string_buffer(bound)
        size = library.ZSTD_compress(output, bound, source, len(raw), 1)
        self.assertFalse(library.ZSTD_isError(size))
        records = compact.records_from_compact(output.raw[:size])
        self.assertEqual(records[0]["targetLabel"], "//pkg:coreutils")
        self.assertEqual(records[0]["mnemonic"], "OrlixGuestPackage")
        self.assertFalse(records[0]["cacheHit"])
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "execution.json"
            compact.write_execution_json(records, destination)
            decoded = json.loads(destination.read_text(encoding="utf-8"))
            self.assertEqual(decoded["targetLabel"], "//pkg:coreutils")
            self.assertIn("cacheHit", decoded)

    def test_empty_spawn_log_is_rejected(self) -> None:
        with self.assertRaises(compact.CompactExecutionLogError):
            compact.write_execution_json([], Path("unused.json"))


if __name__ == "__main__":
    unittest.main()
