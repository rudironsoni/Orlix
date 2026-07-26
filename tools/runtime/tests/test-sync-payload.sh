#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
source_payload="$tmp/source"
destination="$tmp/destination"
mkdir -p "$source_payload/rootfs" "$source_payload/arch/orlix"
printf 'identity=one\n' >"$source_payload/.orlix-payload-ready"
printf 'manifest-one\n' >"$source_payload/OrlixOSManifest.plist"
printf 'payload-one\n' >"$source_payload/rootfs/base.ext4"
printf 'architecture-one\n' >"$source_payload/arch/orlix/identity"
mkdir -p "$destination"
printf 'framework-owned\n' >"$destination/preserved"

"$root/tools/runtime/orlix-sync-payload.sh" "$source_payload" "$destination" >/dev/null
[ -f "$destination/OrlixOSManifest.plist" ]
[ ! -e "$destination/Info.plist" ]
[ -f "$destination/preserved" ]
inode_before="$(stat -f '%i' "$destination/rootfs/base.ext4")"
mtime_before="$(stat -f '%m' "$destination/rootfs/base.ext4")"
"$root/tools/runtime/orlix-sync-payload.sh" "$source_payload" "$destination" >/dev/null
[ "$(stat -f '%i' "$destination/rootfs/base.ext4")" = "$inode_before" ]
[ "$(stat -f '%m' "$destination/rootfs/base.ext4")" = "$mtime_before" ]

printf 'identity=two\n' >"$source_payload/.orlix-payload-ready"
printf 'manifest-two\n' >"$source_payload/OrlixOSManifest.plist"
printf 'payload-two\n' >"$source_payload/rootfs/base.ext4"
"$root/tools/runtime/orlix-sync-payload.sh" "$source_payload" "$destination" >/dev/null
grep -Fxq 'payload-two' "$destination/rootfs/base.ext4"
grep -Fxq 'manifest-two' "$destination/OrlixOSManifest.plist"
[ -f "$destination/preserved" ]
[ ! -e "$destination.tmp.$$" ]

echo "pass: sync-payload-check"
