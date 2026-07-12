#!/usr/bin/env bash

orlix_capture_tcti_first_syscall() {
	local output="$1"
	shift
	grep -h -F 'Orlix TCTI: svc #0' "$@" >"$output" 2>/dev/null
}
