# SPDX-License-Identifier: GPL-2.0

# Recipe-local archive reuse predicate. Callers provide archive, symbols,
# required_symbols, cache_deps, sources, objects, and depfiles shell variables.
define orlix_archive_cache_predicate
orlix_archive_cache_current() { \
	[ -s "$$archive" ] && [ -s "$$symbols" ] || return 1; \
	[ "$${#sources[@]}" -eq "$${#objects[@]}" ] || return 1; \
	[ "$${#sources[@]}" -eq "$${#depfiles[@]}" ] || return 1; \
	for symbol in "$${required_symbols[@]}"; do \
		grep -q -- "$$symbol" "$$symbols" || return 1; \
	done; \
	for dependency in "$${cache_deps[@]}"; do \
		[ -e "$$dependency" ] && [ "$$archive" -nt "$$dependency" ] || return 1; \
	done; \
	for index in "$${!sources[@]}"; do \
		source="$${sources[$$index]}"; \
		object="$${objects[$$index]}"; \
		depfile="$${depfiles[$$index]}"; \
		[ -s "$$source" ] && [ -s "$$object" ] && [ -s "$$depfile" ] || return 1; \
		[ "$$archive" -nt "$$source" ] && [ "$$archive" -nt "$$object" ] || return 1; \
		dependency_list="$$(mktemp "$${TMPDIR:-/tmp}/orlix-archive-dependencies.XXXXXX")" || return 1; \
		if ! perl -0ne 's/\\\n/ /g; s/^[^:]*:\s*//s; while (s/^\s*((?:\\.|[^\s\\])+)//s) { $$token = $$1; $$token =~ s/\\(.)/$$1/gs; print "$$token\0"; } exit 1 if /\S/;' "$$depfile" > "$$dependency_list"; then rm -f "$$dependency_list"; return 1; fi; \
		while IFS= read -r -d '' dependency; do \
			[ -n "$$dependency" ] || continue; \
			[ -e "$$dependency" ] && [ "$$archive" -nt "$$dependency" ] || { rm -f "$$dependency_list"; return 1; }; \
		done < "$$dependency_list"; \
		rm -f "$$dependency_list"; \
	done; \
	};
endef

