#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
set -euo pipefail

repository_root="$(cd "$(dirname "$0")/../../../../.." && pwd)"
predicate="$repository_root/OrlixKernel/Sources/ports/orlix/kbuild/archive-cache.sh"
kernel_rules="$repository_root/OrlixKernel/Sources/ports/orlix/kbuild/kernel-rules.mk"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/orlix-archive-cache.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT

fail() { printf 'archive cache test failed: %s\n' "$*" >&2; exit 1; }
timestamp=202607251800
touch_newer() {
	timestamp="$((timestamp + 1))"
	touch -t "$timestamp" "$1"
}

grep -Fq '"$$target_inventory_system_accessors"' "$kernel_rules" ||
	fail "production archive predicate omits target_system_accessor_reconciliation.def"
grep -Fq '"$$archive_cache" "$${archive_cache_args[@]}"' "$kernel_rules" ||
	fail "production archive predicate is not wired"

archive="$tmp/OrlixKernel.a"; symbols="$tmp/symbols.txt"; source="$tmp/source.c"; object="$tmp/source.o"; depfile="$tmp/source.d"; header="$tmp/source.h"; reconciliation="$tmp/target_system_accessor_reconciliation.def"
printf 'source\n' > "$source"; printf 'header\n' > "$header"; printf 'reconciliation\n' > "$reconciliation"; printf 'object\n' > "$object"
printf '%s: %s %s\n' "$object" "$source" "$header" > "$depfile"
printf '_arch_boot_entry\n_arch_boot_params\n' > "$symbols"
printf 'archive\n' > "$archive"
touch -t 202607251759 "$source" "$object" "$header" "$depfile" "$symbols" "$reconciliation"
touch_newer "$archive"

assert_current() { "$predicate" --archive "$archive" --symbols "$symbols" --symbol _arch_boot_entry --symbol _arch_boot_params --cache-dep "$reconciliation" --source "$source" --object "$object" --depfile "$depfile"; }
assert_stale() { if assert_current; then fail "$1 remained reusable"; fi; }

assert_current
assert_current
touch_newer "$reconciliation"; assert_stale "reconciliation artifact"; touch_newer "$archive"; assert_current
touch_newer "$header"; assert_stale "depfile dependency"; touch_newer "$archive"; assert_current
rm "$depfile"; assert_stale "missing depfile"; printf '%s: %s %s\n' "$object" "$source" "$header" > "$depfile"; touch_newer "$archive"; assert_current
touch_newer "$source"; assert_stale "newer source"; touch_newer "$archive"; assert_current
printf '_arch_boot_entry\n' > "$symbols"; assert_stale "missing archive symbol"
printf 'archive cache freshness test: passed\n'
