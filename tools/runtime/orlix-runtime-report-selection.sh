#!/usr/bin/env bash

orlix_latest_runtime_report_for_gate() {
	local report_dir="$1"
	local gate="$2"
	local destination="$3"
	local latest=""
	local path

	for path in "$report_dir"/"$gate"-*.json; do
		[ -e "$path" ] || continue
		grep -q "\"destination\"[[:space:]]*:[[:space:]]*\"$destination\"" "$path" || continue
		if [ -z "$latest" ] || [ "$path" -nt "$latest" ]; then
			latest="$path"
		fi
	done

	[ -n "$latest" ] || return 1
	printf '%s\n' "$latest"
}
