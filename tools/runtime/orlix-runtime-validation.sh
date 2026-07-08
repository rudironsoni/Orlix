#!/usr/bin/env bash
set -euo pipefail

gate="${GATE:-tcti-init-first-syscall}"
profile="${PROFILE:-}"
if [ -z "$profile" ]; then
	case "$gate" in
	tcti-*) profile="tcti_runtime" ;;
	*) profile="development" ;;
	esac
fi
if [ -n "${DESTINATION:-}" ]; then
	destination="$DESTINATION"
else
	case "$gate" in
	tcti-*) destination="iphonesimulator" ;;
	*) destination="iphoneos" ;;
	esac
fi
configuration="${CONFIGURATION:-Debug}"
scheme="${ORLIX_SCHEME:-Orlix}"
bundle_id="${ORLIX_APP_BUNDLE_ID:-com.rudironsoni.Orlix}"
report_dir="${REPORT_DIR:-${ORLIX_RUNTIME_REPORT_DIR:-Build/Reports/runtime}}"
device_id="${ORLIX_DEVICE_ID:-}"
simulator_id="${ORLIX_SIMULATOR_ID:-${ORLIX_BETA_SIMULATOR_ID:-}}"
required_simulator_id="${ORLIX_TCTI_REQUIRED_SIMULATOR_ID:-1E5553B0-203A-4A11-BAD7-EBDE46863F66}"
required_simulator_name="${ORLIX_TCTI_REQUIRED_SIMULATOR_NAME:-Orlix-iPhone-15-Pro-Max}"
capture_seconds="${ORLIX_RUNTIME_GATE_CAPTURE_SECONDS:-45}"
simulator_boot_timeout_seconds="${ORLIX_SIMULATOR_BOOT_TIMEOUT_SECONDS:-600}"
development_team="${ORLIX_DEVELOPMENT_TEAM:-}"
code_sign_style="${ORLIX_CODE_SIGN_STYLE:-}"
provisioning_profile_specifier="${ORLIX_PROVISIONING_PROFILE_SPECIFIER:-}"
runtime_preflight_only="${ORLIX_RUNTIME_PREFLIGHT_ONLY:-}"
tcti_device_override="${ORLIX_TCTI_DEVICE_OVERRIDE:-}"
tcti_device_override_reason="${ORLIX_TCTI_DEVICE_OVERRIDE_REASON:-${ORLIX_TCTI_DEVICE_OVERRIDE_REASON_TEXT:-}}"
tcti_evidence_mode=0
build_root="${ORLIX_BUILD_ROOT:-}"
busybox_shell_marker="ORLIX-TCTI-BUSYBOX-USABLE"
console_marker="ORLIX-TCTI-CONSOLE-OK"
full_shell_marker="ORLIX-TCTI-SHELL-USABLE"
mlibc_smoke_marker="ORLIX-TCTI-MLIBC-SMOKE-OK"
coreutils_smoke_marker="ORLIX-TCTI-COREUTILS-SMOKE-OK"
shell_pipeline_marker="ORLIX-TCTI-SHELL-PIPELINE-OK"
shell_env_marker="ORLIX-TCTI-SHELL-ENV-OK"
shell_redirection_marker="ORLIX-TCTI-SHELL-REDIRECTION-OK"
shell_script_marker="ORLIX-TCTI-SHELL-SCRIPT-OK"
coreutils_true_false_echo_marker="ORLIX-TCTI-COREUTILS-TRUE-FALSE-ECHO-OK"
coreutils_cat_wc_marker="ORLIX-TCTI-COREUTILS-CAT-WC-OK"
coreutils_ls_stat_marker="ORLIX-TCTI-COREUTILS-LS-STAT-OK"
coreutils_mkdir_rm_cp_ln_marker="ORLIX-TCTI-COREUTILS-MKDIR-RM-CP-LN-OK"
coreutils_env_path_marker="ORLIX-TCTI-COREUTILS-ENV-PATH-OK"
coreutils_test_subset_marker="ORLIX-TCTI-COREUTILS-TEST-SUBSET-OK"
package_behavior_marker="ORLIX-TCTI-PACKAGE-BEHAVIOR-OK"
oci_rootfs_command_marker="ORLIX-TCTI-OCI-ROOTFS-COMMAND-OK"
dynamic_loader_marker="ORLIX-TCTI-DYNAMIC-LOADER-OK"
signals_marker="ORLIX-TCTI-SIGNALS-OK"
vfs_marker="ORLIX-TCTI-VFS-OK"
full_runtime_marker="ORLIX-TCTI-FULL-RUNTIME-OK"

devices_json=""
device_name=""
simulator_booted_count=""
simulator_booted_ids=""
simulator_single_booted="false"
device_xcode_id=""
device_ddi_available=""
app_path=""
failure_stage=""
failure_exit_status=""
failure_timeout_seconds=""
failure_stdout_empty=""
failure_stderr_empty=""
runtime_failure_reducer=""

mkdir -p "$report_dir"
report="$report_dir/${gate}-$(date -u +%Y%m%dT%H%M%SZ)-$$.md"
json_report="${report%.md}.json"
artifact_dir="${report%.md}.artifacts"
mkdir -p "$artifact_dir"

if [ -z "$build_root" ]; then
	external_root="$(external-ssd-root 2>/dev/null || true)"
	if [ -n "$external_root" ]; then
		build_root="$external_root/Xcode/OrlixSystem/Build"
	else
		build_root="$PWD/Build"
	fi
fi

die() {
	local message="$1"
	write_runtime_failure_reducer "$message"
	write_json_report "fail" "false" "$message" "false" ""
	write_report "failed" "$message"
	printf 'runtime validation failed, report: %s\n' "$report" >&2
	exit 1
}

require_command() {
	command -v "$1" >/dev/null 2>&1 || die "Required command \`$1\` was not found."
}

artifact_rel() {
	local path="$1"
	printf '%s' "${path#$report_dir/}"
}

