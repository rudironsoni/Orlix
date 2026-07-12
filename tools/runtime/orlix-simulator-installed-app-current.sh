#!/usr/bin/env bash
set -euo pipefail

device_id="$1"
bundle_id="$2"
expected_version="$3"
expected_build="$4"
info_plist="${ORLIX_INSTALLED_APP_INFO_PLIST:-}"

if [ -z "$info_plist" ]; then
	container="$(xcrun simctl get_app_container "$device_id" "$bundle_id" app 2>/dev/null)" || exit 1
	info_plist="$container/Info.plist"
fi
[ -s "$info_plist" ] || exit 1

installed_version="$(plutil -extract CFBundleShortVersionString raw -o - "$info_plist" 2>/dev/null)" || exit 1
installed_build="$(plutil -extract CFBundleVersion raw -o - "$info_plist" 2>/dev/null)" || exit 1
[ "$installed_version" = "$expected_version" ] && [ "$installed_build" = "$expected_build" ]
