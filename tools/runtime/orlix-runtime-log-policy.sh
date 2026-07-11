#!/bin/sh

orlix_runtime_log_validate() {
	bad=0
	readable=0
	for log in "$@"; do
		[ -r "$log" ] || continue
		readable=1
		LC_ALL=C tr -d '\r' < "$log" | awk '
			BEGIN { bad = 0 }
			/(^|[^[:alnum:]_])not ok[[:space:]]+[0-9]+([[:space:]-]|$)/ {
				print "upstream failure marker: " $0 > "/dev/stderr"
				bad = 1
			}
			/Kernel panic|kernel panic|panic:|Oops|BUG:|Out of memory|oom-killer|Killed process|Attempted (to )?kill init|Assertion .* failed/ {
				print "fatal runtime marker: " $0 > "/dev/stderr"
				bad = 1
			}
			END { exit bad ? 1 : 0 }
		' || bad=1
	done
	if [ "$readable" -eq 0 ]; then
		echo "no readable runtime log was provided" >&2
		return 1
	fi
	return "$bad"
}

orlix_runtime_log_has_failure() {
	if orlix_runtime_log_validate "$@" >/dev/null 2>&1; then
		return 1
	fi
	return 0
}

orlix_runtime_log_observe() {
	duration="$1"
	shift
	elapsed=0
	while [ "$elapsed" -lt "$duration" ]; do
		orlix_runtime_log_has_failure "$@" && return 1
		sleep 1
		elapsed=$((elapsed + 1))
	done
	return 0
}
