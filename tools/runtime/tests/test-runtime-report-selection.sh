#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../../.." && pwd)"
source "$root/tools/runtime/orlix-runtime-report-selection.sh"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

printf '%s\n' '{"destination":"iphonesimulator","status":"pass"}' >"$tmp/tcti-init-first-syscall-older.json"
sleep 1
printf '%s\n' '{"destination":"iphoneos","status":"fail"}' >"$tmp/tcti-init-first-syscall-newer.json"

selected="$(orlix_latest_runtime_report_for_gate "$tmp" tcti-init-first-syscall iphonesimulator)"
[ "$selected" = "$tmp/tcti-init-first-syscall-older.json" ] || {
	printf 'expected simulator report, got %s\n' "$selected" >&2
	exit 1
}

printf '%s\n' "pass: runtime report selection"

printf '%s\n' '{"destination":"iphonesimulator","simulator_runtime_identifier":"runtime-26","simulator_runtime_build":"26A"}' >"$tmp/tcti-runtime-old.json"
sleep 1
printf '%s\n' '{"destination":"iphonesimulator","simulator_runtime_identifier":"runtime-27","simulator_runtime_build":"27A"}' >"$tmp/tcti-runtime-new.json"

selected="$(orlix_latest_runtime_report_for_gate "$tmp" tcti-runtime iphonesimulator runtime-26 26A)"
[ "$selected" = "$tmp/tcti-runtime-old.json" ] || {
	printf 'expected matching simulator runtime report, got %s\n' "$selected" >&2
	exit 1
}

printf '%s\n' "pass: simulator runtime report selection"
