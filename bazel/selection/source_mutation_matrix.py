"""Executable source-mutation matrix.

A row is PASS only after this process records compiler invocations or action-key
bytes. Rows that were not executed stay UNVERIFIED. This report has no timings.
"""

from __future__ import annotations

from bazel.feasibility.packages.test_action_inputs import observe_unrelated_package_action_key
from bazel.feasibility.test_compiler_cache_action import observe_compiler_cache
from bazel.selection.source_mutation_exec import observe_cache_layers, observe_source_edits


COMPILER_CACHE_ROWS = (
    "compiler_cache_unchanged_second_compile",
    "compiler_cache_changed_header",
    "compiler_cache_changed_optimization",
    "compiler_cache_corrupt_entry",
    "compiler_cache_missing_cache",
)

EDIT_ROWS = (
    "kernel_c_closure",
    "swift_unrelated_edit",
    "mlibc_edit_keeps_kernel",
    "guest_package_edit_keeps_kernel_and_mlibc",
)

# These stay UNVERIFIED. Nothing in this module executes them.
UNVERIFIED_ROWS = (
    ("kernel_kbuild_macos", "full kernel Kbuild on macOS is not executed"),
    ("swift_module_rebuild", "Swift module rebuilds are not executed"),
    ("orlix_engine", "OrlixEngine rebuild is not executed"),
    ("orlix_bootloader", "OrlixBootloader rebuild is not executed"),
    ("orlix_host_adapter", "OrlixHostAdapter rebuild is not executed"),
    ("installed_uapi", "installed UAPI rebuild is not executed"),
    ("mlibc_rebuild", "mlibc rebuild is not executed"),
    ("rootfs_policy", "rootfs policy rebuild is not executed"),
)


def _unverified(row_id: str, reason: str) -> dict[str, object]:
    return {"id": row_id, "status": "UNVERIFIED", "reason": reason}


def _fail(row_id: str, reason: str) -> dict[str, object]:
    return {"id": row_id, "status": "FAIL", "reason": reason}


def _compiler_cache_rows() -> list[dict[str, object]]:
    try:
        observed = observe_compiler_cache()
    except (AssertionError, OSError) as error:
        return [_fail(row_id, error.__class__.__name__) for row_id in COMPILER_CACHE_ROWS]
    if observed is None:
        return [
            _unverified(row_id, "ccache or C compiler was not executed")
            for row_id in COMPILER_CACHE_ROWS
        ]
    cases = observed["cases"]
    if not isinstance(cases, dict):
        return [_fail(row_id, "compiler cache observation has no cases") for row_id in COMPILER_CACHE_ROWS]
    unchanged = cases["compiler_cache_unchanged_second_compile"]
    header = cases["compiler_cache_changed_header"]
    flagged = cases["compiler_cache_changed_optimization"]
    corrupt = cases["compiler_cache_corrupt_entry"]
    missing = cases["compiler_cache_missing_cache"]
    rows = [
        _compiler_row(
            "compiler_cache_unchanged_second_compile",
            unchanged["invocations"] >= 2
            and unchanged["cold_hits"] == 0
            and unchanged["cold_misses"] >= 1
            and unchanged["objects_equal"] is True
            and unchanged["hits"] >= 1,
            unchanged,
            "cold_hits",
            "cold_misses",
            "hits",
            "misses",
        ),
        _compiler_row(
            "compiler_cache_changed_header",
            header["invocations"] >= 1
            and header["object_differs"] is True
            and header["misses"] > unchanged["misses"]
            and header["hits"] == unchanged["hits"],
            header,
            "hits",
            "misses",
        ),
        _compiler_row(
            "compiler_cache_changed_optimization",
            flagged["invocations"] >= 1
            and flagged["object_differs"] is True
            and flagged["misses"] > header["misses"],
            flagged,
            "hits",
            "misses",
        ),
        _compiler_row(
            "compiler_cache_corrupt_entry",
            corrupt["invocations"] >= 1
            and corrupt["object_matches"] is True
            and corrupt["entry_rewritten"] is True,
            corrupt,
            "object_matches",
            "entry_rewritten",
        ),
        _compiler_row(
            "compiler_cache_missing_cache",
            missing["invocations"] >= 1
            and missing["object_matches"] is True
            and missing["cache_created"] is True,
            missing,
            "object_matches",
            "cache_created",
        ),
    ]
    for row in rows:
        if row["status"] == "PASS" and int(row["invocations"]) < 1:
            row["status"] = "FAIL"
            row["reason"] = "no compiler invocation was recorded"
    return rows


