"""Read Bazel's zstd compact execution log into JSON spawn records.

``--execution_log_compact_file`` and ``--execution_log_json_file`` are mutually
exclusive in Bazel 9.2. Apple CI keeps the compact log on the BuildBuddy
invocation and projects the spawn records cache observation and promoted
action counts already read from the JSON log.
"""

from __future__ import annotations

import argparse
import ctypes
import json
import shutil
import subprocess
import sys
from pathlib import Path


class CompactExecutionLogError(ValueError):
    pass


_LIBRARY_NAMES = (
    "libzstd.so.1",
    "libzstd.dylib",
    "/opt/homebrew/lib/libzstd.dylib",
    "/usr/local/lib/libzstd.dylib",
)


def _library():
    for name in _LIBRARY_NAMES:
        try:
            library = ctypes.CDLL(name)
        except OSError:
            continue
        library.ZSTD_decompressBound.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        library.ZSTD_decompressBound.restype = ctypes.c_size_t
        library.ZSTD_decompress.argtypes = [
            ctypes.c_void_p,
            ctypes.c_size_t,
            ctypes.c_void_p,
            ctypes.c_size_t,
        ]
        library.ZSTD_decompress.restype = ctypes.c_size_t
        library.ZSTD_isError.argtypes = [ctypes.c_size_t]
        library.ZSTD_isError.restype = ctypes.c_uint
        return library
    return None


def _decompress_library(payload: bytes) -> bytes | None:
    library = _library()
    if library is None:
        return None
    source = ctypes.create_string_buffer(payload)
    bound = library.ZSTD_decompressBound(source, len(payload))
    if library.ZSTD_isError(bound):
        raise CompactExecutionLogError("zstd could not bound the compact execution log")
    if bound > 8 * 1024 * 1024 * 1024:
        raise CompactExecutionLogError("compact execution log decompresses beyond 8 GiB")
    output = ctypes.create_string_buffer(bound)
    size = library.ZSTD_decompress(output, bound, source, len(payload))
    if library.ZSTD_isError(size):
        raise CompactExecutionLogError("zstd could not decompress the compact execution log")
    return output.raw[:size]


def _decompress_cli(payload: bytes) -> bytes | None:
    binary = shutil.which("zstd")
    if binary is None:
        return None
    completed = subprocess.run(
        [binary, "-d", "-c"],
        input=payload,
        check=False,
        capture_output=True,
    )
    if completed.returncode != 0:
        detail = completed.stderr.decode("utf-8", errors="replace").strip()
        raise CompactExecutionLogError(detail or "zstd could not decompress the compact execution log")
    return completed.stdout


def decompress_zstd(payload: bytes) -> bytes:
    cli_bytes = _decompress_cli(payload)
    if cli_bytes is not None:
        return cli_bytes
    library_bytes = _decompress_library(payload)
    if library_bytes is not None:
        return library_bytes
    raise CompactExecutionLogError("zstd is required to read the compact execution log")


def _read_varint(data: bytes, index: int) -> tuple[int, int]:
    value = 0
    shift = 0
    while True:
        if index >= len(data):
            raise CompactExecutionLogError("truncated protobuf varint")
        byte = data[index]
        index += 1
        value |= (byte & 0x7F) << shift
        if byte & 0x80 == 0:
            return value, index
        shift += 7
        if shift > 70:
            raise CompactExecutionLogError("protobuf varint is too long")


def _skip(data: bytes, index: int, wire: int) -> int:
    if wire == 0:
        _, index = _read_varint(data, index)
        return index
    if wire == 1:
        end = index + 8
    elif wire == 2:
        length, index = _read_varint(data, index)
        end = index + length
    elif wire == 5:
        end = index + 4
    else:
        raise CompactExecutionLogError(f"unsupported protobuf wire type {wire}")
    if end > len(data):
        raise CompactExecutionLogError("truncated protobuf field")
    return end


def _length_delimited(data: bytes, index: int) -> tuple[bytes, int]:
    length, index = _read_varint(data, index)
    end = index + length
    if end > len(data):
        raise CompactExecutionLogError("truncated protobuf bytes")
    return data[index:end], end


def _parse_spawn(data: bytes) -> dict[str, object]:
    record: dict[str, object] = {
        "targetLabel": "",
        "mnemonic": "",
        "runner": "",
        "status": "",
        "exitCode": 0,
        "cacheHit": False,
    }
    index = 0
    while index < len(data):
        key, index = _read_varint(data, index)
        field, wire = key >> 3, key & 7
        if field in {7, 8, 10, 11} and wire == 2:
            raw, index = _length_delimited(data, index)
            text = raw.decode("utf-8")
            if field == 7:
                record["targetLabel"] = text
            elif field == 8:
                record["mnemonic"] = text
            elif field == 10:
                record["status"] = text
            else:
                record["runner"] = text
        elif field == 9 and wire == 0:
            value, index = _read_varint(data, index)
            record["exitCode"] = value & 0xFFFFFFFF
            if record["exitCode"] >= 2**31:
                record["exitCode"] -= 2**32
        elif field == 12 and wire == 0:
            value, index = _read_varint(data, index)
            record["cacheHit"] = value != 0
        else:
            index = _skip(data, index, wire)
    return record


def spawn_records(blob: bytes) -> list[dict[str, object]]:
    """Return Spawn records from an uncompressed delimited ExecLogEntry stream."""
    records: list[dict[str, object]] = []
    index = 0
    while index < len(blob):
        entry, index = _length_delimited(blob, index)
        cursor = 0
        while cursor < len(entry):
            key, cursor = _read_varint(entry, cursor)
            field, wire = key >> 3, key & 7
            if field == 7 and wire == 2:
                spawn, cursor = _length_delimited(entry, cursor)
                records.append(_parse_spawn(spawn))
            else:
                cursor = _skip(entry, cursor, wire)
    return records


def records_from_compact(payload: bytes) -> list[dict[str, object]]:
    return spawn_records(decompress_zstd(payload))


def write_execution_json(records: list[dict[str, object]], destination: Path) -> None:
    if not records:
        raise CompactExecutionLogError("compact execution log has no spawn records")
    text = "".join(json.dumps(record, sort_keys=True) + "\n" for record in records)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(text, encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compact", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args(argv)
    source = Path(args.compact)
    try:
        if not source.is_file() or source.stat().st_size == 0:
            raise CompactExecutionLogError(f"compact execution log is missing: {source}")
        write_execution_json(records_from_compact(source.read_bytes()), Path(args.out))
    except (CompactExecutionLogError, OSError, UnicodeError) as error:
        print(f"compact-execution-log: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
