#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
CHECK="$REPO_ROOT/tools/release/orlix-app-release-inputs-check.sh"
MANIFEST="$REPO_ROOT/docs/sources/release/orlix-app-release-inputs.json"
VENDOR_BUILD="$REPO_ROOT/Orlix/App/scripts/build.sh"

"$CHECK"

tmp="$(mktemp -d "${TMPDIR:-/tmp}/orlix-release-inputs-test.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
jq '.swift_packages.OpenTelemetrySwift.revision = "0000000000000000000000000000000000000000"' "$MANIFEST" > "$tmp/bad-manifest.json"

if "$CHECK" --manifest "$tmp/bad-manifest.json" > "$tmp/stdout" 2> "$tmp/stderr"; then
    echo "expected mismatched package manifest to fail" >&2
    exit 1
fi
grep -Fq "project.yml package definitions differ from immutable release inputs" "$tmp/stderr"

source "$VENDOR_BUILD"
printf '%s' "verified archive" > "$tmp/archive.tar.gz"
archive_sha256="$(shasum -a 256 "$tmp/archive.tar.gz" | awk '{print $1}')"
download_verified_archive "unused" "$tmp/archive.tar.gz" "$archive_sha256"

if (download_verified_archive "unused" "$tmp/archive.tar.gz" "0000000000000000000000000000000000000000000000000000000000000000") > "$tmp/stdout" 2> "$tmp/stderr"; then
    echo "expected source archive hash mismatch to fail" >&2
    exit 1
fi
grep -Fq "SHA-256 mismatch" "$tmp/stdout"

if (GHOSTTY_REF=custom-io build_ghosttykit) > "$tmp/stdout" 2> "$tmp/stderr"; then
    echo "expected branch-only Ghostty input to fail" >&2
    exit 1
fi
grep -Fq "GHOSTTY_REF must be a full 40-character commit" "$tmp/stdout"

echo "pass: Orlix application release-input checks fail closed"
