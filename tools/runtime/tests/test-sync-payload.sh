#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
source_payload="$tmp/source"
destination="$tmp/destination"
mkdir -p "$source_payload/rootfs"
printf 'identity=one\n' >"$source_payload/.orlix-payload-ready"
printf 'payload-one\n' >"$source_payload/rootfs/base.ext4"

"$root/tools/runtime/orlix-sync-payload.sh" "$source_payload" "$destination" >/dev/null
inode_before="$(stat -f '%i' "$destination/rootfs/base.ext4")"
mtime_before="$(stat -f '%m' "$destination/rootfs/base.ext4")"
"$root/tools/runtime/orlix-sync-payload.sh" "$source_payload" "$destination" >/dev/null
[ "$(stat -f '%i' "$destination/rootfs/base.ext4")" = "$inode_before" ]
[ "$(stat -f '%m' "$destination/rootfs/base.ext4")" = "$mtime_before" ]

printf 'identity=two\n' >"$source_payload/.orlix-payload-ready"
printf 'payload-two\n' >"$source_payload/rootfs/base.ext4"
"$root/tools/runtime/orlix-sync-payload.sh" "$source_payload" "$destination" >/dev/null
grep -Fxq 'payload-two' "$destination/rootfs/base.ext4"
[ ! -e "$destination.tmp.$$" ]

echo "pass: sync-payload-check"
