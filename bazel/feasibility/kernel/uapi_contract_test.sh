#!/bin/sh
set -eu
root="${1:-}"
if [ -z "$root" ]; then
  for candidate in bazel-bin/bazel/feasibility/kernel/uapi "$(dirname "$0")/uapi"; do
    if [ -d "$candidate" ]; then
      root="$candidate"
      break
    fi
  done
fi
test -n "$root"
test -s "$root/uapi/include/linux/unistd.h"
test -s "$root/uapi/include/asm/unistd.h"
test -s "$root/uapi.sha256"
test -s "$root/kbuild-archive.tar"
arch="$(/usr/bin/sed -n 's/.*"arch": "\([^"]*\)".*/\1/p' "$root/manifest.json" | /usr/bin/head -n 1)"
test "$arch" = "arm64"
digest="$(/usr/bin/tr -d '[:space:]' < "$root/uapi.sha256")"
test "${#digest}" -eq 64
expected="$(/usr/bin/python3 "$(dirname "$0")/../../content_digest.py" "$root/uapi/include")"
test "$digest" = "$expected"
manifest_digest="$(/usr/bin/sed -n 's/.*"uapi_digest": "\([^"]*\)".*/\1/p' "$root/manifest.json" | /usr/bin/head -n 1)"
test "$digest" = "$manifest_digest"
echo "uapi contract ok arch=$arch digest=$digest"