def _compiler_row(
    row_id: str,
    passed: bool,
    case: dict[str, object],
    *fields: str,
) -> dict[str, object]:
    row: dict[str, object] = {
        "id": row_id,
        "status": "PASS" if passed else "FAIL",
        "invocations": case["invocations"],
    }
    for field in fields:
        row[field] = case[field]
    if not passed:
        row["reason"] = "observed compiler cache did not match the closure"
    return row


def _guest_package_row() -> dict[str, object]:
    try:
        observed = observe_unrelated_package_action_key()
    except (AssertionError, OSError) as error:
        return _fail("guest_package_unrelated_input", error.__class__.__name__)
    passed = (
        int(observed["source_key_invocations"]) >= 3
        and observed["fail_closed"] is True
        and observed["foreign_edit_changes_action_key"] is False
        and observed["owned_edit_changes_action_key"] is True
        and observed["included_foreign_changes_key"] is True
        and observed["foreign_digest_in_stamp"] is False
        and observed["owned_digest_in_stamp"] is True
        and observed["observed_paths"] == observed["expected_paths"]
        and bool(observed["observed_paths"])
    )
    row: dict[str, object] = {
        "id": "guest_package_unrelated_input",
        "status": "PASS" if passed else "FAIL",
        "invocations": observed["source_key_invocations"],
        "selector_calls": observed["selector_calls"],
        "expected_paths": observed["expected_paths"],
        "observed_paths": observed["observed_paths"],
        "rejected_paths": observed["rejected_paths"],
        "fail_closed": observed["fail_closed"],
        "foreign_digest_in_stamp": observed["foreign_digest_in_stamp"],
        "owned_digest_in_stamp": observed["owned_digest_in_stamp"],
        "foreign_edit_changes_action_key": observed["foreign_edit_changes_action_key"],
        "owned_edit_changes_action_key": observed["owned_edit_changes_action_key"],
        "included_foreign_changes_key": observed["included_foreign_changes_key"],
        "stamp_sha256": observed["stamp_sha256"],
    }
    if not passed:
        row["reason"] = "unrelated package input changed the action key or was not executed"
    return row


def _key_changed(case: dict[str, object], component: str) -> bool:
    keys = case["keys"]
    baseline = case["baseline"]
    if not isinstance(keys, dict) or not isinstance(baseline, dict):
        raise AssertionError("edit observation has no action keys")
    return keys[component] != baseline[component]


