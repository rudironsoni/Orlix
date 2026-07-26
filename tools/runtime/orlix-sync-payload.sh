#!/usr/bin/env bash
set -euo pipefail

source_payload="$1"
destination="$2"
source_stamp="$source_payload/.orlix-payload-ready"
destination_stamp="$destination/.orlix-payload-ready"
source_manifest="$source_payload/OrlixOSManifest.plist"
destination_manifest="$destination/OrlixOSManifest.plist"

[ -s "$source_stamp" ] || { echo "missing payload identity stamp: $source_stamp" >&2; exit 1; }
[ -s "$source_manifest" ] || { echo "missing payload manifest: $source_manifest" >&2; exit 1; }
if [ -s "$destination_stamp" ] && [ -s "$destination_manifest" ] && [ -d "$destination/rootfs" ] && [ -d "$destination/arch" ] && cmp -s "$source_stamp" "$destination_stamp"; then
	echo "reusing unchanged embedded OrlixOS resources: $destination"
	exit 0
fi

temporary="$destination.tmp.$$"
trap 'rm -rf "$temporary"' EXIT
rm -rf "$temporary"
mkdir -p "$temporary"
/usr/bin/rsync -a --delete "$source_payload/" "$temporary/"
cmp -s "$source_stamp" "$temporary/.orlix-payload-ready" || {
	echo "copied payload identity does not match source" >&2
	exit 1
}
mkdir -p "$destination"
/usr/bin/rsync -a --checksum --delete "$temporary/rootfs/" "$destination/rootfs/"
/usr/bin/rsync -a --checksum --delete "$temporary/arch/" "$destination/arch/"
install -m 0644 "$temporary/OrlixOSManifest.plist" "$destination_manifest"
install -m 0644 "$temporary/.orlix-payload-ready" "$destination_stamp"
echo "embedded direct OrlixOS resources: $destination"
trap - EXIT
rm -rf "$temporary"
