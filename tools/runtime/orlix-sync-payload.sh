#!/usr/bin/env bash
set -euo pipefail

source_payload="$1"
destination="$2"
source_stamp="$source_payload/.orlix-payload-ready"
destination_stamp="$destination/.orlix-payload-ready"

[ -s "$source_stamp" ] || { echo "missing payload identity stamp: $source_stamp" >&2; exit 1; }
if [ -s "$destination_stamp" ] && cmp -s "$source_stamp" "$destination_stamp"; then
	echo "reusing unchanged embedded OrlixOS payload: $destination"
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
rm -rf "$destination"
mv "$temporary" "$destination"
trap - EXIT