def _edit_rows() -> list[dict[str, object]]:
    try:
        observed = observe_source_edits()
    except (AssertionError, OSError) as error:
        return [_fail(row_id, error.__class__.__name__) for row_id in EDIT_ROWS]
    if observed.get("available") is not True:
        return [_unverified(row_id, "C compiler, make, or ar was not executed") for row_id in EDIT_ROWS]
    swift = observed["swift"]
    mlibc = observed["mlibc"]
    guest = observed["guest"]
    kernel = observed["kernel"]
    if not all(isinstance(case, dict) for case in (swift, mlibc, guest, kernel)):
        return [_fail(row_id, "edit observation has no cases") for row_id in EDIT_ROWS]
    invocations = int(observed["source_key_invocations"])
    kernel_closed = (
        invocations >= 1
        and kernel["rebuilt"]
        and not kernel["leaked"]
        and kernel["compiler_lines"]
        and kernel["changed_object_nonempty"] is True
        and kernel["archive_changed"] is True
        and kernel["kept_same"] is True
        and _key_changed(kernel, "kernel")
        and not _key_changed(kernel, "mlibc")
        and not _key_changed(kernel, "guest")
        and not _key_changed(kernel, "ssh")
        and not _key_changed(kernel, "swift")
    )
    swift_isolated = (
        invocations >= 1
        and not swift["compiler_lines"]
        and _key_changed(swift, "swift")
        and not _key_changed(swift, "kernel")
        and not _key_changed(swift, "mlibc")
        and not _key_changed(swift, "guest")
        and not _key_changed(swift, "ssh")
    )
    mlibc_isolated = (
        invocations >= 1
        and not mlibc["compiler_lines"]
        and _key_changed(mlibc, "mlibc")
        and not _key_changed(mlibc, "kernel")
    )
    guest_isolated = (
        invocations >= 1
        and not guest["compiler_lines"]
        and _key_changed(guest, "guest")
        and not _key_changed(guest, "kernel")
        and not _key_changed(guest, "mlibc")
    )
    rows = [
        {
            "id": "kernel_c_closure",
            "status": "PASS" if kernel_closed else "FAIL",
            "invocations": len(kernel["compiler_lines"]),
            "source_key_invocations": invocations,
            "rebuilt": ",".join(kernel["rebuilt"]),
            "kept_objects": ",".join(kernel["kept_objects"]),
            "leaked": ",".join(kernel["leaked"]),
            "kernel_key_changed": _key_changed(kernel, "kernel"),
            "mlibc_key_changed": _key_changed(kernel, "mlibc"),
            "guest_key_changed": _key_changed(kernel, "guest"),
            "ssh_key_changed": _key_changed(kernel, "ssh"),
            "swift_key_changed": _key_changed(kernel, "swift"),
        },
        {
            "id": "swift_unrelated_edit",
            "status": "PASS" if swift_isolated else "FAIL",
            "invocations": invocations,
            "compiler_invocations": len(swift["compiler_lines"]),
            "swift_key_changed": _key_changed(swift, "swift"),
            "kernel_key_changed": _key_changed(swift, "kernel"),
            "mlibc_key_changed": _key_changed(swift, "mlibc"),
            "guest_key_changed": _key_changed(swift, "guest"),
            "ssh_key_changed": _key_changed(swift, "ssh"),
        },
        {
            "id": "mlibc_edit_keeps_kernel",
            "status": "PASS" if mlibc_isolated else "FAIL",
            "invocations": invocations,
            "compiler_invocations": len(mlibc["compiler_lines"]),
            "mlibc_key_changed": _key_changed(mlibc, "mlibc"),
            "kernel_key_changed": _key_changed(mlibc, "kernel"),
        },
        {
            "id": "guest_package_edit_keeps_kernel_and_mlibc",
            "status": "PASS" if guest_isolated else "FAIL",
            "invocations": invocations,
            "compiler_invocations": len(guest["compiler_lines"]),
            "guest_key_changed": _key_changed(guest, "guest"),
            "kernel_key_changed": _key_changed(guest, "kernel"),
            "mlibc_key_changed": _key_changed(guest, "mlibc"),
        },
    ]
    for row in rows:
        if row["status"] == "FAIL":
            row["reason"] = "recorded invocations did not match the closure"
    return rows


