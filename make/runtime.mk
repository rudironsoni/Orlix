# Shared runtime and payload Make implementation. This file is not a command surface.

.PHONY: __runtime-log-policy-tests __runtime-support-tests

define orlix_runtime_log_policy
orlix_runtime_log_validate() { \
	bad=0; readable=0; \
	for log in "$$@"; do \
		[ -r "$$log" ] || continue; \
		readable=1; \
		LC_ALL=C tr -d '\r' < "$$log" | awk ' \
			BEGIN { bad = 0 } \
			/(^|[^[:alnum:]_])not ok[[:space:]]+[0-9]+([[:space:]-]|$$)/ { print "upstream failure marker: " $$0 > "/dev/stderr"; bad = 1 } \
			/Kernel panic|kernel panic|panic:|Oops|BUG:|Out of memory|oom-killer|Killed process|Attempted (to )?kill init|Assertion .* failed/ { print "fatal runtime marker: " $$0 > "/dev/stderr"; bad = 1 } \
			END { exit bad ? 1 : 0 } \
		' || bad=1; \
	done; \
	if [ "$$readable" -eq 0 ]; then echo "no readable runtime log was provided" >&2; return 1; fi; \
	return "$$bad"; \
}; \
orlix_runtime_log_has_failure() { \
	if orlix_runtime_log_validate "$$@" >/dev/null 2>&1; then return 1; fi; \
	return 0; \
}; \
orlix_runtime_log_observe() { \
	duration="$$1"; shift; elapsed=0; \
	while [ "$$elapsed" -lt "$$duration" ]; do \
		orlix_runtime_log_has_failure "$$@" && return 1; \
		sleep 1; elapsed=$$((elapsed + 1)); \
	done; \
	return 0; \
}
endef

__runtime-log-policy-tests:
	@set -eu; \
	$(orlix_runtime_log_policy); \
	tmp="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-runtime-log-test.XXXXXX")"; \
	trap 'rm -rf "$$tmp"' EXIT INT TERM; \
	clean="$$tmp/clean.log"; printf '%s\n' 'ORLIX-MLIBC-TEST-END' > "$$clean"; \
	orlix_runtime_log_validate "$$clean"; \
	if orlix_runtime_log_has_failure "$$clean"; then echo "clean completion was classified as failure" >&2; exit 1; fi; \
	if orlix_runtime_log_validate "$$tmp/missing.log" >/dev/null 2>&1; then echo "missing runtime log was accepted" >&2; exit 1; fi; \
	orlix_runtime_log_validate "$$clean" "$$tmp/missing.log"; \
	for marker in 'not ok 1 - terminal failure' 'Kernel panic - not syncing' 'kernel panic' 'panic: runtime failure' 'Oops: fatal exception' 'BUG: failure' 'Out of memory' 'oom-killer invoked' 'Killed process 1' 'Attempted to kill init' 'Attempted kill init' 'Assertion test failed'; do \
		failure="$$tmp/failure.log"; printf '%s\n%s\n' 'ORLIX-MLIBC-TEST-END' "$$marker" > "$$failure"; \
		if orlix_runtime_log_validate "$$failure" >/dev/null 2>&1; then echo "fatal marker was accepted: $$marker" >&2; exit 1; fi; \
		if ! orlix_runtime_log_has_failure "$$failure"; then echo "fatal marker was not detected: $$marker" >&2; exit 1; fi; \
	done; \
	fatal_os="$$tmp/fatal-os.log"; printf 'BUG: secondary log\n' > "$$fatal_os"; \
	if orlix_runtime_log_validate "$$clean" "$$fatal_os" >/dev/null 2>&1; then echo "fatal secondary log was accepted" >&2; exit 1; fi; \
	started="$$(date +%s)"; if orlix_runtime_log_observe 5 "$$fatal_os"; then echo "runtime observer ignored failure" >&2; exit 1; fi; \
	elapsed=$$(($$(date +%s) - started)); [ "$$elapsed" -lt 2 ] || { echo "runtime observer did not stop promptly" >&2; exit 1; }; \
	started="$$(date +%s)"; orlix_runtime_log_observe 1 "$$clean"; elapsed=$$(($$(date +%s) - started)); \
	[ "$$elapsed" -ge 1 ] || { echo "runtime observer did not observe the full interval" >&2; exit 1; }; \
	echo "pass: runtime log policy"

__runtime-support-tests: __runtime-log-policy-tests
	@$(ORLIXOS_MAKE) __payload-sync-tests PROFILE="$(PROFILE)" ORLIX_BUILD_ROOT="$(ORLIX_BUILD_ROOT)"