write_report() {
	local status="$1"
	local message="$2"
	{
		printf '# Orlix Runtime Validation\n\n'
		printf -- '- gate: `%s`\n' "$gate"
		printf -- '- profile: `%s`\n' "$profile"
		printf -- '- destination: `%s`\n' "$destination"
		printf -- '- configuration: `%s`\n' "$configuration"
		printf -- '- scheme: `%s`\n' "$scheme"
		printf -- '- bundle id: `%s`\n' "$bundle_id"
		printf -- '- device: `%s`\n' "${device_id:-auto}"
		printf -- '- selected device id: `%s`\n' "${device_id:-unknown}"
		if [ -n "$device_name" ]; then
			printf -- '- device name: `%s`\n' "$device_name"
		fi
		if [ -n "$device_xcode_id" ]; then
			printf -- '- selected xcode device id: `%s`\n' "$device_xcode_id"
		fi
		printf -- '- artifact dir: `%s`\n' "$artifact_dir"
		printf -- '- capture seconds: `%s`\n' "$capture_seconds"
		if [ "$destination" = "iphonesimulator" ] || [ "$destination" = "iOS Simulator" ]; then
			printf -- '- simulator boot timeout seconds: `%s`\n' "$simulator_boot_timeout_seconds"
			printf -- '- required simulator id: `%s`\n' "$required_simulator_id"
			printf -- '- required simulator name: `%s`\n' "$required_simulator_name"
			printf -- '- booted simulator count: `%s`\n' "${simulator_booted_count:-unknown}"
			printf -- '- single required simulator booted: `%s`\n' "$simulator_single_booted"
		fi
		printf -- '- status: `%s`\n\n' "$status"
		printf '## Result\n\n%s\n\n' "$message"
		if [ -n "$failure_stage" ]; then
			printf '## Failure Context\n\n'
			printf -- '- stage: `%s`\n' "$failure_stage"
			if [ -n "$failure_exit_status" ]; then
				printf -- '- exit status: `%s`\n' "$failure_exit_status"
			fi
			if [ -n "$failure_timeout_seconds" ]; then
				printf -- '- timeout seconds: `%s`\n' "$failure_timeout_seconds"
			fi
			if [ -n "$failure_stdout_empty" ]; then
				printf -- '- install stdout empty: `%s`\n' "$failure_stdout_empty"
			fi
			if [ -n "$failure_stderr_empty" ]; then
				printf -- '- install stderr empty: `%s`\n' "$failure_stderr_empty"
			fi
			printf '\n'
		fi
		printf '## Artifacts\n\n'
		if compgen -G "$artifact_dir/*" >/dev/null; then
			for path in "$artifact_dir"/*; do
				[ -f "$path" ] || continue
				printf -- '- `%s`\n' "$(artifact_rel "$path")"
			done
		else
			printf -- '- none\n'
		fi
	} >"$report"
}

json_escape() {
	printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'
}

json_string_or_null() {
	local value="$1"
	if [ -n "$value" ]; then
		printf '"%s"' "$(json_escape "$value")"
	else
		printf 'null'
	fi
}

json_artifacts_array() {
	if ! compgen -G "$artifact_dir/*" >/dev/null; then
		printf '[]'
		return
	fi
	local first="true"
	local path
	printf '['
	for path in "$artifact_dir"/*; do
		[ -f "$path" ] || continue
		if [ "$first" = "true" ]; then
			first="false"
		else
			printf ', '
		fi
		printf '"%s"' "$(json_escape "$(artifact_rel "$path")")"
	done
	printf ']'
}

write_runtime_failure_reducer() {
	local message="$1"
	case "$gate:$destination" in
	tcti-package-behavior:iphonesimulator|tcti-package-behavior:"iOS Simulator") ;;
	*) return 0 ;;
	esac
	if [ -n "$runtime_failure_reducer" ]; then
		return 0
	fi

	local reducer_root="${ORLIX_TCTI_BUILD_ROOT:-Build/TCTI}/reproducers/simulator-tcti-package-behavior"
	local report_base case_id reducer_copy
	report_base="$(basename "$json_report" .json)"
	case_id="package-behavior-fail-${report_base#tcti-package-behavior-}"
	runtime_failure_reducer="$reducer_root/$case_id.json"
	reducer_copy="$artifact_dir/tcti-package-behavior-reducer.json"
	mkdir -p "$reducer_root"

	local replay_command
	replay_command="env PATH=\"\$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\" ORLIX_RUNTIME_GATE_CAPTURE_SECONDS=$capture_seconds make runtime-validation DESTINATION=iphonesimulator GATE=tcti-package-behavior ORLIX_SIMULATOR_ID=$required_simulator_id ORLIX_TCTI_REQUIRED_SIMULATOR_ID=$required_simulator_id ORLIX_TCTI_REQUIRED_SIMULATOR_NAME=$required_simulator_name"

	python3 - "$runtime_failure_reducer" "$PWD" "$case_id" "$replay_command" "$message" \
		"$json_report" \
		"$report" \
		"$artifact_dir/simulator-terminal-output.txt" \
		"$artifact_dir/tcti-package-behavior.txt" \
		"$artifact_dir/tcti-first-syscall.txt" \
		"$artifact_dir/host-exec-violations.txt" \
		"$artifact_dir/launch-console.log" \
		"$artifact_dir/launch.log" \
		"$artifact_dir/simulator-unified.log" <<'PY'
import json
import os
import sys

output, cwd, case_id, command, message, *candidates = sys.argv[1:]
artifacts = []
for candidate in candidates:
    if candidate and (os.path.exists(candidate) or candidate.endswith(".json") or candidate.endswith(".md")):
        artifacts.append(os.path.relpath(candidate, cwd))

payload = {
    "target": "simulator-tcti-package-behavior",
    "case_id": case_id,
    "command": command,
    "working_directory": cwd,
    "artifacts": artifacts,
    "reason": message,
    "expected_status": "fail",
}

tmp = output + ".tmp"
with open(tmp, "w", encoding="utf-8") as handle:
    json.dump(payload, handle, indent=2, sort_keys=True)
    handle.write("\n")
os.replace(tmp, output)
PY
	cp "$runtime_failure_reducer" "$reducer_copy"
}

failure_context_json() {
	if [ -z "$failure_stage" ]; then
		printf 'null'
		return
	fi

	cat <<JSON
{
    "stage": $(json_string_or_null "$failure_stage"),
    "exit_status": $(json_string_or_null "$failure_exit_status"),
    "timeout_seconds": $(json_string_or_null "$failure_timeout_seconds"),
    "install_stdout_empty": $(json_string_or_null "$failure_stdout_empty"),
    "install_stderr_empty": $(json_string_or_null "$failure_stderr_empty")
  }
JSON
}

json_number_or_null() {
	local value="$1"
	if [[ "$value" =~ ^-?[0-9]+$ ]]; then
		printf '%s' "$value"
	else
		printf 'null'
	fi
}

log_field() {
	local line="$1"
	local key="$2"
	printf '%s\n' "$line" | sed -nE "s/.*(^|[[:space:]])${key}=([^[:space:]]+).*/\\2/p" | head -1
}

tcti_runtime_events_json() {
	local first_svc_line=""
	local static_pie_line=""
	local exec_start_thread_line=""
	local mmap_line=""
	local fault_line=""
	local signaled_line=""
	local last_return_line=""
	local first_svc_task=""
	local first_svc_pid=""
	local first_svc_pc=""
	local first_svc_syscall=""
	local static_pie_task=""
	local static_pie_pid=""
	local static_pie_pc=""
	local static_pie_base=""
	local static_pie_entry=""
	local exec_start_thread_task=""
	local exec_start_thread_pid=""
	local exec_start_thread_pc=""
	local exec_start_thread_sp=""
	local exec_start_thread_pstate=""
	local exec_start_thread_syscallno=""
	local mmap_task=""
	local mmap_pid=""
	local mmap_pc=""
	local mmap_syscall=""
	local mmap_x0=""
	local mmap_x1=""
	local mmap_x2=""
	local mmap_x3=""
	local fault_task=""
	local fault_pid=""
	local fault_pc=""
	local fault_lr=""
	local fault_sp=""
	local fault_addr=""
	local fault_access=""
	local fault_si=""
	local signaled_pid=""
	local signaled_signal=""
	local last_return_task=""
	local last_return_pid=""
	local last_return_pc=""
	local last_return_syscall=""
	local last_return_ret=""
	local last_return_signed_ret=""

	first_svc_line="$(grep -h -F 'Orlix TCTI: svc #0' "$artifact_dir"/tcti-first-syscall.txt "$artifact_dir"/launch-console.log "$artifact_dir"/launch.log "$artifact_dir"/simulator-terminal-output.txt "$artifact_dir"/simulator-unified.log 2>/dev/null | head -1 || true)"
	static_pie_line="$(grep -h -E 'Orlix TCTI: static PIE image task=(init|sh) ' "$artifact_dir"/launch-console.log "$artifact_dir"/launch.log "$artifact_dir"/simulator-terminal-output.txt "$artifact_dir"/simulator-unified.log 2>/dev/null | tail -1 || true)"
	exec_start_thread_line="$(grep -h -E 'Orlix TCTI: linux exec start_thread ' "$artifact_dir"/launch-console.log "$artifact_dir"/launch.log "$artifact_dir"/simulator-terminal-output.txt "$artifact_dir"/simulator-unified.log 2>/dev/null | tail -1 || true)"
	mmap_line="$(grep -h -E 'Orlix TCTI: svc #0 task=(init|sh) .* syscall=222' "$artifact_dir"/launch-console.log "$artifact_dir"/launch.log "$artifact_dir"/simulator-terminal-output.txt "$artifact_dir"/simulator-unified.log 2>/dev/null | tail -1 || true)"
	fault_line="$(grep -h -E 'Orlix TCTI: user fault ' "$artifact_dir"/tcti-simulator-fatal-runtime.txt "$artifact_dir"/launch-console.log "$artifact_dir"/launch.log "$artifact_dir"/simulator-terminal-output.txt "$artifact_dir"/simulator-unified.log 2>/dev/null | tail -1 || true)"
	signaled_line="$(grep -h -E 'orlix-init: process signaled pid=[0-9]+ signal=[0-9]+' "$artifact_dir"/tcti-simulator-fatal-runtime.txt "$artifact_dir"/launch-console.log "$artifact_dir"/launch.log "$artifact_dir"/simulator-terminal-output.txt "$artifact_dir"/simulator-unified.log 2>/dev/null | tail -1 || true)"
	last_return_line="$(grep -h -E 'Orlix TCTI: syscall return task=sh ' "$artifact_dir"/launch-console.log "$artifact_dir"/launch.log "$artifact_dir"/simulator-terminal-output.txt "$artifact_dir"/simulator-unified.log 2>/dev/null | tail -1 || true)"

	first_svc_task="$(log_field "$first_svc_line" task)"
	first_svc_pid="$(log_field "$first_svc_line" pid)"
	first_svc_pc="$(log_field "$first_svc_line" pc)"
	first_svc_syscall="$(log_field "$first_svc_line" syscall)"
	static_pie_task="$(log_field "$static_pie_line" task)"
	static_pie_pid="$(log_field "$static_pie_line" pid)"
	static_pie_pc="$(log_field "$static_pie_line" pc)"
	static_pie_base="$(log_field "$static_pie_line" base)"
	static_pie_entry="$(log_field "$static_pie_line" entry)"
	exec_start_thread_task="$(log_field "$exec_start_thread_line" task)"
	exec_start_thread_pid="$(log_field "$exec_start_thread_line" pid)"
	exec_start_thread_pc="$(log_field "$exec_start_thread_line" pc)"
	exec_start_thread_sp="$(log_field "$exec_start_thread_line" sp)"
	exec_start_thread_pstate="$(log_field "$exec_start_thread_line" pstate)"
	exec_start_thread_syscallno="$(log_field "$exec_start_thread_line" syscallno)"
	mmap_task="$(log_field "$mmap_line" task)"
	mmap_pid="$(log_field "$mmap_line" pid)"
	mmap_pc="$(log_field "$mmap_line" pc)"
	mmap_syscall="$(log_field "$mmap_line" syscall)"
	mmap_x0="$(log_field "$mmap_line" x0)"
	mmap_x1="$(log_field "$mmap_line" x1)"
	mmap_x2="$(log_field "$mmap_line" x2)"
	mmap_x3="$(log_field "$mmap_line" x3)"
	fault_task="$(log_field "$fault_line" task)"
	fault_pid="$(log_field "$fault_line" pid)"
	fault_pc="$(log_field "$fault_line" pc)"
	fault_lr="$(log_field "$fault_line" lr)"
	fault_sp="$(log_field "$fault_line" sp)"
	fault_addr="$(log_field "$fault_line" addr)"
	fault_access="$(log_field "$fault_line" access)"
	fault_si="$(log_field "$fault_line" si)"
	signaled_pid="$(printf '%s\n' "$signaled_line" | sed -nE 's/.*pid=([0-9]+).*/\1/p' | head -1)"
	signaled_signal="$(printf '%s\n' "$signaled_line" | sed -nE 's/.*signal=([0-9]+).*/\1/p' | head -1)"
	last_return_task="$(log_field "$last_return_line" task)"
	last_return_pid="$(log_field "$last_return_line" pid)"
	last_return_pc="$(log_field "$last_return_line" pc)"
	last_return_syscall="$(log_field "$last_return_line" syscall)"
	last_return_ret="$(log_field "$last_return_line" ret)"
	last_return_signed_ret="$(log_field "$last_return_line" signed_ret)"

	cat <<JSON
{
    "first_svc": {
      "task": $(json_string_or_null "$first_svc_task"),
      "pid": $(json_number_or_null "$first_svc_pid"),
      "pc": $(json_string_or_null "$first_svc_pc"),
      "syscall": $(json_number_or_null "$first_svc_syscall")
    },
    "static_pie_image": {
      "task": $(json_string_or_null "$static_pie_task"),
      "pid": $(json_number_or_null "$static_pie_pid"),
      "pc": $(json_string_or_null "$static_pie_pc"),
      "base": $(json_string_or_null "$static_pie_base"),
      "entry": $(json_string_or_null "$static_pie_entry")
    },
    "linux_exec_start_thread": {
      "task": $(json_string_or_null "$exec_start_thread_task"),
      "pid": $(json_number_or_null "$exec_start_thread_pid"),
      "pc": $(json_string_or_null "$exec_start_thread_pc"),
      "sp": $(json_string_or_null "$exec_start_thread_sp"),
      "pstate": $(json_string_or_null "$exec_start_thread_pstate"),
      "syscallno": $(json_number_or_null "$exec_start_thread_syscallno")
    },
    "last_mmap_syscall": {
      "task": $(json_string_or_null "$mmap_task"),
      "pid": $(json_number_or_null "$mmap_pid"),
      "pc": $(json_string_or_null "$mmap_pc"),
      "syscall": $(json_number_or_null "$mmap_syscall"),
      "args": [$(json_string_or_null "$mmap_x0"), $(json_string_or_null "$mmap_x1"), $(json_string_or_null "$mmap_x2"), $(json_string_or_null "$mmap_x3")]
    },
    "fatal_user_fault": {
      "task": $(json_string_or_null "$fault_task"),
      "pid": $(json_number_or_null "$fault_pid"),
      "pc": $(json_string_or_null "$fault_pc"),
      "lr": $(json_string_or_null "$fault_lr"),
      "sp": $(json_string_or_null "$fault_sp"),
      "addr": $(json_string_or_null "$fault_addr"),
      "access": $(json_number_or_null "$fault_access"),
      "si": $(json_number_or_null "$fault_si")
    },
	    "signaled_process": {
	      "pid": $(json_number_or_null "$signaled_pid"),
	      "signal": $(json_number_or_null "$signaled_signal")
	    },
	    "last_sh_syscall_return": {
	      "task": $(json_string_or_null "$last_return_task"),
	      "pid": $(json_number_or_null "$last_return_pid"),
	      "pc": $(json_string_or_null "$last_return_pc"),
	      "syscall": $(json_number_or_null "$last_return_syscall"),
	      "ret": $(json_string_or_null "$last_return_ret"),
	      "signed_ret": $(json_number_or_null "$last_return_signed_ret")
	    }
	  }
JSON
}

simulator_launch_arguments() {
	local -n output_args="$1"
	case "$gate" in
	tcti-init-first-syscall|tcti-simulator-stability)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.terminal=0 orlix.exec=/bin/true orlix.argv0=/bin/true"
		)
		;;
	tcti-init-console-write)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=printf%20$console_marker%3B%20exit%200"
		)
		;;
	tcti-static-busybox-shell-command)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=printf%20$busybox_shell_marker%3B%20exit%200"
		)
		;;
	tcti-full-shell-usability)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=set%20-e%3B%20cd%20/%3B%20pwd%3B%20echo%20shell-basic%20%3E%20/tmp/orlix-tcti-shell%3B%20test%20-f%20/tmp/orlix-tcti-shell%3B%20cat%20/tmp/orlix-tcti-shell%3B%20printf%20$full_shell_marker%3B%20exit%200"
		)
		;;
	tcti-mlibc-smoke)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=printf%20$mlibc_smoke_marker%3B%20exit%200"
		)
		;;
	tcti-coreutils-smoke)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=set%20-e%3B%20cd%20/%3B%20/bin/true%3B%20if%20/bin/false%3B%20then%20exit%201%3B%20fi%3B%20/bin/echo%20coreutils-smoke-ok%3B%20printf%20$coreutils_smoke_marker%3B%20exit%200"
		)
		;;
	tcti-shell-pipeline-smoke)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=set%20-e%3B%20cd%20/%3B%20echo%20beta%20%7C%20%7B%20read%20line%3B%20test%20%24line%20%3D%20beta%3B%20printf%20%24line%3B%20%7D%3B%20printf%20$shell_pipeline_marker%3B%20exit%200"
		)
		;;
	tcti-shell-env-var-smoke)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=set%20-e%3B%20cd%20/%3B%20FOO%3Denv-ok%3B%20export%20FOO%3B%20test%20%24FOO%20%3D%20env-ok%3B%20printf%20%24FOO%3B%20printf%20$shell_env_marker%3B%20exit%200"
		)
		;;
	tcti-shell-redirection-smoke)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=set%20-e%3B%20cd%20/%3B%20echo%20redir-ok%20%3E%20/tmp/orlix-tcti-redir%3B%20test%20-f%20/tmp/orlix-tcti-redir%3B%20read%20line%20%3C%20/tmp/orlix-tcti-redir%3B%20test%20%24line%20%3D%20redir-ok%3B%20printf%20%24line%3B%20printf%20$shell_redirection_marker%3B%20exit%200"
		)
		;;
	tcti-shell-script-smoke)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=set%20-e%3B%20cd%20/%3B%20script%3D/tmp/orlix-tcti-script%3B%20echo%20VALUE%3Dscript-ok%20%3E%20%24script%3B%20echo%20%22test%20%5C%24VALUE%20%3D%20script-ok%22%20%3E%3E%20%24script%3B%20echo%20%22printf%20%5C%24VALUE%22%20%3E%3E%20%24script%3B%20echo%20%22printf%20$shell_script_marker%22%20%3E%3E%20%24script%3B%20/bin/sh%20%24script%3B%20exit%200"
		)
		;;
	tcti-coreutils-true-false-echo)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=set%20-e%3B%20cd%20/%3B%20/bin/true%3B%20if%20/bin/false%3B%20then%20exit%201%3B%20fi%3B%20/bin/echo%20coreutils-ok%3B%20printf%20$coreutils_true_false_echo_marker%3B%20exit%200"
		)
		;;
	tcti-coreutils-cat-wc)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=set%20-e%3B%20cd%20/%3B%20printf%20%27cat-wc-ok%5Cn%27%20%3E%20/tmp/orlix-coreutils-cat-wc%3B%20set%20--%20%24(/bin/wc%20-l%20/tmp/orlix-coreutils-cat-wc)%3B%20test%20%22%241%22%20%3D%201%3B%20/bin/cat%20/tmp/orlix-coreutils-cat-wc%3B%20printf%20$coreutils_cat_wc_marker%3B%20exit%200"
		)
		;;
	tcti-coreutils-ls-stat)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=set%20-e%3B%20cd%20/%3B%20/bin/ls%20-ld%20/bin/sh%20%3E%20/tmp/orlix-coreutils-ls-stat%3B%20/bin/stat%20/bin/sh%20%3E%3E%20/tmp/orlix-coreutils-ls-stat%3B%20test%20-s%20/tmp/orlix-coreutils-ls-stat%3B%20/bin/cat%20/tmp/orlix-coreutils-ls-stat%3B%20printf%20$coreutils_ls_stat_marker%3B%20exit%200"
		)
		;;
	tcti-coreutils-mkdir-rm-cp-ln)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=set%20-e%3B%20cd%20/%3B%20root%3D/tmp/orlix-coreutils-mkdir-rm-cp-ln%3B%20/bin/rm%20-rf%20%24root%3B%20/bin/mkdir%20-p%20%24root/src%20%24root/dst%3B%20printf%20%27mkdir-rm-cp-ln-ok%5Cn%27%20%3E%20%24root/src/input%3B%20/bin/cp%20%24root/src/input%20%24root/dst/output%3B%20/bin/ln%20%24root/dst/output%20%24root/hardlink%3B%20/bin/cat%20%24root/hardlink%3B%20/bin/rm%20%24root/hardlink%3B%20/bin/rm%20-r%20%24root%3B%20printf%20$coreutils_mkdir_rm_cp_ln_marker%3B%20exit%200"
		)
		;;
	tcti-coreutils-env-path)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=set%20-e%3B%20cd%20/%3B%20PATH%3D/bin%3A/usr/bin%3B%20export%20PATH%3B%20/bin/env%20%3E%20/tmp/orlix-coreutils-env-path%3B%20/bin/printenv%20PATH%20%3E%3E%20/tmp/orlix-coreutils-env-path%3B%20test%20%22%24PATH%22%20%3D%20/bin%3A/usr/bin%3B%20/bin/cat%20/tmp/orlix-coreutils-env-path%3B%20printf%20%27env-path-ok%5Cn%27%3B%20printf%20$coreutils_env_path_marker%3B%20exit%200"
		)
		;;
	tcti-coreutils-test-subset)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=set%20-e%3B%20cd%20/%3B%20root%3D/tmp/orlix-coreutils-test-subset%3B%20/bin/rm%20-rf%20%24root%3B%20/bin/mkdir%20-p%20%24root/src%20%24root/dst%3B%20printf%20%27subset-ok%5Cn%27%20%3E%20%24root/src/input%3B%20/bin/cp%20%24root/src/input%20%24root/dst/output%3B%20/bin/ln%20%24root/dst/output%20%24root/hardlink%3B%20/bin/cat%20%24root/hardlink%20%3E%20%24root/out%3B%20set%20--%20%24(/bin/wc%20-l%20%24root/out)%3B%20test%20%22%241%22%20%3D%201%3B%20/bin/ls%20-ld%20%24root/dst%20%3E%3E%20%24root/out%3B%20/bin/stat%20%24root/dst/output%20%3E%3E%20%24root/out%3B%20PATH%3D/bin%3A/usr/bin%3B%20export%20PATH%3B%20/bin/env%20%3E%3E%20%24root/out%3B%20/bin/printenv%20PATH%20%3E%3E%20%24root/out%3B%20/bin/cat%20%24root/out%3B%20/bin/rm%20%24root/hardlink%3B%20/bin/rm%20-r%20%24root%3B%20printf%20$coreutils_test_subset_marker%3B%20exit%200"
		)
		;;
	tcti-package-behavior)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/grep orlix.argv0=/bin/grep orlix.argv1=-F orlix.argv2=$package_behavior_marker orlix.argv3=/usr/share/orlixos/package-behavior.txt"
		)
		;;
	tcti-oci-rootfs-command)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=set%20-e%3B%20cd%20/%3B%20/bin/grep%20-F%20$package_behavior_marker%20/usr/share/orlixos/package-behavior.txt%20%3E/dev/null%3B%20/bin/echo%20oci-rootfs-command-ok%3B%20printf%20$oci_rootfs_command_marker%3B%20exit%200"
		)
		;;
	tcti-dynamic-loader-support)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/bash orlix.argv0=/bin/bash orlix.argv1=-lc orlix.argv2=printf%20$dynamic_loader_marker%3B%20exit%200"
		)
		;;
	tcti-signals)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=trap%20%27printf%20$signals_marker%27%20TERM%3B%20kill%20-TERM%20%24%24%3B%20exit%201"
		)
		;;
	tcti-vfs-completeness)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=set%20-e%3B%20mkdir%20-p%20/tmp/orlix-tcti-vfs%3B%20echo%20vfs%20%3E%20/tmp/orlix-tcti-vfs/file%3B%20grep%20-F%20vfs%20/tmp/orlix-tcti-vfs/file%20%3E/dev/null%3B%20mv%20/tmp/orlix-tcti-vfs/file%20/tmp/orlix-tcti-vfs/file2%3B%20rm%20/tmp/orlix-tcti-vfs/file2%3B%20rmdir%20/tmp/orlix-tcti-vfs%3B%20printf%20$vfs_marker%3B%20exit%200"
		)
		;;
	tcti-full-linux-runtime-readiness)
		output_args=(
			--orlix-kernel-command-line-append \
			"orlix.exec=/bin/sh orlix.argv0=/bin/sh orlix.argv1=-c orlix.argv2=set%20-e%3B%20/bin/sh%20-c%20true%3B%20/bin/bash%20-lc%20true%3B%20test%20-d%20/proc%3B%20test%20-d%20/dev%3B%20test%20-d%20/tmp%3B%20printf%20$full_runtime_marker%3B%20exit%200"
		)
		;;
	esac
}