.PHONY: __archive-cache-tests
__archive-cache-tests:
	@set -euo pipefail; \
	$(orlix_archive_cache_predicate) \
	repository_root="$(CURDIR)"; \
	kernel_rules="$$repository_root/OrlixKernel/Sources/ports/orlix/kbuild/kernel-rules.mk"; \
	root_makefile="$$repository_root/Makefile"; \
	product_adapter="$$repository_root/OrlixKernel/Sources/ports/orlix/kbuild/product-compile-adapter.mk"; \
	orlix_tcti_kbuild="$$repository_root/OrlixKernel/Sources/ports/orlix/overlay/arch/$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/Makefile"; \
	publisher_declaration="$$repository_root/OrlixKernel/Sources/ports/orlix/overlay/arch/$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/isa/build-time/target_refresh_artifacts.def"; \
	tmp="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-archive-cache.XXXXXX")"; \
	trap 'rm -rf "$$tmp"' EXIT; \
	fail() { printf 'archive cache test failed: %s\n' "$$*" >&2; exit 1; }; \
	timestamp=202607251800; \
	touch_newer() { timestamp="$$((timestamp + 1))"; touch -t "$$timestamp" "$$1"; }; \
	grep -Fq '"$$$$target_inventory_system_accessors"' "$$kernel_rules" || fail "production archive predicate omits target_system_accessor_reconciliation.def"; \
	grep -Fq '$$(orlix_archive_cache_predicate)' "$$kernel_rules" || fail "production archive predicate is not wired"; \
	grep -Fq 'CFLAGS_switch_debug.o += -march=armv8.2-a+fp16' "$$orlix_tcti_kbuild" || fail "KUnit switch_debug FP16 compiler feature is missing"; \
	grep -Fq 'CFLAGS_fixed_fp.o += -march=armv8.2-a+fp16' "$$orlix_tcti_kbuild" || fail "KUnit fixed_fp FP16 compiler feature is missing"; \
	grep -Fq 'orlix_product_adapter_source_cflags_for() {' "$$product_adapter" || fail "product adapter lacks source compiler flag resolver"; \
	grep -Fq 'arch/$$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/switch_debug.c|arch/$$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/fixed_fp.c' "$$product_adapter" || fail "product adapter FP16 source set differs from KUnit"; \
	! grep -Fq 'object_metadata_key()' "$$product_adapter" || fail "product adapter metadata cache retains nested shell command substitution"; \
	[ "$$(grep -Fc 'candidate_key=' "$$product_adapter")" -eq 3 ] || fail "product adapter metadata cache keys are not finite shell values"; \
	grep -Fq 'product_cflags="$$$$(orlix_product_adapter_source_cflags_for "$$$$src_rel")"' "$$kernel_rules" || fail "product archive does not resolve per-source compiler flags"; \
	grep -Fq '$$$$local_cflags $$$$extra_cflags $$$$product_cflags -I"$$(ORLIX_KERNEL_PORT_ABS)/arch/' "$$kernel_rules" || fail "product archive compile command omits per-source compiler flags"; \
	grep -Fq 'orlix_tcti_kbuild="$$(ORLIX_KERNEL_PORT_ABS)/arch/$$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/Makefile";' "$$kernel_rules" || fail "product archive does not name the OrlixTCTI Kbuild contract"; \
	[ "$$(grep -Fc '"$$$$orlix_tcti_kbuild"' "$$kernel_rules")" -ge 2 ] || fail "product archive does not use the OrlixTCTI Kbuild contract for archive and object freshness"; \
	grep -Fq '"$$$$obj" -nt "$$$$orlix_tcti_kbuild"' "$$kernel_rules" || fail "product archive does not rebuild stale objects after a OrlixTCTI Kbuild change"; \
	grep -Fq '"$$(ORLIX_TCTI_PROOF_PROVENANCE_SOURCE)"' "$$kernel_rules" || fail "product archive does not bind proof-provenance.mk to archive and object freshness"; \
	grep -Fq '[ "$$$$obj" -nt "$$(ORLIX_TCTI_PROOF_PROVENANCE_SOURCE)" ]' "$$kernel_rules" || fail "product archive does not rebuild stale objects after a proof-provenance helper change"; \
	grep -Fq '"$$(ORLIX_TCTI_PROOF_PROVENANCE_SOURCE)");' "$$kernel_rules" || fail "product proof identity omits proof-provenance.mk"; \
	[ "$$(grep -Fc '"$$(ORLIX_TCTI_TARGET_REFRESH_ARTIFACTS_DECLARATION)"' "$$kernel_rules")" -eq 3 ] || fail "product archive does not bind every freshness path to the publisher declaration"; \
	make_call='$$(MAKE) -f OrlixKernel/Makefile'; archive_target='__kernel-archive'; profile_ref='$$''(PROFILE)'; type_ref='$$''(type)'; libc_ref='$$''(libc)'; \
	archive_invocations="$$(rg -n --glob Makefile --glob '*.mk' -F "$$make_call $$archive_target" "$$repository_root")"; \
	[ "$$(printf '%s\n' "$$archive_invocations" | wc -l | tr -d ' ')" = 2 ] || fail "unexpected recursive __kernel-archive invocation count"; \
	grep -Fq "$$make_call $$archive_target PROFILE=release ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphoneos" "$$root_makefile" || fail "root archive invocation changed"; \
	grep -Fq "ORLIX_KERNEL_PROFILE_LOCK_HELD=1 $$make_call $$archive_target PROFILE=\"$$profile_ref\" type=\"$$type_ref\" libc=\"$$libc_ref\" ORLIX_KERNEL_ARCHIVE_PLATFORMS=iphonesimulator" "$$kernel_rules" || fail "simulator recursive archive invocation does not inherit the held profile lock"; \
	ORLIX_KERNEL_PROFILE_LOCK_HELD=1 $(MAKE) -f OrlixKernel/Makefile __profile-lock-held-probe; \
	archive="$$tmp/OrlixKernel.a"; symbols="$$tmp/symbols.txt"; source="$$tmp/source.c"; object="$$tmp/source.o"; depfile="$$tmp/source.d"; header="$$tmp/source.h"; spaced_header="$$tmp/header with space.h"; reconciliation="$$tmp/target_system_accessor_reconciliation.def"; test_kbuild="$$tmp/orlix_tcti.Makefile"; test_declaration="$$tmp/target_refresh_artifacts.def"; test_proof_provenance="$$tmp/proof-provenance.mk"; \
	printf 'source\n' > "$$source"; printf 'header\n' > "$$header"; printf 'spaced header\n' > "$$spaced_header"; printf 'reconciliation\n' > "$$reconciliation"; printf 'OrlixTCTI kbuild\n' > "$$test_kbuild"; printf 'publisher declaration\n' > "$$test_declaration"; printf 'proof provenance helper\n' > "$$test_proof_provenance"; printf 'object\n' > "$$object"; \
	printf '%s: %s %s %s\n' "$$object" "$$source" "$$header" "$${spaced_header// /\\ }" > "$$depfile"; \
	printf '_arch_boot_entry\n_arch_boot_params\n' > "$$symbols"; printf 'archive\n' > "$$archive"; \
	touch -t 202607251759 "$$source" "$$object" "$$header" "$$spaced_header" "$$depfile" "$$symbols" "$$reconciliation" "$$test_kbuild" "$$test_declaration" "$$test_proof_provenance"; touch_newer "$$archive"; \
	required_symbols=(_arch_boot_entry _arch_boot_params); cache_deps=("$$reconciliation" "$$test_kbuild" "$$test_declaration" "$$test_proof_provenance"); sources=("$$source"); objects=("$$object"); depfiles=("$$depfile"); \
	assert_current() { orlix_archive_cache_current; }; \
	assert_stale() { if assert_current; then fail "$$1 remained reusable"; fi; return 0; }; \
	assert_current; assert_current; \
	touch_newer "$$reconciliation"; assert_stale "reconciliation artifact"; touch_newer "$$archive"; assert_current; \
	touch_newer "$$test_kbuild"; assert_stale "OrlixTCTI Kbuild contract"; touch_newer "$$archive"; assert_current; \
	touch_newer "$$test_declaration"; assert_stale "target-refresh publisher declaration"; touch_newer "$$archive"; assert_current; \
	touch_newer "$$test_proof_provenance"; assert_stale "proof-provenance helper"; touch_newer "$$archive"; assert_current; \
	touch_newer "$$header"; assert_stale "depfile dependency"; touch_newer "$$archive"; assert_current; \
	touch_newer "$$spaced_header"; assert_stale "escaped-space depfile dependency"; touch_newer "$$archive"; assert_current; \
	printf '%s: %s \\' "$$object" "$$source" > "$$depfile"; assert_stale "unterminated dependency escape"; printf '%s: %s %s %s\n' "$$object" "$$source" "$$header" "$${spaced_header// /\\ }" > "$$depfile"; touch_newer "$$archive"; assert_current; \
	rm "$$depfile"; assert_stale "missing depfile"; printf '%s: %s %s\n' "$$object" "$$source" "$$header" > "$$depfile"; touch_newer "$$archive"; assert_current; \
	touch_newer "$$source"; assert_stale "newer source"; touch_newer "$$archive"; assert_current; \
	printf '_arch_boot_entry\n' > "$$symbols"; assert_stale "missing archive symbol"; \
	printf 'archive cache freshness test: passed\n'
