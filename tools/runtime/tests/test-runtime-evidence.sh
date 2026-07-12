#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "$0")/../../.." && pwd)"
source "$root/tools/runtime/orlix-runtime-evidence.sh"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

orlix_runtime_destination_is_simulator iphonesimulator
orlix_runtime_destination_is_simulator "iOS Simulator"
if orlix_runtime_destination_is_simulator iphoneos; then
	echo "physical destination was classified as simulator" >&2
	exit 1
fi

touch "$tmp/launch-console.log" "$tmp/launch.log"
marker='Orlix TCTI: svc #0 task=init pid=1 pc=0x700d1caa98 syscall=178 x0=0xb2'
printf '%s\n' "$marker" >"$tmp/launch.stderr"

orlix_capture_tcti_first_syscall \
	"$tmp/tcti-first-syscall.txt" \
	"$tmp/launch-console.log" \
	"$tmp/launch.log" \
	"$tmp/launch.stderr" \
	"$tmp/simulator-terminal-output.txt" \
	"$tmp/simulator-unified.log"

grep -Fxq "$marker" "$tmp/tcti-first-syscall.txt"
printf '%s\n' 'pass: runtime physical first-syscall evidence'