write_json_report() {
	local status="$1"
	local passed="$2"
	local message="$3"
	local bypassed="$4"
	local bypass_reason="$5"
	local release_eligible="false"
	local readiness_eligible="false"
	local proof_tier="device"
	local acceptance_weight="blocker"
	local real_stack_required="true"
	local can_claim_runtime_readiness="false"
if [ "$destination" = "iphonesimulator" ]; then
proof_tier="simulator"
fi
if [ "$gate" = "tcti-coreutils-test-subset" ]; then
acceptance_weight="readiness"
fi
if [ "$status" = "pass" ] &&
		[ "$destination" = "iphoneos" ] &&
		[ "$gate" = "tcti-init-first-syscall" ] &&
		autonomous_tcti_reports_passed &&
		simulator_tcti_full_ladder_passed; then
		readiness_eligible="true"
	fi
	local escaped_message
	local escaped_reason
	local escaped_artifact_dir
	local escaped_bundle_id
	local escaped_configuration
	local escaped_destination
	local escaped_profile
		local escaped_scheme
		local escaped_required_simulator_id
		local escaped_required_simulator_name
		local escaped_simulator_booted_ids
	local artifacts_json
	local failure_context
	local tcti_runtime_events
	local preflight_only="false"
	if [ -n "$runtime_preflight_only" ]; then
		preflight_only="true"
	fi
	escaped_message="$(json_escape "$message")"
	escaped_reason="$(json_escape "$bypass_reason")"
	escaped_artifact_dir="$(json_escape "$artifact_dir")"
	escaped_bundle_id="$(json_escape "$bundle_id")"
	escaped_configuration="$(json_escape "$configuration")"
	escaped_destination="$(json_escape "$destination")"
		escaped_profile="$(json_escape "$profile")"
		escaped_scheme="$(json_escape "$scheme")"
		escaped_required_simulator_id="$(json_escape "$required_simulator_id")"
		escaped_required_simulator_name="$(json_escape "$required_simulator_name")"
		escaped_simulator_booted_ids="$(json_escape "$simulator_booted_ids")"
	artifacts_json="$(json_artifacts_array)"
	failure_context="$(failure_context_json)"
	tcti_runtime_events="$(tcti_runtime_events_json)"
	cat >"$json_report.tmp" <<JSON
{
  "artifacts": $artifacts_json,
  "artifact_dir": "$escaped_artifact_dir",
  "autonomous_tests_bypassed": $bypassed,
  "backend": "tcti",
  "bypass_reason": "$escaped_reason",
  "bundle_id": "$escaped_bundle_id",
  "configuration": "$escaped_configuration",
  "counters": {},
	"coverage_warnings": [],
	"destination": "$escaped_destination",
	"acceptance_weight": "$acceptance_weight",
	"can_claim_runtime_readiness": $can_claim_runtime_readiness,
	"failure_context": $failure_context,
	"failures": [],
  "forbidden_behavior": {
    "generated_exec_memory": false,
    "host_exec_guest_text": false,
    "host_x18": false,
    "map_jit": false,
    "native_ios_api_exposure_to_guest": false,
    "rwx": false
  },
  "gate": "$gate",
  "git_sha": "$(git rev-parse HEAD 2>/dev/null || true)",
  "guest_page_size": 4096,
  "host_page_size": $(getconf PAGESIZE),
  "passed": $passed,
	"preflight_only": $preflight_only,
	"proof_tier": "$proof_tier",
	"profile": "$escaped_profile",
	"readiness_gate_eligible": $readiness_eligible,
	"real_stack_required": $real_stack_required,
	"release_gate_eligible": $release_eligible,
	  "scheme": "$escaped_scheme",
	  "selected_device_id": $(json_string_or_null "$device_id"),
	  "selected_device_name": $(json_string_or_null "$device_name"),
	  "selected_xcode_device_id": $(json_string_or_null "$device_xcode_id"),
	  "simulator_booted_count": $(json_string_or_null "$simulator_booted_count"),
	  "simulator_booted_ids": "$escaped_simulator_booted_ids",
	  "simulator_single_booted": $simulator_single_booted,
	  "tcti_required_simulator_id": "$escaped_required_simulator_id",
	  "tcti_required_simulator_name": "$escaped_required_simulator_name",
	  "status": "$status",
  "summary": "$escaped_message",
  "target": "$gate",
  "tcti_runtime_events": $tcti_runtime_events,
  "virtual_cpu_model": "orlix-aarch64-v1"
}
JSON
	mv "$json_report.tmp" "$json_report"
}

