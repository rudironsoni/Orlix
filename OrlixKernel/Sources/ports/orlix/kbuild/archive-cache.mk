# SPDX-License-Identifier: GPL-2.0

define orlix_archive_cache_predicate
orlix_archive_symbol_manifest_has_exact_external_definition() { \
	[ "$$#" -eq 2 ] || return 1; \
	local symbols="$$1" required_symbol="$$2"; \
	[ -s "$$symbols" ] || return 1; \
	awk -v required_symbol="$$required_symbol" '\
		function is_external_definition(type) { return type ~ /^(A|B|C|D|G|I|R|S|T|V|W)$$/ } \
		{ type = ""; name = "" } \
		NF == 3 && $$1 ~ /^[[:xdigit:]]+$$/ { type = $$2; name = $$3 } \
		is_external_definition(type) && name == required_symbol { found = 1 } \
		END { exit !found } \
	' "$$symbols"; \
	};
endef

.PHONY: __archive-cache-tests
__archive-cache-tests:
	@set -euo pipefail; \
	$(strip $(orlix_archive_cache_predicate)) \
	repository_root="$(CURDIR)"; \
	kernel_rules="$$repository_root/OrlixKernel/Sources/ports/orlix/kbuild/kernel-rules.mk"; \
	root_makefile="$$repository_root/Makefile"; \
	product_adapter="$$repository_root/OrlixKernel/Sources/ports/orlix/kbuild/product-compile-adapter.mk"; \
	orlix_tcti_kbuild="$$repository_root/OrlixKernel/Sources/ports/orlix/overlay/arch/$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/Makefile"; \
	publisher_declaration="$$repository_root/OrlixKernel/Sources/ports/orlix/overlay/arch/$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/isa/build-time/target_refresh_artifacts.def"; \
	tmp="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-archive-cache.XXXXXX")"; \
	trap 'rm -rf "$$tmp"' EXIT; \
	fail() { printf 'archive cache test failed: %s\n' "$$*" >&2; exit 1; }; \
	grep -Fq '"$$$$target_inventory_system_accessors"' "$$kernel_rules" || fail "production archive predicate omits target_system_accessor_reconciliation.def"; \
	grep -Fq '$$(orlix_archive_cache_predicate)' "$$kernel_rules" || fail "production archive predicate is not wired"; \
	grep -Fq 'orlix_archive_symbol_manifest_has_exact_external_definition "$$$$symbols_tmp" "$$$$required_symbol"' "$$kernel_rules" || fail "production archive finalization does not require exact external definitions"; \
	grep -Fq 'CFLAGS_switch_debug.o += -march=armv8.2-a+fp16' "$$orlix_tcti_kbuild" || fail "KUnit switch_debug FP16 compiler feature is missing"; \
	grep -Fq 'CFLAGS_fixed_fp.o += -march=armv8.2-a+fp16' "$$orlix_tcti_kbuild" || fail "KUnit fixed_fp FP16 compiler feature is missing"; \
	grep -Fq 'orlix_product_adapter_source_cflags_for() {' "$$product_adapter" || fail "product adapter lacks source compiler flag resolver"; \
	grep -Fq 'arch/$$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/switch_debug.c|arch/$$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/fixed_fp.c' "$$product_adapter" || fail "product adapter FP16 source set differs from KUnit"; \
	! grep -Fq 'object_metadata_key()' "$$product_adapter" || fail "product adapter metadata cache retains nested shell command substitution"; \
	[ "$$(grep -Fc 'candidate_key=' "$$product_adapter")" -eq 3 ] || fail "product adapter metadata cache keys are not finite shell values"; \
	grep -Fq 'product_cflags="$$$$(orlix_product_adapter_source_cflags_for "$$$$src_rel")"' "$$kernel_rules" || fail "product archive does not resolve per-source compiler flags"; \
	grep -Fq '$$$$local_cflags $$$$extra_cflags $$$$product_cflags -I"$$(ORLIX_KERNEL_PORT_ABS)/arch/' "$$kernel_rules" || fail "product archive compile command omits per-source compiler flags"; \
	grep -Fq 'orlix_tcti_kbuild="$$(ORLIX_KERNEL_PORT_ABS)/arch/$$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/Makefile";' "$$kernel_rules" || fail "product archive does not name the OrlixTCTI Kbuild contract"; \
	grep -Fq '"$$(ORLIX_TCTI_PROOF_PROVENANCE_SOURCE)"' "$$kernel_rules" || fail "product archive does not bind proof-provenance.mk to archive and object freshness"; \
	grep -Fq '"$$(ORLIX_TCTI_PROOF_PROVENANCE_SOURCE)" "$$$$target_proof_registry_provenance_header"' "$$kernel_rules" || fail "product proof identity omits provenance header"; \
	grep -Fq '"$$$$target_proof_registry_provenance_header"' "$$kernel_rules" || fail "product archive does not bind provenance header into freshness"; \
	make_call='$$(MAKE) -f OrlixKernel/Makefile'; archive_target='__kernel-archive'; profile_ref='$$''(PROFILE)'; type_ref='$$''(type)'; libc_ref='$$''(libc)'; \
	archive_invocations="$$(rg -n --glob Makefile --glob '*.mk' -F "$$make_call $$archive_target" "$$repository_root")"; \
	[ "$$(printf '%s\n' "$$archive_invocations" | wc -l | tr -d ' ')" = 2 ] || fail "unexpected recursive __kernel-archive invocation count"; \
	grep -Fq "$$make_call $$archive_target PROFILE=release ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphoneos" "$$root_makefile" || fail "root archive invocation changed"; \
	grep -Fq "ORLIX_KERNEL_PROFILE_LOCK_HELD=1 $$make_call $$archive_target PROFILE=\"$$profile_ref\" type=\"$$type_ref\" libc=\"$$libc_ref\" ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphonesimulator" "$$kernel_rules" || fail "simulator recursive archive invocation does not inherit the held profile lock"; \
	ORLIX_KERNEL_PROFILE_LOCK_HELD=1 $(MAKE) -f OrlixKernel/Makefile __profile-lock-held-probe; \
	write_actual_symbol_manifest() { printf '%s\n' '0000000000000000 T _arch_boot_entry' '0000000000000000 T _arch_boot_params' 'orlix-product-kernel.o:' '0000000000018c48 T _orlix_tcti_system_accessor_decode' '0000000000018db0 T _orlix_tcti_execute_system_register' 'orlix-product-kernel.o:' '0000000000018c14 T _orlix_tcti_system_accessor_decode' '0000000000018d7c T _orlix_tcti_execute_system_register' > "$$symbols"; }; \
	symbols="$$tmp/symbols.txt"; write_actual_symbol_manifest; \
	assert_exact_external_definition() { orlix_archive_symbol_manifest_has_exact_external_definition "$$symbols" "$$1" || fail "$$2"; }; \
	assert_missing_external_definition() { if orlix_archive_symbol_manifest_has_exact_external_definition "$$symbols" "$$1"; then fail "$$2"; fi; return 0; }; \
	for symbol in _orlix_tcti_system_accessor_decode _orlix_tcti_execute_system_register; do \
		assert_exact_external_definition "$$symbol" "exact external definition rejected for $$symbol"; \
		printf 'T %s\n' "$$symbol" > "$$symbols"; assert_missing_external_definition "$$symbol" "two-field text definition accepted for $$symbol"; \
		printf 'D %s\n' "$$symbol" > "$$symbols"; assert_missing_external_definition "$$symbol" "two-field data definition accepted for $$symbol"; \
		printf 'orlix-product-kernel.o:\n' > "$$symbols"; assert_missing_external_definition "$$symbol" "archive member header accepted for $$symbol"; \
		printf 'orlix-product-kernel.o:\n0000000000018c48 T %s\n' "$$symbol" > "$$symbols"; assert_exact_external_definition "$$symbol" "header plus exact three-field definition rejected for $$symbol"; \
		write_actual_symbol_manifest; \
		sed -i.bak "s/$$symbol/$${symbol}_suffix/" "$$symbols"; rm -f "$$symbols.bak"; assert_missing_external_definition "$$symbol" "suffix lookalike accepted for $$symbol"; \
		write_actual_symbol_manifest; \
		sed -i.bak "s/$$symbol/prefix_$${symbol}/" "$$symbols"; rm -f "$$symbols.bak"; assert_missing_external_definition "$$symbol" "prefix lookalike accepted for $$symbol"; \
		write_actual_symbol_manifest; \
		sed -i.bak "s/0000000000018c48 T $$symbol/                 U $$symbol/; s/0000000000018db0 T $$symbol/                 U $$symbol/; s/0000000000018c14 T $$symbol/                 U $$symbol/; s/0000000000018d7c T $$symbol/                 U $$symbol/" "$$symbols"; rm -f "$$symbols.bak"; assert_missing_external_definition "$$symbol" "undefined-only lookalike accepted for $$symbol"; \
		write_actual_symbol_manifest; \
		sed -i.bak "s/0000000000018c48 T $$symbol/malformed T $$symbol/; s/0000000000018db0 T $$symbol/malformed T $$symbol/; s/0000000000018c14 T $$symbol/malformed T $$symbol/; s/0000000000018d7c T $$symbol/malformed T $$symbol/" "$$symbols"; rm -f "$$symbols.bak"; assert_missing_external_definition "$$symbol" "malformed address accepted for $$symbol"; \
		write_actual_symbol_manifest; \
		sed -i.bak "s/0000000000018c48 T $$symbol/nonhex T $$symbol/; s/0000000000018db0 T $$symbol/nonhex T $$symbol/; s/0000000000018c14 T $$symbol/nonhex T $$symbol/; s/0000000000018d7c T $$symbol/nonhex T $$symbol/" "$$symbols"; rm -f "$$symbols.bak"; assert_missing_external_definition "$$symbol" "nonhex address accepted for $$symbol"; \
		write_actual_symbol_manifest; \
		sed -i.bak "s/0000000000018c48 T $$symbol/extra 0000000000018c48 T $$symbol/; s/0000000000018db0 T $$symbol/extra 0000000000018db0 T $$symbol/; s/0000000000018c14 T $$symbol/extra 0000000000018c14 T $$symbol/; s/0000000000018d7c T $$symbol/extra 0000000000018d7c T $$symbol/" "$$symbols"; rm -f "$$symbols.bak"; assert_missing_external_definition "$$symbol" "extra leading field accepted for $$symbol"; \
		write_actual_symbol_manifest; \
		sed -i.bak "s/0000000000018c48 T $$symbol/0000000000018c48 T $$symbol trailing/; s/0000000000018db0 T $$symbol/0000000000018db0 T $$symbol trailing/; s/0000000000018c14 T $$symbol/0000000000018c14 T $$symbol trailing/; s/0000000000018d7c T $$symbol/0000000000018d7c T $$symbol trailing/" "$$symbols"; rm -f "$$symbols.bak"; assert_missing_external_definition "$$symbol" "extra trailing field accepted for $$symbol"; \
	done; \
	printf 'archive symbol and compile contract tests: passed\n'
