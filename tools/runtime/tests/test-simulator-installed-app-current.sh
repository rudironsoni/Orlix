#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
cat >"$tmp/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleShortVersionString</key><string>0.1</string>
<key>CFBundleVersion</key><string>21</string>
</dict></plist>
PLIST

ORLIX_INSTALLED_APP_INFO_PLIST="$tmp/Info.plist" \
  "$root/tools/runtime/orlix-simulator-installed-app-current.sh" SIM BUNDLE 0.1 21
if ORLIX_INSTALLED_APP_INFO_PLIST="$tmp/Info.plist" \
  "$root/tools/runtime/orlix-simulator-installed-app-current.sh" SIM BUNDLE 0.1 22; then
	echo "mismatched installed build unexpectedly accepted" >&2
	exit 1
fi
echo "pass: simulator-installed-app-current-check"