is_tcti_physical_gate() {
	[ "$destination" = "iphoneos" ] && [[ "$gate" == tcti-* ]]
}

report_has_passed() {
	local path="$1"
	local current_sha
	current_sha="$(git rev-parse HEAD 2>/dev/null || true)"
	[ -n "$current_sha" ] || return 1
	[ -s "$path" ] &&
		grep -q '"status"[[:space:]]*:[[:space:]]*"pass"' "$path" &&
		grep -q '"passed"[[:space:]]*:[[:space:]]*true' "$path" &&
		grep -q "\"git_sha\"[[:space:]]*:[[:space:]]*\"$current_sha\"" "$path" &&
		! grep -q '"status"[[:space:]]*:[[:space:]]*"evidence"' "$path" &&
		! grep -q '"preflight_only"[[:space:]]*:[[:space:]]*true' "$path" &&
		! grep -q '"autonomous_tests_bypassed"[[:space:]]*:[[:space:]]*true' "$path"
}

autonomous_tcti_reports_passed() {
	local root="${ORLIX_TCTI_BUILD_ROOT:-Build/TCTI}"
	local target
	for target in \
		tcti-contract \
		tcti-golden-elf \
		tcti-diff-switch \
		tcti-memory-fuzz \
		tcti-direct-chain-fuzz \
		tcti-appstore-safety-audit \
		tcti-report-schema-check; do
		report_has_passed "$root/reports/$target/report.json" || return 1
	done
	return 0
}

latest_simulator_stability_report() {
	local latest=""
	local path
	for path in "$report_dir"/tcti-simulator-stability-*.json; do
		[ -e "$path" ] || continue
		if [ -z "$latest" ] || [ "$path" -nt "$latest" ]; then
			latest="$path"
		fi
	done
	[ -n "$latest" ] || return 1
	printf '%s\n' "$latest"
}

latest_simulator_first_syscall_report() {
	local latest=""
	local path
	for path in "$report_dir"/tcti-init-first-syscall-*.json; do
		[ -e "$path" ] || continue
		if [ -z "$latest" ] || [ "$path" -nt "$latest" ]; then
			latest="$path"
		fi
	done
	[ -n "$latest" ] || return 1
	printf '%s\n' "$latest"
}

latest_simulator_console_usability_report() {
	local latest=""
	local path
	for path in "$report_dir"/tcti-init-console-write-*.json; do
		[ -e "$path" ] || continue
		if [ -z "$latest" ] || [ "$path" -nt "$latest" ]; then
			latest="$path"
		fi
	done
	[ -n "$latest" ] || return 1
	printf '%s\n' "$latest"
}

latest_simulator_static_busybox_report() {
	local latest=""
	local path
	for path in "$report_dir"/tcti-static-busybox-start-*.json; do
		[ -e "$path" ] || continue
		if [ -z "$latest" ] || [ "$path" -nt "$latest" ]; then
			latest="$path"
		fi
	done
	[ -n "$latest" ] || return 1
	printf '%s\n' "$latest"
}

latest_runtime_report_for_gate() {
	local expected_gate="$1"
	local latest=""
	local path
	for path in "$report_dir"/"$expected_gate"-*.json; do
		[ -e "$path" ] || continue
		if [ -z "$latest" ] || [ "$path" -nt "$latest" ]; then
			latest="$path"
		fi
	done
	[ -n "$latest" ] || return 1
	printf '%s\n' "$latest"
}

simulator_tcti_report_passed() {
	local path="$1"
	local expected_gate="$2"
	local current_sha
	current_sha="$(git rev-parse HEAD 2>/dev/null || true)"
	[ -n "$current_sha" ] || return 1
	report_has_passed "$path" || return 1
	grep -q "\"git_sha\"[[:space:]]*:[[:space:]]*\"$current_sha\"" "$path" || return 1
	grep -q '"destination"[[:space:]]*:[[:space:]]*"iphonesimulator"' "$path" || return 1
	grep -q "\"selected_device_id\"[[:space:]]*:[[:space:]]*\"$required_simulator_id\"" "$path" || return 1
	grep -q "\"selected_device_name\"[[:space:]]*:[[:space:]]*\"$required_simulator_name\"" "$path" || return 1
	grep -q '"simulator_booted_count"[[:space:]]*:[[:space:]]*"1"' "$path" || return 1
	grep -q '"simulator_single_booted"[[:space:]]*:[[:space:]]*true' "$path" || return 1
	grep -Eq "\"(gate|target)\"[[:space:]]*:[[:space:]]*\"$expected_gate\"" "$path" || return 1
	grep -q '"backend"[[:space:]]*:[[:space:]]*"tcti"' "$path" || return 1
	grep -q '"profile"[[:space:]]*:[[:space:]]*"tcti_runtime"' "$path" || return 1
	grep -q '"preflight_only"[[:space:]]*:[[:space:]]*false' "$path" || return 1
	grep -q '"autonomous_tests_bypassed"[[:space:]]*:[[:space:]]*false' "$path" || return 1
	for key in generated_exec_memory host_exec_guest_text host_x18 map_jit native_ios_api_exposure_to_guest rwx; do
		grep -q "\"$key\"[[:space:]]*:[[:space:]]*false" "$path" || return 1
	done
	simulator_tcti_report_fatal_free "$path" || return 1
	return 0
}

