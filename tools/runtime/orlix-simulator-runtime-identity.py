#!/usr/bin/env python3
import argparse
import json
import subprocess
import sys


def load(path: str | None, command: list[str]) -> dict:
    if path:
        with open(path, "r", encoding="utf-8") as handle:
            return json.load(handle)
    return json.loads(subprocess.check_output(command, text=True))


parser = argparse.ArgumentParser()
parser.add_argument("--device-id", required=True)
parser.add_argument("--devices-json")
parser.add_argument("--runtimes-json")
args = parser.parse_args()

devices = load(args.devices_json, ["xcrun", "simctl", "list", "devices", "-j"])
runtimes = load(args.runtimes_json, ["xcrun", "simctl", "list", "runtimes", "-j"])

runtime_identifier = None
for identifier, entries in devices.get("devices", {}).items():
    if any(device.get("udid") == args.device_id for device in entries):
        runtime_identifier = identifier
        break

runtime = next(
    (
        item
        for item in runtimes.get("runtimes", [])
        if item.get("identifier") == runtime_identifier and item.get("isAvailable") is not False
    ),
    None,
)
if not runtime:
    print(f"cannot resolve available simulator runtime for {args.device_id}", file=sys.stderr)
    sys.exit(1)

identity = {
    "identifier": runtime["identifier"],
    "version": runtime.get("version"),
    "build": runtime.get("buildversion"),
}
if not identity["version"] or not identity["build"]:
    print(f"simulator runtime {runtime_identifier} lacks version/build metadata", file=sys.stderr)
    sys.exit(1)
json.dump(identity, sys.stdout, sort_keys=True)
sys.stdout.write("\n")
