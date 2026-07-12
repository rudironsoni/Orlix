#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

mkdir -p "$tmp/bin" "$tmp/artifacts"
cat >"$tmp/bin/xcrun" <<'SH'
#!/usr/bin/env bash
printf '%s\n' "$@" >"$ORLIX_TEST_XCRUN_ARGS"
exit 7
SH
chmod +x "$tmp/bin/xcrun"

set +e
(
	export PATH="$tmp/bin:$PATH"
	export ORLIX_TEST_XCRUN_ARGS="$tmp/xcrun-args"
	export REPORT_DIR="$tmp/reports"
	export GATE=tcti-mlibc-smoke
	export DESTINATION=iphoneos
	# shellcheck source=../orlix-runtime-validation.sh
	source "$root/tools/runtime/orlix-runtime-validation.sh"
	artifact_dir="$tmp/artifacts"
	destination=iphoneos
	gate=tcti-mlibc-smoke
	device_id=00008130-001E74A11193803A
	bundle_id=com.rudironsoni.Orlix
	capture_seconds=1
	mlibc_smoke_marker=ORLIX-TCTI-MLIBC-OK
	capture_launch
) >"$tmp/stdout" 2>"$tmp/stderr"
status=$?
set -e

[ "$status" -eq 1 ]
[ -f "$tmp/xcrun-args" ] || {
	cat "$tmp/stderr" >&2
	exit 1
}
grep -Fxq -- '--orlix-kernel-command-line-append' "$tmp/xcrun-args"
grep -Fxq 'orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=printf%20ORLIX-TCTI-MLIBC-OK%3B%20exit%200' "$tmp/xcrun-args"
python3 - "$tmp/reports" <<'PY'
import json
import pathlib
import sys

reports = sorted(pathlib.Path(sys.argv[1]).glob("tcti-mlibc-smoke-*.json"))
assert len(reports) == 1, reports
payload = json.loads(reports[0].read_bytes())
assert payload["status"] == "fail"
assert payload["failure_context"]["stage"] == "physical-device-launch"
assert payload["failure_context"]["exit_status"] in (7, "7")
PY

printf 'pass: physical launch forwards gate arguments and reports early devicectl failure\n'