simulator_tcti_report_fatal_free() {
	local path="$1"
	local artifacts="${path%.json}.artifacts"

	if grep -E 'Kernel panic|Attempted (to )?kill init|Orlix TCTI: user fault|panic - not syncing|BUG:|Oops|SIGSEGV|fatal error|Fatal error|crash|Crash|orlix-init: process signaled .* signal=[0-9]+|orlix-init: shell exit status=[1-9][0-9]*([[:space:]]|$)' \
		"$artifacts"/launch-console.log \
		"$artifacts"/launch.log \
		"$artifacts"/simulator-terminal-output.txt \
		"$artifacts"/simulator-unified.log >/dev/null 2>&1; then
		return 1
	fi
	return 0
}

simulator_tcti_stability_report_passed() {
	local path
	path="$(latest_simulator_stability_report)" || return 1
	simulator_tcti_report_passed "$path" "tcti-simulator-stability"
}

simulator_tcti_first_syscall_report_passed() {
	local path
	path="$(latest_simulator_first_syscall_report)" || return 1
	simulator_tcti_report_passed "$path" "tcti-init-first-syscall" || return 1
	grep -q 'tcti-first-syscall.txt' "$path" || return 1
	return 0
}

simulator_tcti_console_usability_report_passed() {
	local path
	path="$(latest_simulator_console_usability_report)" || return 1
	simulator_tcti_report_passed "$path" "tcti-init-console-write" || return 1
	grep -q 'tcti-console-write.txt' "$path" || return 1
	grep -F -q "$console_marker" "${path%.json}.artifacts/tcti-console-write.txt" || return 1
	return 0
}

simulator_tcti_static_busybox_report_passed() {
	local path
	path="$(latest_simulator_static_busybox_report)" || return 1
	simulator_tcti_report_passed "$path" "tcti-static-busybox-start"
	return 0
}

simulator_tcti_marker_report_passed() {
	local expected_gate="$1"
	local marker_artifact="$2"
	local marker="$3"
	local path
	path="$(latest_runtime_report_for_gate "$expected_gate")" || return 1
	simulator_tcti_report_passed "$path" "$expected_gate" || return 1
	local marker_path="${path%.json}.artifacts/$marker_artifact"
	[ -s "$marker_path" ] || return 1
	grep -F -q "$marker" "$marker_path" || return 1
	return 0
}

simulator_tcti_full_ladder_passed() {
	simulator_tcti_first_syscall_report_passed &&
		simulator_tcti_stability_report_passed &&
		simulator_tcti_console_usability_report_passed &&
		simulator_tcti_static_busybox_report_passed &&
		simulator_tcti_marker_report_passed tcti-static-busybox-shell-command tcti-static-busybox-shell-command.txt ORLIX-TCTI-BUSYBOX-USABLE &&
		simulator_tcti_marker_report_passed tcti-full-shell-usability tcti-full-shell-usability.txt ORLIX-TCTI-SHELL-USABLE &&
		simulator_tcti_marker_report_passed tcti-package-behavior tcti-package-behavior.txt ORLIX-TCTI-PACKAGE-BEHAVIOR-OK &&
		simulator_tcti_marker_report_passed tcti-dynamic-loader-support tcti-dynamic-loader-support.txt ORLIX-TCTI-DYNAMIC-LOADER-OK &&
		simulator_tcti_marker_report_passed tcti-signals tcti-signals.txt ORLIX-TCTI-SIGNALS-OK &&
		simulator_tcti_marker_report_passed tcti-vfs-completeness tcti-vfs-completeness.txt ORLIX-TCTI-VFS-OK &&
			simulator_tcti_marker_report_passed tcti-full-linux-runtime-readiness tcti-full-linux-runtime-readiness.txt ORLIX-TCTI-FULL-RUNTIME-OK
}

tcti_runtime_or_harness_worktree_clean() {
	[ -z "$(git status --short 2>/dev/null)" ]
}

physical_tcti_preflight() {
	is_tcti_physical_gate || return 0
	if [ "${ORLIX_TCTI_ALLOW_PHYSICAL_DEVICE:-0}" != "1" ] &&
		[ "${ORLIX_TCTI_PHYSICAL_DEVICE_ALLOWED:-0}" != "1" ]; then
		die "Physical TCTI gates require explicit human opt-in through ORLIX_TCTI_ALLOW_PHYSICAL_DEVICE=1 or ORLIX_TCTI_PHYSICAL_DEVICE_ALLOWED=1 after simulator readiness passes."
	fi
	if ! tcti_runtime_or_harness_worktree_clean; then
		die "Physical TCTI gates require a clean worktree so current simulator reports cover the app, kernel, libc, OS payload, runtime, and harness inputs."
	fi
	if autonomous_tcti_reports_passed && simulator_tcti_full_ladder_passed; then
		return 0
	fi
		die "Physical TCTI gates require passing autonomous TCTI reports plus current passing simulator first-syscall, stability, Linux console, static BusyBox start, static BusyBox shell command, full shell usability, package behavior, dynamic loader, signals, VFS, and full Linux runtime readiness reports before device work."
}

validate_gate() {
	case "$gate" in
	tcti-init-first-syscall|tcti-simulator-stability|tcti-init-console-write|tcti-static-busybox-start|tcti-static-busybox-shell-command|tcti-full-shell-usability|tcti-mlibc-smoke|tcti-coreutils-smoke|tcti-shell-pipeline-smoke|tcti-shell-env-var-smoke|tcti-shell-redirection-smoke|tcti-shell-script-smoke|tcti-coreutils-true-false-echo|tcti-coreutils-cat-wc|tcti-coreutils-ls-stat|tcti-coreutils-mkdir-rm-cp-ln|tcti-coreutils-env-path|tcti-coreutils-test-subset|tcti-package-behavior|tcti-oci-rootfs-command|tcti-dynamic-loader-support|tcti-signals|tcti-vfs-completeness|tcti-full-linux-runtime-readiness|tcti-dynamic-loader-start|tcti-alpine-sh-start|tcti-benchmark)
		;;
	*)
		die "Unknown runtime validation gate \`$gate\`."
		;;
	esac
}

discover_device_id() {
	local json_path="$1"

	python3 - "$json_path" <<'PY'
import json
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as handle:
    data = json.load(handle)

devices = data.get("result", {}).get("devices", [])
for device in devices:
    if not isinstance(device, dict):
        continue
    hardware = device.get("hardwareProperties", {})
    properties = device.get("deviceProperties", {})
    if not isinstance(hardware, dict) or not isinstance(properties, dict):
        continue
    if hardware.get("platform") != "iOS":
        continue
    product_type = hardware.get("productType")
    if not isinstance(product_type, str) or not product_type.startswith("iPhone"):
        continue
    if hardware.get("reality") != "physical":
        continue
    if properties.get("developerModeStatus") not in (None, "enabled"):
        continue
    identifier = device.get("identifier")
    if not isinstance(identifier, str) or not identifier:
        continue
    udid = hardware.get("udid")
    if not isinstance(udid, str) or not udid:
        continue
    ddi_available = properties.get("ddiServicesAvailable")
    ddi_text = "true" if ddi_available is True else "false"
    name = properties.get("name") or hardware.get("marketingName") or product_type
    print(f"{identifier}\t{udid}\t{ddi_text}\t{name}")
PY
}

discover_simulator_id() {
	local json_path="$1"

	python3 - "$json_path" <<'PY'
import json
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as handle:
    data = json.load(handle)

matches = []
for runtime, devices in data.get("devices", {}).items():
    if "iOS" not in runtime:
        continue
    if not isinstance(devices, list):
        continue
    for device in devices:
        if not isinstance(device, dict):
            continue
        name = device.get("name")
        identifier = device.get("udid")
        if not isinstance(name, str) or not name.startswith("iPhone"):
            continue
        if not isinstance(identifier, str) or not identifier:
            continue
        if device.get("isAvailable") is False:
            continue
        state = device.get("state") or "Unknown"
        matches.append((identifier, name, state))

for identifier, name, state in matches:
    print(f"{identifier} {name} [{state}]")
	PY
}

assert_tcti_simulator_scope() {
	case "$gate" in
	tcti-*) ;;
	*) return ;;
	esac

	if [ "$destination" != "iphonesimulator" ] && [ "$destination" != "iOS Simulator" ]; then
		return
	fi

	if [ -z "$required_simulator_id" ] || [ -z "$required_simulator_name" ]; then
		die "TCTI simulator gates require ORLIX_TCTI_REQUIRED_SIMULATOR_ID and ORLIX_TCTI_REQUIRED_SIMULATOR_NAME."
	fi
	if [ -n "$simulator_id" ] && [ "$simulator_id" != "$required_simulator_id" ]; then
		die "TCTI simulator gates must use ${required_simulator_name} (${required_simulator_id}); got simulator id \`${simulator_id}\`."
	fi
	simulator_id="$required_simulator_id"
}

assert_single_required_simulator_booted() {
	local allow_zero="${1:-false}"
	if [ "$destination" != "iphonesimulator" ] && [ "$destination" != "iOS Simulator" ]; then
		return
	fi

	local simulator_state_json="$artifact_dir/simulator-booted-state.json"
	local booted_txt="$artifact_dir/simulator-booted.txt"

	xcrun simctl list devices -j >"$simulator_state_json" 2>"$artifact_dir/simulator-booted-state.stderr" ||
		die "\`xcrun simctl list devices\` failed while checking booted simulator state."

	python3 - "$simulator_state_json" <<'PY' >"$booted_txt"
import json
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as handle:
    data = json.load(handle)

for devices in data.get("devices", {}).values():
    if not isinstance(devices, list):
        continue
    for device in devices:
        if not isinstance(device, dict):
            continue
        if device.get("state") != "Booted":
            continue
        udid = device.get("udid")
        name = device.get("name")
        if isinstance(udid, str) and isinstance(name, str):
            print(f"{udid}\t{name}")
PY
	simulator_booted_count="$(wc -l <"$booted_txt" | tr -d '[:space:]')"
	simulator_booted_ids="$(cut -f1 "$booted_txt" | paste -sd, -)"

	if [ "$simulator_booted_count" -eq 0 ] && [ "$allow_zero" = "true" ]; then
		return
	fi
	if [ "$simulator_booted_count" -ne 1 ]; then
		die "TCTI simulator gates require exactly one booted simulator, ${required_simulator_name} (${required_simulator_id}); found ${simulator_booted_count}."
	fi

	local booted_id booted_name
	IFS=$'\t' read -r booted_id booted_name <"$booted_txt"
	if [ "$booted_id" != "$required_simulator_id" ] || [ "$booted_name" != "$required_simulator_name" ]; then
		die "TCTI simulator gates require the only booted simulator to be ${required_simulator_name} (${required_simulator_id}); found ${booted_name:-unknown} (${booted_id:-unknown})."
	fi

	simulator_single_booted="true"
}

