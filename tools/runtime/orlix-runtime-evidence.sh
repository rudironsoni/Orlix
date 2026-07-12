#!/usr/bin/env bash

orlix_capture_tcti_first_syscall() {
	local output="$1"
	shift
	local inputs=()
	local input
	for input in "$@"; do
		[ -f "$input" ] && inputs+=("$input")
	done
	[ "${#inputs[@]}" -gt 0 ] || return 1
	grep -h -F 'Orlix TCTI: svc #0' "${inputs[@]}" >"$output"
}
