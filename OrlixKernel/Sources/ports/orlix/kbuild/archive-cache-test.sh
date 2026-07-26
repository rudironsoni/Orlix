#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
set -euo pipefail

repository_root="$(cd "$(dirname "$0")/../../../../.." && pwd)"
predicate="$repository_root/OrlixKernel/Sources/ports/orlix/kbuild/archive-cache.sh"
kernel_rules="$repository_root/OrlixKernel/Sources/ports/orlix/kbuild/kernel-rules.mk"
product_adapter="$repository_root/OrlixKernel/Sources/ports/orlix/kbuild/product-compile-adapter.mk"
orlix_tcti_kbuild="$repository_root/OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/Makefile"
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

grep -Fq 'CFLAGS_switch_debug.o += -march=armv8.2-a+fp16' "$orlix_tcti_kbuild" || fail "KUnit switch_debug FP16 compiler feature is missing"
grep -Fq 'CFLAGS_fixed_fp.o += -march=armv8.2-a+fp16' "$orlix_tcti_kbuild" || fail "KUnit fixed_fp FP16 compiler feature is missing"
grep -Fq 'orlix_product_adapter_source_cflags_for() {' "$product_adapter" || fail "product adapter lacks source compiler flag resolver"
grep -Fq 'arch/$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/switch_debug.c|arch/$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/fixed_fp.c' "$product_adapter" || fail "product adapter FP16 source set differs from KUnit"
grep -Fq 'product_cflags="$$(orlix_product_adapter_source_cflags_for "$$src_rel")"' "$kernel_rules" || fail "product archive does not resolve per-source compiler flags"
grep -Fq '$$local_cflags $$extra_cflags $$product_cflags -I"$(ORLIX_KERNEL_PORT_ABS)/arch/' "$kernel_rules" || fail "product archive compile command omits per-source compiler flags"
grep -Fq 'orlix_tcti_kbuild="$(ORLIX_KERNEL_PORT_ABS)/arch/$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/Makefile";' "$kernel_rules" || fail "product archive does not name the OrlixTCTI Kbuild contract"
[ "$(grep -Fc '"$$orlix_tcti_kbuild"' "$kernel_rules")" -ge 2 ] || fail "product archive does not use the OrlixTCTI Kbuild contract for archive and object freshness"
grep -Fq '"$$obj" -nt "$$orlix_tcti_kbuild"' "$kernel_rules" || fail "product archive does not rebuild stale objects after a OrlixTCTI Kbuild change"

archive="$tmp/OrlixKernel.a"; symbols="$tmp/symbols.txt"; source="$tmp/source.c"; object="$tmp/source.o"; depfile="$tmp/source.d"; header="$tmp/source.h"; reconciliation="$tmp/target_system_accessor_reconciliation.def"; orlix_tcti_makefile="$tmp/orlix_tcti.Makefile"
printf 'source\n' > "$source"; printf 'header\n' > "$header"; printf 'reconciliation\n' > "$reconciliation"; printf 'OrlixTCTI kbuild\n' > "$orlix_tcti_makefile"; printf 'object\n' > "$object"
printf '%s: %s %s\n' "$object" "$source" "$header" > "$depfile"
printf '_arch_boot_entry\n_arch_boot_params\n' > "$symbols"
printf 'archive\n' > "$archive"
touch -t 202607251759 "$source" "$object" "$header" "$depfile" "$symbols" "$reconciliation" "$orlix_tcti_makefile"
touch_newer "$archive"

assert_current() { "$predicate" --archive "$archive" --symbols "$symbols" --symbol _arch_boot_entry --symbol _arch_boot_params --cache-dep "$reconciliation" --cache-dep "$orlix_tcti_makefile" --source "$source" --object "$object" --depfile "$depfile"; }
assert_stale() { if assert_current; then fail "$1 remained reusable"; fi; }

assert_current
assert_current
touch_newer "$reconciliation"; assert_stale "reconciliation artifact"; touch_newer "$archive"; assert_current
touch_newer "$orlix_tcti_makefile"; assert_stale "OrlixTCTI Kbuild contract"; touch_newer "$archive"; assert_current
touch_newer "$header"; assert_stale "depfile dependency"; touch_newer "$archive"; assert_current
rm "$depfile"; assert_stale "missing depfile"; printf '%s: %s %s\n' "$object" "$source" "$header" > "$depfile"; touch_newer "$archive"; assert_current
touch_newer "$source"; assert_stale "newer source"; touch_newer "$archive"; assert_current
printf '_arch_boot_entry\n' > "$symbols"; assert_stale "missing archive symbol"
printf 'archive cache freshness test: passed\n'
