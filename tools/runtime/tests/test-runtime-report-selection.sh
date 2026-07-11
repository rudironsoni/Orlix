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
