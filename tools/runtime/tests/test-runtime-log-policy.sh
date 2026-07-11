#!/bin/sh
set -eu

root="$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)"
. "$root/tools/runtime/orlix-runtime-log-policy.sh"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT INT TERM

clean="$tmp/clean.log"
printf '%s\n' 'ORLIX-MLIBC-TEST-END' > "$clean"
orlix_runtime_log_validate "$clean"
if orlix_runtime_log_has_failure "$clean"; then
	echo "clean completion was classified as failure" >&2
	exit 1
fi
if orlix_runtime_log_validate "$tmp/missing.log" >/dev/null 2>&1; then
	echo "missing runtime log was accepted" >&2
	exit 1
fi
orlix_runtime_log_validate "$clean" "$tmp/missing.log"

for marker in \
	'not ok 1 - terminal failure' \
	'Kernel panic - not syncing' \
	'kernel panic' \
	'panic: runtime failure' \
	'Oops: fatal exception' \
	'BUG: failure' \
	'Out of memory' \
	'oom-killer invoked' \
	'Killed process 1' \
	'Attempted to kill init' \
	'Attempted kill init' \
	'Assertion test failed'; do
	failure="$tmp/failure.log"
	printf '%s\n%s\n' 'ORLIX-MLIBC-TEST-END' "$marker" > "$failure"
	if orlix_runtime_log_validate "$failure" >/dev/null 2>&1; then
		echo "fatal marker was accepted: $marker" >&2
		exit 1
	fi
	if ! orlix_runtime_log_has_failure "$failure"; then
		echo "fatal marker was not detected: $marker" >&2
		exit 1
	fi
done

fatal_os="$tmp/fatal-os.log"
printf '%s\r\n' 'BUG: terminal failure' > "$fatal_os"
if orlix_runtime_log_validate "$clean" "$fatal_os" >/dev/null 2>&1; then
	echo "fatal secondary log was accepted" >&2
	exit 1
fi

started="$(date +%s)"
if orlix_runtime_log_observe 5 "$fatal_os"; then
	echo "fatal observation unexpectedly passed" >&2
	exit 1
fi
elapsed=$(( $(date +%s) - started ))
if [ "$elapsed" -ge 2 ]; then
	echo "fatal observation was not prompt: ${elapsed}s" >&2
	exit 1
fi

started="$(date +%s)"
orlix_runtime_log_observe 1 "$clean"
elapsed=$(( $(date +%s) - started ))
if [ "$elapsed" -lt 1 ]; then
	echo "clean observation did not last for the requested interval" >&2
	exit 1
fi

echo "pass: runtime-log-policy"