select_device() {
	if [ -n "$device_id" ]; then
		device_xcode_id="$device_id"
		return
	fi

	devices_json="$artifact_dir/devices.json"
	local discovered_txt="$artifact_dir/devices.txt"

	xcrun devicectl --timeout 30 --json-output "$devices_json" list devices \
		>"$artifact_dir/devices.stdout" 2>"$artifact_dir/devices.stderr" ||
		die "\`xcrun devicectl list devices\` failed."

	discover_device_id "$devices_json" >"$discovered_txt"
	local device_count
	device_count="$(wc -l <"$discovered_txt" | tr -d '[:space:]')"
	if [ "$device_count" -eq 0 ]; then
		die "No connected eligible physical iPhone was discovered. Set \`ORLIX_DEVICE_ID\` if devicectl reports the device in an unexpected JSON shape."
	fi
	if [ "$device_count" -gt 1 ]; then
		die "Multiple eligible physical iPhones were discovered. Set \`ORLIX_DEVICE_ID\`."
	fi

	IFS=$'\t' read -r device_id device_xcode_id device_ddi_available device_name <"$discovered_txt"
	if [ "$device_ddi_available" != "true" ]; then
		die "Physical iPhone \`${device_name:-$device_id}\` is not ready for Xcode device builds because developer disk image services are unavailable. Connect, unlock, trust the device, and let Xcode mount the developer disk image before rerunning runtime validation."
	fi
}

assert_tcti_simulator_scope_top_level() {
	case "$gate" in
	tcti-*) ;;
	*) return ;;
	esac

	if [ "$destination" != "iphonesimulator" ] && [ "$destination" != "iOS Simulator" ]; then
		return
	fi

	if [ -z "$required_simulator_id" ] || [ -z "$required_simulator_name" ]; then
		die "TCTI simulator gates require ORLIX_TCTI_REQUIRED_SIMULATOR_ID and ORLIX_TCTI_REQUIRED_SIMULATOR_NAME."
	fi
	if [ -n "$simulator_id" ] && [ "$simulator_id" != "$required_simulator_id" ]; then
		die "TCTI simulator gates must use ${required_simulator_name} (${required_simulator_id}); got simulator id \`${simulator_id}\`."
	fi
	simulator_id="$required_simulator_id"
}

assert_single_required_simulator_booted_top_level() {
	local allow_zero="${1:-false}"
	if [ "$destination" != "iphonesimulator" ] && [ "$destination" != "iOS Simulator" ]; then
		return
	fi

	local simulator_state_json="$artifact_dir/simulator-booted-state.json"
	local booted_txt="$artifact_dir/simulator-booted.txt"

	xcrun simctl list devices -j >"$simulator_state_json" 2>"$artifact_dir/simulator-booted-state.stderr" ||
		die "\`xcrun simctl list devices\` failed while checking booted simulator state."

	python3 - "$simulator_state_json" <<'PY' >"$booted_txt"
import json
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as handle:
    data = json.load(handle)

for devices in data.get("devices", {}).values():
    if not isinstance(devices, list):
        continue
    for device in devices:
        if not isinstance(device, dict):
            continue
        if device.get("state") != "Booted":
            continue
        udid = device.get("udid")
        name = device.get("name")
        if isinstance(udid, str) and isinstance(name, str):
            print(f"{udid}\t{name}")
PY
	simulator_booted_count="$(wc -l <"$booted_txt" | tr -d '[:space:]')"
	simulator_booted_ids="$(cut -f1 "$booted_txt" | paste -sd, -)"

	if [ "$simulator_booted_count" -eq 0 ] && [ "$allow_zero" = "true" ]; then
		return
	fi
	if [ "$simulator_booted_count" -ne 1 ]; then
		die "TCTI simulator gates require exactly one booted simulator, ${required_simulator_name} (${required_simulator_id}); found ${simulator_booted_count}."
	fi

	local booted_id booted_name
	IFS=$'\t' read -r booted_id booted_name <"$booted_txt"
	if [ "$booted_id" != "$required_simulator_id" ] || [ "$booted_name" != "$required_simulator_name" ]; then
		die "TCTI simulator gates require the only booted simulator to be ${required_simulator_name} (${required_simulator_id}); found ${booted_name:-unknown} (${booted_id:-unknown})."
	fi

	simulator_single_booted="true"
}

select_simulator() {
	assert_tcti_simulator_scope_top_level
	if [ -n "$device_id" ]; then
		if [ "$device_id" != "$required_simulator_id" ]; then
			die "TCTI simulator gates must not override the pinned simulator through ORLIX_DEVICE_ID; got \`${device_id}\`, expected \`${required_simulator_id}\`."
		fi
		device_name="$required_simulator_name"
		return
	fi
	if [ -n "$simulator_id" ]; then
		device_id="$simulator_id"
		if [ "$device_id" = "$required_simulator_id" ]; then
			device_name="$required_simulator_name"
		fi
		return
	fi

	devices_json="$artifact_dir/simulators.json"
	local discovered_txt="$artifact_dir/simulators.txt"

	xcrun simctl list devices available -j \
		>"$devices_json" 2>"$artifact_dir/simulators.stderr" ||
		die "\`xcrun simctl list devices available\` failed."

	discover_simulator_id "$devices_json" >"$discovered_txt"
	local device_count
	device_count="$(wc -l <"$discovered_txt" | tr -d '[:space:]')"
	if [ "$device_count" -eq 0 ]; then
		die "No eligible iPhone simulator was discovered. Set \`ORLIX_SIMULATOR_ID\` or \`ORLIX_DEVICE_ID\`."
	fi
	if [ "$device_count" -gt 1 ]; then
		die "Multiple eligible iPhone simulators were discovered. Set \`ORLIX_SIMULATOR_ID\` or \`ORLIX_DEVICE_ID\`."
	fi

	device_id="$(sed -n '1s/ .*$//p' "$discovered_txt")"
	device_name="$(sed -n '1s/^[^ ]* //p' "$discovered_txt")"
}

select_target() {
	case "$destination" in
	iphoneos)
		select_device
		;;
	iphonesimulator|"iOS Simulator")
		select_simulator
		;;
	*)
		die "Unsupported destination \`$destination\`. Use \`iphoneos\` or \`iphonesimulator\`."
		;;
	esac
}

build_kernel_for_gate() {
	local platform="iphoneos"

	if [ "$destination" = "iphonesimulator" ] || [ "$destination" = "iOS Simulator" ]; then
		platform="iphonesimulator"
	fi

	make -f OrlixKernel/Makefile __kernel-archive \
		PROFILE="$profile" \
		ORLIX_BUILD_ROOT="$build_root" \
		ORLIX_KERNEL_ARCHIVE_PLATFORMS="$platform" \
		>"$artifact_dir/kernel-build.log" 2>&1 ||
		die "TCTI kernel archive build failed."
}

build_payload_for_gate() {
	make -f OrlixOS/Makefile kernel-payload \
		PROFILE="$profile" \
		ORLIX_BUILD_ROOT="$build_root" \
		>"$artifact_dir/payload-build.log" 2>&1 ||
		die "TCTI OrlixOS payload packaging failed."
}

assert_tcti_kernel_config() {
	local config="$build_root/OrlixKernel/build/$profile/.config"

	case "$gate" in
	tcti-*) ;;
	*) return ;;
	esac

	if [ ! -s "$config" ]; then
		die "TCTI runtime gate could not inspect kernel config at \`$config\`."
	fi
	if ! grep -Fxq "CONFIG_ORLIX_HOSTED_EXEC_TCTI=y" "$config"; then
		die "TCTI runtime gate requires CONFIG_ORLIX_HOSTED_EXEC_TCTI=y in \`$config\`."
	fi
	if grep -Fxq "CONFIG_ORLIX_HOSTED_EXEC_NATIVE=y" "$config"; then
		die "TCTI runtime gate must not use CONFIG_ORLIX_HOSTED_EXEC_NATIVE=y in \`$config\`."
	fi
}

build_app_for_target() {
	local build_settings=(ORLIX_PROFILE="$profile")
	local signing_settings=()
	local xcode_destination_id="${device_xcode_id:-$device_id}"
	local xcode_destination="platform=iOS,id=$xcode_destination_id"

	if [ "$destination" = "iphonesimulator" ] || [ "$destination" = "iOS Simulator" ]; then
		xcode_destination="platform=iOS Simulator,id=$device_id"
	fi

	if [ "$destination" = "iphoneos" ] && [ -n "$development_team" ]; then
		signing_settings+=(DEVELOPMENT_TEAM="$development_team")
	fi
	if [ "$destination" = "iphoneos" ] && [ -n "$code_sign_style" ]; then
		signing_settings+=(CODE_SIGN_STYLE="$code_sign_style")
	fi
	if [ "$destination" = "iphoneos" ] && [ -n "$provisioning_profile_specifier" ]; then
		signing_settings+=(PROVISIONING_PROFILE_SPECIFIER="$provisioning_profile_specifier")
	fi

	if [ -z "${USER:-}" ] && [ -n "${LOGNAME:-}" ]; then
		export USER="$LOGNAME"
	fi

	xcodegen generate --spec project.yml \
		>"$artifact_dir/xcodegen.log" 2>&1 ||
		die "xcodegen failed."

	ORLIX_BUILD_ROOT="$build_root" ORLIX_PROFILE="$profile" xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "$scheme" \
		-configuration "$configuration" \
		-destination "$xcode_destination" \
		"${build_settings[@]}" \
		"${signing_settings[@]}" \
		build \
		>"$artifact_dir/xcodebuild.log" 2>&1 ||
		die "xcodebuild failed."

	ORLIX_BUILD_ROOT="$build_root" ORLIX_PROFILE="$profile" xcodebuild \
		-project Orlix.xcodeproj \
		-scheme "$scheme" \
		-configuration "$configuration" \
		-destination "$xcode_destination" \
		"${build_settings[@]}" \
		"${signing_settings[@]}" \
		-showBuildSettings -json \
		>"$artifact_dir/build-settings.json" 2>"$artifact_dir/build-settings.stderr" ||
		die "xcodebuild -showBuildSettings failed."

	local resolved_app_path
	resolved_app_path="$(python3 - "$artifact_dir/build-settings.json" "$bundle_id" "$scheme" <<'PY'
import json
import os
import sys

settings_path, bundle_id, scheme = sys.argv[1:4]
with open(settings_path, "r", encoding="utf-8") as handle:
    data = json.load(handle)

fallback = None
for entry in data:
    settings = entry.get("buildSettings", {})
    target = entry.get("target", "")
    product_bundle_id = settings.get("PRODUCT_BUNDLE_IDENTIFIER")
    target_dir = settings.get("TARGET_BUILD_DIR")
    product_name = settings.get("FULL_PRODUCT_NAME")
    if not target_dir or not product_name:
        continue
    candidate = os.path.join(target_dir, product_name)
    if target == scheme:
        fallback = candidate
    if product_bundle_id == bundle_id:
        print(candidate)
        sys.exit(0)

if fallback:
    print(fallback)
    sys.exit(0)

sys.exit(1)
PY
)" || die "Could not resolve built app path from Xcode build settings."

	if [ -z "$resolved_app_path" ]; then
		die "Xcode build settings did not contain a built app path."
	fi
	app_path="$resolved_app_path"
}

