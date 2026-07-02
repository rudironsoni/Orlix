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
destination="${DESTINATION:-iphoneos}"
configuration="${CONFIGURATION:-Debug}"
scheme="${ORLIX_SCHEME:-Orlix}"
bundle_id="${ORLIX_APP_BUNDLE_ID:-com.rudironsoni.Orlix}"
report_dir="${REPORT_DIR:-${ORLIX_RUNTIME_REPORT_DIR:-Build/Reports/runtime}}"
device_id="${ORLIX_DEVICE_ID:-}"
simulator_id="${ORLIX_SIMULATOR_ID:-${ORLIX_BETA_SIMULATOR_ID:-}}"
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

devices_json=""
device_name=""
device_xcode_id=""
device_ddi_available=""
app_path=""
failure_stage=""
failure_exit_status=""
failure_timeout_seconds=""
failure_stdout_empty=""
failure_stderr_empty=""

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

write_json_report() {
	local status="$1"
	local passed="$2"
	local message="$3"
	local bypassed="$4"
	local bypass_reason="$5"
	local release_eligible="false"
	local readiness_eligible="false"
	if [ "$status" = "pass" ] && [ "$destination" = "iphoneos" ]; then
		release_eligible="true"
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
	local failure_context
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
	failure_context="$(failure_context_json)"
	cat >"$json_report.tmp" <<JSON
{
  "artifacts": [],
  "artifact_dir": "$escaped_artifact_dir",
  "autonomous_tests_bypassed": $bypassed,
  "backend": "tcti",
  "bypass_reason": "$escaped_reason",
  "bundle_id": "$escaped_bundle_id",
  "configuration": "$escaped_configuration",
  "counters": {},
  "coverage_warnings": [],
  "destination": "$escaped_destination",
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
  "profile": "$escaped_profile",
  "readiness_gate_eligible": $readiness_eligible,
  "release_gate_eligible": $release_eligible,
  "scheme": "$escaped_scheme",
  "selected_device_id": $(json_string_or_null "$device_id"),
  "selected_device_name": $(json_string_or_null "$device_name"),
  "selected_xcode_device_id": $(json_string_or_null "$device_xcode_id"),
  "status": "$status",
  "summary": "$escaped_message",
  "target": "$gate",
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
	[ -s "$path" ] &&
		grep -q '"status"[[:space:]]*:[[:space:]]*"pass"' "$path" &&
		grep -q '"passed"[[:space:]]*:[[:space:]]*true' "$path"
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

physical_tcti_preflight() {
	is_tcti_physical_gate || return 0
	if autonomous_tcti_reports_passed; then
		return 0
	fi
	if [ "$tcti_device_override" = "I_ACCEPT_DEVICE_DEBUG_DEBT" ]; then
		[ -n "$tcti_device_override_reason" ] ||
			die "ORLIX_TCTI_DEVICE_OVERRIDE_REASON is required when using ORLIX_TCTI_DEVICE_OVERRIDE."
		tcti_evidence_mode=1
		return 0
	fi
	die "Physical TCTI gates require passing autonomous TCTI reports before device work. Set ORLIX_TCTI_DEVICE_OVERRIDE=I_ACCEPT_DEVICE_DEBUG_DEBT and ORLIX_TCTI_DEVICE_OVERRIDE_REASON only for evidence collection."
}

validate_gate() {
	case "$gate" in
	tcti-init-first-syscall|tcti-init-console-write|tcti-static-busybox-start|tcti-dynamic-loader-start|tcti-alpine-sh-start|tcti-benchmark)
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

select_simulator() {
	if [ -n "$device_id" ]; then
		return
	fi
	if [ -n "$simulator_id" ]; then
		device_id="$simulator_id"
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
		local launch_status
		touch "$artifact_dir/launch.json" "$artifact_dir/launch.log"
		run_command_with_timeout "$capture_seconds" \
			"$artifact_dir/launch-console.log" \
			"$artifact_dir/launch.stderr" \
			xcrun simctl launch \
			--terminate-running-process \
			--console \
			"$device_id" \
			"$bundle_id"
		launch_status=$?
		set -e
		if [ "$launch_status" -ne 0 ] && [ "$launch_status" -ne 124 ]; then
			failure_stage="simulator-launch"
			failure_exit_status="$launch_status"
			die "Launching Orlix on simulator failed."
		fi
		xcrun simctl spawn "$device_id" log show \
			--last "$((capture_seconds + 60))s" \
			--info \
			--style compact \
			--predicate 'subsystem == "com.rudironsoni.Orlix"' \
			>"$artifact_dir/simulator-unified.log" \
			2>"$artifact_dir/simulator-unified.stderr" || true
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

assert_no_host_exec_guest_text() {
	if grep -E -i 'guest.*(PROT_EXEC|EXECUTE)|attempted_prot=.*EXEC|vm_protect.*EXEC|mmap.*PROT_EXEC' \
		"$artifact_dir"/launch*.log "$artifact_dir"/launch*.stderr \
		"$artifact_dir"/simulator-unified.log "$artifact_dir"/xcodebuild.log \
		>"$artifact_dir/host-exec-violations.txt" 2>/dev/null; then
		die "Gate observed a host executable mapping request while running guest text."
	fi
}

assert_gate_markers() {
	case "$gate" in
	tcti-init-first-syscall)
		grep -F 'Orlix TCTI: svc #0' \
			"$artifact_dir/launch-console.log" \
			"$artifact_dir/launch.log" \
			"$artifact_dir/simulator-unified.log" \
			>"$artifact_dir/tcti-first-syscall.txt" 2>/dev/null || {
			failure_stage="tcti-first-syscall-marker"
			die "No TCTI \`svc #0\` marker was captured from \`$destination\`."
		}
		;;
	tcti-init-console-write)
		grep -E 'linux-console|Orlix TCTI: svc #0|ORLIX|Linux version' \
			"$artifact_dir/launch-console.log" \
			"$artifact_dir/launch.log" \
			"$artifact_dir/simulator-unified.log" \
			>"$artifact_dir/tcti-console-write.txt" 2>/dev/null || {
			failure_stage="tcti-console-write-marker"
			die "No Linux console or TCTI console marker was captured from \`$destination\`."
		}
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
	build_kernel_for_gate
	assert_tcti_kernel_config
	build_app_for_target
	install_app "$app_path"
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
