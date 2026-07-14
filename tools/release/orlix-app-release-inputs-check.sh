#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
MANIFEST="$REPO_ROOT/docs/reference/ORLIX_APP_RELEASE_INPUTS.json"

if [ "${1:-}" = "--manifest" ]; then
    [ "$#" -eq 2 ] || { echo "usage: $0 [--manifest <path>]" >&2; exit 2; }
    MANIFEST="$2"
elif [ "$#" -ne 0 ]; then
    echo "usage: $0 [--manifest <path>]" >&2
    exit 2
fi

for command in cmp jq python3 shasum xcodegen; do
    command -v "$command" >/dev/null 2>&1 || { echo "missing required command: $command" >&2; exit 1; }
done

[ -f "$MANIFEST" ] || { echo "missing release-input manifest: $MANIFEST" >&2; exit 1; }

python3 "$SCRIPT_DIR/orlix_app_capability_gate.py" validate-manifest \
    --manifest "$MANIFEST" \
    --repo-root "$REPO_ROOT"
jq -e '.schema_version == 1' "$MANIFEST" >/dev/null || { echo "unsupported release-input manifest schema" >&2; exit 1; }
jq -e '
    (.swift_packages | length > 0) and
    all(.swift_packages[]; (.revision | test("^[0-9a-f]{40}$")) and (.url | type == "string" and length > 0))
' "$MANIFEST" >/dev/null || { echo "every Swift package must use a full commit and URL" >&2; exit 1; }

tmp="$(mktemp -d "${TMPDIR:-/tmp}/orlix-release-inputs.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT

cd "$REPO_ROOT"
xcodegen dump --spec project.yml --type json > "$tmp/project.json"
jq -S '.packages' "$tmp/project.json" > "$tmp/project-packages.json"
jq -S '.swift_packages' "$MANIFEST" > "$tmp/manifest-packages.json"
if ! cmp -s "$tmp/project-packages.json" "$tmp/manifest-packages.json"; then
    echo "project.yml package definitions differ from immutable release inputs" >&2
    diff -u "$tmp/manifest-packages.json" "$tmp/project-packages.json" >&2 || true
    exit 1
fi

ghostty_commit="$(jq -r '.native_sources.ghostty.commit' "$MANIFEST")"
openssl_version="$(jq -r '.native_sources.openssl.version' "$MANIFEST")"
openssl_sha256="$(jq -r '.native_sources.openssl.archive_sha256' "$MANIFEST")"
libssh2_version="$(jq -r '.native_sources.libssh2.version' "$MANIFEST")"
libssh2_sha256="$(jq -r '.native_sources.libssh2.archive_sha256' "$MANIFEST")"

[ "$(tr -d '[:space:]' < Orlix/App/Vendor/libghostty/VERSION)" = "$ghostty_commit" ] || {
    echo "Ghostty VERSION does not match immutable release inputs" >&2
    exit 1
}
grep -Fq "GHOSTTY_REF=\"\${GHOSTTY_REF:-$ghostty_commit}\"" Orlix/App/scripts/build.sh || { echo "Ghostty build default is not pinned" >&2; exit 1; }
grep -Fq "OPENSSL_VERSION=\"$openssl_version\"" Orlix/App/scripts/build.sh || { echo "OpenSSL build version differs from release inputs" >&2; exit 1; }
grep -Fq "OPENSSL_SHA256=\"$openssl_sha256\"" Orlix/App/scripts/build.sh || { echo "OpenSSL source hash differs from release inputs" >&2; exit 1; }
grep -Fq "LIBSSH2_VERSION=\"$libssh2_version\"" Orlix/App/scripts/build.sh || { echo "libssh2 build version differs from release inputs" >&2; exit 1; }
grep -Fq "LIBSSH2_SHA256=\"$libssh2_sha256\"" Orlix/App/scripts/build.sh || { echo "libssh2 source hash differs from release inputs" >&2; exit 1; }

while IFS=$'\t' read -r path expected_sha256; do
    [ -f "$path" ] || { echo "missing vendored artifact: $path" >&2; exit 1; }
    actual_sha256="$(shasum -a 256 "$path" | awk '{print $1}')"
    [ "$actual_sha256" = "$expected_sha256" ] || {
        echo "vendored artifact hash mismatch: $path" >&2
        exit 1
    }
done < <(jq -r '.vendor_artifacts | to_entries[] | [.key, .value] | @tsv' "$MANIFEST")

while IFS= read -r path; do
    [ -f "$path" ] || { echo "missing required release evidence: $path" >&2; exit 1; }
done < <(jq -r '.required_evidence[]' "$MANIFEST")

echo "pass: Orlix application release inputs"
