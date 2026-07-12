#!/usr/bin/env bash

orlix_latest_runtime_report_for_gate() {
	local report_dir="$1"
	local gate="$2"
	local destination="$3"
	local runtime_identifier="${4:-}"
	local runtime_build="${5:-}"
	local latest=""
	local path

	for path in "$report_dir"/"$gate"-*.json; do
		[ -e "$path" ] || continue
		grep -q "\"destination\"[[:space:]]*:[[:space:]]*\"$destination\"" "$path" || continue
		if [ -n "$runtime_identifier" ] || [ -n "$runtime_build" ]; then
			grep -q "\"simulator_runtime_identifier\"[[:space:]]*:[[:space:]]*\"$runtime_identifier\"" "$path" || continue
			grep -q "\"simulator_runtime_build\"[[:space:]]*:[[:space:]]*\"$runtime_build\"" "$path" || continue
		fi
		if [ -z "$latest" ] || [ "$path" -nt "$latest" ]; then
			latest="$path"
		fi
	done

	[ -n "$latest" ] || return 1
	printf '%s\n' "$latest"
}
