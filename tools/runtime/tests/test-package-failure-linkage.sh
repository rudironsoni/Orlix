#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$root"
token="package-failure-linkage-$$-$RANDOM"
report_root="$root/Build/Reports/runtime/$token"
build_root="$root/Build/TCTI/tests/$token"
mkdir -p "$report_root" "$build_root"
trap 'rm -rf "$report_root" "$build_root"' EXIT

run_failure() {
	local output_dir="$1"
	local build_root="$2"
	local stage="${3:-simulator-launch}"
	local prefinalization_check="${4:-0}"
	set +e
	(
		export REPORT_DIR="${output_dir#"$root/"}"
		export ORLIX_TCTI_BUILD_ROOT="$build_root"
		export GATE=tcti-package-behavior
		export DESTINATION=iphonesimulator
		# shellcheck source=../orlix-runtime-validation.sh
		source "$root/tools/runtime/orlix-runtime-validation.sh"
		failure_stage="$stage"
		failure_kind="command-failure"
		failure_exit_status="1"
		failure_timeout_seconds="45"
		product_launch_attempted="false"
		simulator_runtime_identifier="iOS-26-5"
		simulator_runtime_version="26.5"
		simulator_runtime_build="26F1"
		if [ "$prefinalization_check" = "1" ]; then
			if write_runtime_failure_reducer "failure before report finalization"; then
				printf 'pre-finalization reducer was emitted\n' >&2
				exit 1
			fi
			printf 'pre-finalization reducer rejected\n'
		fi
		die "unstable explanatory prose is not fingerprint input"
	) >"$output_dir/stdout" 2>"$output_dir/stderr"
	local status=$?
	set -e
	[ "$status" -eq 1 ] || {
		cat "$output_dir/stderr" >&2
		return 1
	}
}

mkdir -p "$report_root/first" "$report_root/second" "$report_root/changed-input"
run_failure "$report_root/first" "$build_root/first" simulator-launch 1
grep -F -q 'pre-finalization reducer rejected' "$report_root/first/stdout"

run_failure "$report_root/second" "$build_root/second"
run_failure "$report_root/changed-input" "$build_root/changed-input" package-launch

python3 - "$report_root" "$build_root" "$root" <<'PY'
import hashlib
import json
import pathlib
import sys

report_root = pathlib.Path(sys.argv[1])
build_root = pathlib.Path(sys.argv[2])
repo_root = pathlib.Path(sys.argv[3])

def one_report(directory):
    reports = sorted(directory.glob("tcti-package-behavior-*.json"))
    assert len(reports) == 1, reports
    return reports[0]

def one_reducer(build_root):
    reducers = sorted(build_root.glob("reproducers/simulator-tcti-package-behavior/*.json"))
    assert len(reducers) == 1, reducers
    return reducers[0]

report = one_report(report_root / "first")
reducer = one_reducer(build_root / "first")
payload = json.loads(report.read_bytes())
reducer_payload = json.loads(reducer.read_bytes())
assert payload["status"] == "fail"
assert len(payload["failures"]) == 1
failure = payload["failures"][0]
assert failure["id"]
assert failure["fingerprint"]
assert failure["message"]
source_failure = reducer_payload["source_failure"]
assert source_failure["report_path"] == str(report.relative_to(repo_root))
assert source_failure["report_sha256"] == hashlib.sha256(report.read_bytes()).hexdigest()
assert source_failure["failure_id"] == failure["id"]
assert source_failure["failure_fingerprint"] == failure["fingerprint"]
assert reducer_payload["replay_outcome"] == "not_run"
assert "runtime_patch_allowed" not in payload
assert "runtime_patch_allowed" not in reducer_payload

second_report = one_report(report_root / "second")
changed_report = one_report(report_root / "changed-input")
assert json.loads(second_report.read_bytes())["failures"][0]["fingerprint"] == failure["fingerprint"]
assert json.loads(changed_report.read_bytes())["failures"][0]["fingerprint"] != failure["fingerprint"]

tampered = json.loads(report.read_bytes())
tampered["summary"] = "changed source content"
tampered_bytes = json.dumps(tampered, indent=2, sort_keys=True).encode() + b"\n"
assert hashlib.sha256(tampered_bytes).hexdigest() != source_failure["report_sha256"]
print("pass: package failure report linkage, digest, fingerprint, and fail-closed reducer ordering")
PY