simulator_is_booted() {
	local selected_device_id="$1"
	local simulator_state_json="$artifact_dir/simulator-state.json"
	xcrun simctl list devices -j >"$simulator_state_json" 2>"$artifact_dir/simulator-state.stderr" ||
		return 1
	python3 - "$selected_device_id" "$simulator_state_json" <<'PY'
import json
import sys

selected_device_id, simulator_state_json = sys.argv[1:3]
with open(simulator_state_json, "r", encoding="utf-8") as handle:
    data = json.load(handle)
for devices in data.get("devices", {}).values():
    for device in devices:
        if device.get("udid") == selected_device_id:
            sys.exit(0 if device.get("state") == "Booted" else 1)
sys.exit(1)
PY
}

run_command_with_timeout() {
	local timeout_seconds="$1"
	local stdout_path="$2"
	local stderr_path="$3"
	shift 3

	python3 - "$timeout_seconds" "$stdout_path" "$stderr_path" "$@" <<'PY'
import os
import signal
import subprocess
import sys

timeout_seconds = int(sys.argv[1])
stdout_path = sys.argv[2]
stderr_path = sys.argv[3]
argv = sys.argv[4:]

with open(stdout_path, "w", encoding="utf-8") as stdout, \
     open(stderr_path, "w", encoding="utf-8") as stderr:
    proc = subprocess.Popen(
        argv,
        stdout=stdout,
        stderr=stderr,
        start_new_session=True,
    )
    try:
        sys.exit(proc.wait(timeout=timeout_seconds))
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGTERM)
            proc.wait(timeout=5)
        except Exception:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except Exception:
                pass
        sys.exit(124)
PY
}

install_app() {
	local app_path="$1"
	if [ ! -d "$app_path" ]; then
		die "Built app was not found at \`$app_path\`."
	fi
	printf '%s\n' "$app_path" >"$artifact_dir/app-path.txt"

	if [ "$destination" = "iphonesimulator" ] || [ "$destination" = "iOS Simulator" ]; then
		local bootstatus_status

		xcrun simctl boot "$device_id" >"$artifact_dir/boot.stdout" 2>"$artifact_dir/boot.stderr" || true
		if simulator_is_booted "$device_id"; then
			printf 'simctl reports the selected simulator is Booted. Skipping blocking bootstatus and continuing to install/launch probe.\n' \
				>"$artifact_dir/bootstatus-skipped-booted.txt"
		else
		set +e
		python3 - "$device_id" "$simulator_boot_timeout_seconds" \
			"$artifact_dir/bootstatus.stdout" \
			"$artifact_dir/bootstatus.stderr" <<'PY'
import subprocess
import sys
import os
import signal

device_id, timeout, stdout_path, stderr_path = sys.argv[1:5]
with open(stdout_path, "w", encoding="utf-8") as stdout, \
     open(stderr_path, "w", encoding="utf-8") as stderr:
    proc = None
    try:
        proc = subprocess.Popen(
            ["xcrun", "simctl", "bootstatus", device_id, "-b"],
            stdout=stdout,
            stderr=stderr,
            start_new_session=True,
        )
        completed = proc.wait(timeout=int(timeout))
    except subprocess.TimeoutExpired:
        if proc is not None:
            try:
                os.killpg(proc.pid, signal.SIGTERM)
                proc.wait(timeout=5)
            except Exception:
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except Exception:
                    pass
        sys.exit(124)
sys.exit(completed)
PY
		bootstatus_status=$?
		set -e
		if [ "$bootstatus_status" -eq 124 ]; then
			if simulator_is_booted "$device_id"; then
				printf 'Simulator bootstatus did not reach a terminal state within %ss, but simctl reports the selected simulator is Booted. Continuing to install/launch probe.\n' \
					"$simulator_boot_timeout_seconds" \
					>"$artifact_dir/bootstatus-nonterminal.txt"
			else
				die "Booting the simulator did not finish within ${simulator_boot_timeout_seconds}s."
			fi
		fi
		if [ "$bootstatus_status" -ne 0 ]; then
			if simulator_is_booted "$device_id"; then
				printf 'Simulator bootstatus exited with status %s, but simctl reports the selected simulator is Booted. Continuing to install/launch probe.\n' \
					"$bootstatus_status" \
					>"$artifact_dir/bootstatus-nonterminal.txt"
			else
				die "Booting the simulator failed."
			fi
		fi
		fi
		run_command_with_timeout 10 /dev/null /dev/null \
			xcrun simctl terminate "$device_id" "$bundle_id" || true
		run_command_with_timeout 10 /dev/null /dev/null \
			xcrun simctl uninstall "$device_id" "$bundle_id" || true
		local install_timeout_seconds=120
		local install_status
		set +e
		run_command_with_timeout "$install_timeout_seconds" \
			"$artifact_dir/install.stdout" \
			"$artifact_dir/install.stderr" \
			xcrun simctl install "$device_id" "$app_path"
		install_status=$?
		set -e
		if [ "$install_status" -ne 0 ]; then
			failure_stage="simulator-install"
			failure_exit_status="$install_status"
			failure_timeout_seconds="$install_timeout_seconds"
			if [ -s "$artifact_dir/install.stdout" ]; then
				failure_stdout_empty="false"
			else
				failure_stdout_empty="true"
			fi
			if [ -s "$artifact_dir/install.stderr" ]; then
				failure_stderr_empty="false"
			else
				failure_stderr_empty="true"
			fi
			die "Installing Orlix on the simulator failed."
		fi
		touch "$artifact_dir/install.json" "$artifact_dir/install.log"
		return
	fi

	xcrun devicectl --timeout 120 \
		--json-output "$artifact_dir/install.json" \
		--log-output "$artifact_dir/install.log" \
		device install app --device "$device_id" "$app_path" \
		>"$artifact_dir/install.stdout" 2>"$artifact_dir/install.stderr" ||
		die "Installing Orlix on the physical iPhone failed."
}

capture_launch() {
	local launch_pid
	set +e
		if [ "$destination" = "iphonesimulator" ] || [ "$destination" = "iOS Simulator" ]; then
			local launch_status log_stream_pid
			local launch_args=()
			touch "$artifact_dir/launch.json" "$artifact_dir/launch.log"
			simulator_launch_arguments launch_args
			run_command_with_timeout "$((capture_seconds + 15))" \
				"$artifact_dir/simulator-unified.log" \
			"$artifact_dir/simulator-unified.stderr" \
			xcrun simctl spawn "$device_id" log stream \
				--level info \
				--style compact \
				--predicate 'subsystem == "com.rudironsoni.Orlix"' &
		log_stream_pid=$!
		sleep 1
			run_command_with_timeout "$capture_seconds" \
				"$artifact_dir/launch-console.log" \
				"$artifact_dir/launch.stderr" \
				env SIMCTL_CHILD_ORLIX_SIMULATOR_CAPTURE_TERMINAL_OUTPUT=1 \
				xcrun simctl launch \
				--terminate-running-process \
					--console \
					"$device_id" \
					"$bundle_id" \
					"${launch_args[@]}"
		launch_status=$?
		xcrun simctl terminate "$device_id" "$bundle_id" >/dev/null 2>&1 || true
		wait "$log_stream_pid" || true
		collect_simulator_terminal_capture
		set -e
		if [ "$launch_status" -ne 0 ] && [ "$launch_status" -ne 124 ]; then
			failure_stage="simulator-launch"
			failure_exit_status="$launch_status"
			die "Launching Orlix on simulator failed."
		fi
		return
	else
		xcrun devicectl --timeout "$((capture_seconds + 15))" \
			--json-output "$artifact_dir/launch.json" \
			--log-output "$artifact_dir/launch.log" \
			device process launch \
			--terminate-existing \
			--console \
			--device "$device_id" \
			"$bundle_id" \
			>"$artifact_dir/launch-console.log" 2>"$artifact_dir/launch.stderr" &
	fi
	launch_pid=$!
	set -e

	sleep "$capture_seconds"
	if kill -0 "$launch_pid" >/dev/null 2>&1; then
		kill "$launch_pid" >/dev/null 2>&1 || true
	fi
	wait "$launch_pid" >/dev/null 2>&1 || true
}

collect_simulator_terminal_capture() {
	if [ "$destination" != "iphonesimulator" ] && [ "$destination" != "iOS Simulator" ]; then
		return
	fi

	local data_container
	data_container="$(xcrun simctl get_app_container "$device_id" "$bundle_id" data 2>"$artifact_dir/app-container.stderr" || true)"
	if [ -z "$data_container" ]; then
		touch "$artifact_dir/simulator-terminal-output.txt"
		return
	fi
	printf '%s\n' "$data_container" >"$artifact_dir/app-container.txt"
	local capture_path="$data_container/tmp/orlix-simulator-terminal-output.txt"
	if [ -f "$capture_path" ]; then
		cp "$capture_path" "$artifact_dir/simulator-terminal-output.txt"
	else
		touch "$artifact_dir/simulator-terminal-output.txt"
	fi
}

assert_no_host_exec_guest_text() {
	if grep -E -i 'guest.*(PROT_EXEC|EXECUTE)|attempted_prot=.*EXEC|vm_protect.*EXEC|mmap.*PROT_EXEC' \
		"$artifact_dir"/launch*.log "$artifact_dir"/launch*.stderr \
		"$artifact_dir"/simulator-unified.log "$artifact_dir"/xcodebuild.log \
		>"$artifact_dir/host-exec-violations.txt" 2>/dev/null; then
		die "Gate observed a host executable mapping request while running guest text."
	fi
}

