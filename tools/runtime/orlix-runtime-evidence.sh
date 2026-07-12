#!/usr/bin/env bash

orlix_runtime_destination_is_simulator() {
	case "$1" in
	iphonesimulator|"iOS Simulator") return 0 ;;
	*) return 1 ;;
	esac
}

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
