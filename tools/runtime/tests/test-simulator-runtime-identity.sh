#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

cat >"$tmp/devices.json" <<'JSON'
{"devices":{"com.apple.CoreSimulator.SimRuntime.iOS-26-5":[{"udid":"SIM-1"}]}}
JSON
cat >"$tmp/runtimes.json" <<'JSON'
{"runtimes":[{"identifier":"com.apple.CoreSimulator.SimRuntime.iOS-26-5","version":"26.5","buildversion":"23F77","isAvailable":true}]}
JSON

actual="$(python3 "$root/tools/runtime/orlix-simulator-runtime-identity.py" \
  --device-id SIM-1 --devices-json "$tmp/devices.json" --runtimes-json "$tmp/runtimes.json")"
python3 - "$actual" <<'PY'
import json
import sys
assert json.loads(sys.argv[1]) == {
    "identifier": "com.apple.CoreSimulator.SimRuntime.iOS-26-5",
    "version": "26.5",
    "build": "23F77",
}
PY

if python3 "$root/tools/runtime/orlix-simulator-runtime-identity.py" \
  --device-id UNKNOWN --devices-json "$tmp/devices.json" --runtimes-json "$tmp/runtimes.json" \
  >/dev/null 2>&1; then
  echo "unknown simulator unexpectedly resolved" >&2
  exit 1
fi

echo "pass: simulator-runtime-identity-check"