def _cache_layer_row() -> dict[str, object]:
    row_id = "fresh_runner_buildbuddy_hit"
    try:
        observed = observe_cache_layers()
    except (AssertionError, OSError) as error:
        return _fail(row_id, error.__class__.__name__)
    if observed is None:
        return _unverified(row_id, "ccache or C compiler was not executed")
    ccache = observed["ccache"]
    disk = observed["disk"]
    fresh = observed["fresh"]
    if not all(isinstance(case, dict) for case in (ccache, disk, fresh)):
        return _fail(row_id, "cache observation has no layers")
    layers = (ccache["layer"], disk["layer"], fresh["layer"])
    passed = (
        ccache["cold_hits"] == 0
        and ccache["cold_misses"] >= 1
        and ccache["hits"] >= 1
        and ccache["compiler_invocations_on_miss"] >= 1
        and ccache["compiler_invocations_after_hit"] == ccache["compiler_invocations_on_miss"]
        and ccache["objects_equal"] is True
        and ccache["layer"] == "ccache"
        and disk["reads"] == 1
        and disk["remote_reads"] == 0
        and disk["compiler_invocations"] == 0
        and disk["object_matches"] is True
        and disk["layer"] == "disk cache hit"
        and fresh["ccache_hits"] == 0
        and fresh["disk_reads"] == 0
        and fresh["local_store_reads"] == 1
        and fresh["remote_reads"] == 0
        and fresh["opened_endpoint"] is False
        and fresh["live_remote_hit"] is False
        and fresh["compiler_invocations"] == 0
        and fresh["object_matches"] is True
        and fresh["local_disk_empty"] is True
        and fresh["layer"] == "local store read"
        and len(set(layers)) == 3
        and "buildbuddy" in str(observed["remote_cache_url"])
    )
    row: dict[str, object] = {
        "id": row_id,
        "status": "PASS" if passed else "FAIL",
        "invocations": int(fresh["local_store_reads"]) + int(disk["reads"]) + int(ccache["hits"]),
        "remote_cache_url": observed["remote_cache_url"],
        "fresh_layer": fresh["layer"],
        "fresh_runner": False,
        "live_remote_hit": fresh["live_remote_hit"],
        "opened_endpoint": fresh["opened_endpoint"],
        "local_store_reads": fresh["local_store_reads"],
        "fresh_ccache_hits": fresh["ccache_hits"],
        "fresh_disk_reads": fresh["disk_reads"],
        "fresh_remote_reads": fresh["remote_reads"],
        "fresh_compiler_invocations": fresh["compiler_invocations"],
        "ccache_layer": ccache["layer"],
        "ccache_hits": ccache["hits"],
        "ccache_compiler_invocations_on_hit": ccache["compiler_invocations_after_hit"] - ccache["compiler_invocations_on_miss"],
        "disk_layer": disk["layer"],
        "disk_reads": disk["reads"],
        "disk_remote_reads": disk["remote_reads"],
        "disk_compiler_invocations": disk["compiler_invocations"],
        "layers_distinct": len(set(layers)) == 3,
    }
    if not passed:
        row["reason"] = "cache layers were not distinct or were not executed"
    return row


def run_source_mutation_matrix() -> list[dict[str, object]]:
    rows = [*_compiler_cache_rows(), _guest_package_row(), *_edit_rows(), _cache_layer_row()]
    rows.extend(_unverified(row_id, reason) for row_id, reason in UNVERIFIED_ROWS)
    return rows


def format_report(rows: list[dict[str, object]]) -> str:
    lines = []
    for row in rows:
        parts = [f"source-mutation-matrix {row['id']} {row['status']}"]
        for key in sorted(name for name in row if name not in {"id", "status"}):
            parts.append(f"{key}={_format_value(row[key])}")
        lines.append(" ".join(parts))
    return "\n".join(lines) + "\n"


def _format_value(value: object) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    text = str(value)
    if any(character.isspace() for character in text):
        return '"' + text.replace('"', "'") + '"'
    return text


def main() -> int:
    rows = run_source_mutation_matrix()
    print(format_report(rows), end="", flush=True)
    return 1 if any(row["status"] == "FAIL" for row in rows) else 0


if __name__ == "__main__":
    raise SystemExit(main())