assert_no_simulator_fatal_runtime() {
	if [ "$destination" != "iphonesimulator" ] && [ "$destination" != "iOS Simulator" ]; then
		die "Gate \`$gate\` is simulator-only."
	fi
	if grep -E 'Kernel panic|Attempted (to )?kill init|Orlix TCTI: user fault|panic - not syncing|BUG:|Oops|SIGSEGV|fatal error|Fatal error|crash|Crash|orlix-init: process signaled .* signal=[0-9]+|orlix-init: shell exit status=[1-9][0-9]*([[:space:]]|$)' \
		"$artifact_dir"/launch-console.log \
		"$artifact_dir"/launch.log \
		"$artifact_dir"/simulator-terminal-output.txt \
		"$artifact_dir"/simulator-unified.log \
		>"$artifact_dir/tcti-simulator-fatal-runtime.txt" 2>/dev/null; then
		failure_stage="tcti-simulator-stability"
		die "Simulator TCTI runtime captured a fatal post-launch error."
	fi
	touch "$artifact_dir/tcti-simulator-fatal-runtime.txt"
}

assert_gate_markers() {
	capture_tcti_first_syscall() {
		grep -F 'Orlix TCTI: svc #0' \
			"$artifact_dir/launch-console.log" \
			"$artifact_dir/launch.log" \
			"$artifact_dir/simulator-terminal-output.txt" \
			"$artifact_dir/simulator-unified.log" \
			>"$artifact_dir/tcti-first-syscall.txt" 2>/dev/null || {
			failure_stage="tcti-first-syscall-marker"
			die "No TCTI \`svc #0\` marker was captured from \`$destination\`."
		}
	}
	capture_guest_marker() {
		local marker="$1"
		local artifact_name="$2"
		local description="$3"
		grep -F "$marker" \
			"$artifact_dir/launch-console.log" \
			"$artifact_dir/launch.log" \
			"$artifact_dir/simulator-terminal-output.txt" \
			"$artifact_dir/simulator-unified.log" |
			grep -v 'Kernel command line:' |
			grep -v 'Orlix TCTI: execve argv' \
			>"$artifact_dir/$artifact_name" 2>/dev/null || {
			failure_stage="${artifact_name%.txt}-marker"
			die "$description marker was not captured from \`$destination\`."
		}
	}
	case "$gate" in
		tcti-init-first-syscall)
			capture_tcti_first_syscall
			assert_no_simulator_fatal_runtime
			;;
	tcti-simulator-stability)
		capture_tcti_first_syscall
		assert_no_simulator_fatal_runtime
		;;
		tcti-init-console-write)
			capture_tcti_first_syscall
			capture_guest_marker "$console_marker" "tcti-console-write.txt" "TCTI console write"
			assert_no_simulator_fatal_runtime
			;;
		tcti-static-busybox-start)
		capture_tcti_first_syscall
		grep -E 'Orlix TCTI: static PIE image task=sh pid=[0-9]+|Orlix TCTI: svc #0 task=sh pid=[0-9]+' \
			"$artifact_dir/launch-console.log" \
			"$artifact_dir/launch.log" \
			"$artifact_dir/simulator-terminal-output.txt" \
			"$artifact_dir/simulator-unified.log" \
			>"$artifact_dir/tcti-static-busybox-start.txt" 2>/dev/null || {
			failure_stage="tcti-static-busybox-start-marker"
			die "No static BusyBox shell TCTI start marker was captured from \`$destination\`."
		}
			assert_no_simulator_fatal_runtime
			;;
		tcti-static-busybox-shell-command)
			capture_tcti_first_syscall
			capture_guest_marker "$busybox_shell_marker" "tcti-static-busybox-shell-command.txt" "Static BusyBox shell command"
			assert_no_simulator_fatal_runtime
		;;
	tcti-full-shell-usability)
		capture_guest_marker "$full_shell_marker" "tcti-full-shell-usability.txt" "Full shell usability"
		assert_no_simulator_fatal_runtime
		;;
	tcti-mlibc-smoke)
		capture_tcti_first_syscall
		capture_guest_marker "$mlibc_smoke_marker" "tcti-mlibc-smoke.txt" "OrlixMLibC smoke"
		assert_no_simulator_fatal_runtime
		;;
	tcti-coreutils-smoke)
		capture_tcti_first_syscall
		capture_guest_marker "$coreutils_smoke_marker" "tcti-coreutils-smoke.txt" "Coreutils smoke"
		assert_no_simulator_fatal_runtime
		;;
	tcti-shell-pipeline-smoke)
		capture_guest_marker "$shell_pipeline_marker" "tcti-shell-pipeline-smoke.txt" "Shell pipeline smoke"
		assert_no_simulator_fatal_runtime
		;;
	tcti-shell-env-var-smoke)
		capture_guest_marker "$shell_env_marker" "tcti-shell-env-var-smoke.txt" "Shell env-var smoke"
		assert_no_simulator_fatal_runtime
		;;
	tcti-shell-redirection-smoke)
		capture_guest_marker "$shell_redirection_marker" "tcti-shell-redirection-smoke.txt" "Shell redirection smoke"
		assert_no_simulator_fatal_runtime
		;;
	tcti-shell-script-smoke)
		capture_guest_marker "$shell_script_marker" "tcti-shell-script-smoke.txt" "Shell script smoke"
		assert_no_simulator_fatal_runtime
		;;
	tcti-coreutils-true-false-echo)
		capture_guest_marker "$coreutils_true_false_echo_marker" "tcti-coreutils-true-false-echo.txt" "Coreutils true/false/echo"
		assert_no_simulator_fatal_runtime
		;;
	tcti-coreutils-cat-wc)
		capture_guest_marker "$coreutils_cat_wc_marker" "tcti-coreutils-cat-wc.txt" "Coreutils cat/wc"
		assert_no_simulator_fatal_runtime
		;;
	tcti-coreutils-ls-stat)
		capture_guest_marker "$coreutils_ls_stat_marker" "tcti-coreutils-ls-stat.txt" "Coreutils ls/stat"
		assert_no_simulator_fatal_runtime
		;;
	tcti-coreutils-mkdir-rm-cp-ln)
		capture_guest_marker "$coreutils_mkdir_rm_cp_ln_marker" "tcti-coreutils-mkdir-rm-cp-ln.txt" "Coreutils mkdir/rm/cp/ln"
		assert_no_simulator_fatal_runtime
		;;
	tcti-coreutils-env-path)
		capture_guest_marker "$coreutils_env_path_marker" "tcti-coreutils-env-path.txt" "Coreutils env/PATH"
		assert_no_simulator_fatal_runtime
		;;
	tcti-coreutils-test-subset)
		capture_guest_marker "$coreutils_test_subset_marker" "tcti-coreutils-test-subset.txt" "Coreutils test subset"
		assert_no_simulator_fatal_runtime
		;;
	tcti-package-behavior)
		capture_tcti_first_syscall
		capture_guest_marker "$package_behavior_marker" "tcti-package-behavior.txt" "Package behavior"
		assert_no_simulator_fatal_runtime
		;;
	tcti-oci-rootfs-command)
		capture_tcti_first_syscall
		capture_guest_marker "$oci_rootfs_command_marker" "tcti-oci-rootfs-command.txt" "OCI rootfs command"
		assert_no_simulator_fatal_runtime
		;;
	tcti-dynamic-loader-support)
			capture_tcti_first_syscall
			capture_guest_marker "$dynamic_loader_marker" "tcti-dynamic-loader-support.txt" "Dynamic loader support"
			assert_no_simulator_fatal_runtime
			;;
		tcti-signals)
			capture_tcti_first_syscall
			capture_guest_marker "$signals_marker" "tcti-signals.txt" "Signal behavior"
			assert_no_simulator_fatal_runtime
			;;
		tcti-vfs-completeness)
			capture_tcti_first_syscall
			capture_guest_marker "$vfs_marker" "tcti-vfs-completeness.txt" "VFS completeness"
			assert_no_simulator_fatal_runtime
			;;
		tcti-full-linux-runtime-readiness)
			capture_tcti_first_syscall
			capture_guest_marker "$full_runtime_marker" "tcti-full-linux-runtime-readiness.txt" "Full Linux runtime readiness"
			assert_no_simulator_fatal_runtime
			;;
		*)
		die "Gate \`$gate\` has discovery/build/install/launch plumbing, but its pass markers are not implemented yet."
		;;
	esac
}

main() {
	validate_gate
	physical_tcti_preflight

	if [ -n "$runtime_preflight_only" ]; then
		if [ "$tcti_evidence_mode" -eq 1 ]; then
			write_json_report "evidence" "false" "Preflight accepted emergency evidence mode; this is not a passing gate." "true" "$tcti_device_override_reason"
			write_report "evidence" "Preflight accepted emergency evidence mode. This is not a passing gate."
			printf 'runtime validation evidence-only preflight, report: %s\n' "$report" >&2
			exit 1
		fi
		write_json_report "evidence" "false" "Runtime validation preflight passed; no runtime gate was executed." "false" ""
		write_report "evidence" "Runtime validation preflight passed. No runtime gate was executed."
		printf 'runtime validation preflight passed without executing gate, report: %s\n' "$report"
		exit 0
	fi

	require_command xcrun
	require_command xcodebuild
	require_command xcodegen
	require_command make
	require_command python3

	select_target
	assert_single_required_simulator_booted_top_level true
	build_kernel_for_gate
	assert_tcti_kernel_config
	build_payload_for_gate
	build_app_for_target
	install_app "$app_path"
	assert_single_required_simulator_booted_top_level false
	capture_launch
	assert_no_host_exec_guest_text
	assert_gate_markers

	if [ "$tcti_evidence_mode" -eq 1 ]; then
		write_json_report "evidence" "false" "Gate evidence collected with autonomous TCTI tests bypassed. This is not a passing gate." "true" "$tcti_device_override_reason"
		write_report "evidence" "Gate evidence collected with autonomous TCTI tests bypassed. This is not a passing gate."
		printf 'runtime validation evidence-only report: %s\n' "$report" >&2
		exit 1
	fi
	write_json_report "pass" "true" "Gate \`$gate\` captured the required \`$destination\` marker." "false" ""
	write_report "passed" "Gate \`$gate\` captured the required \`$destination\` marker."
	printf 'runtime validation passed, report: %s\n' "$report"
}

main "$@"
