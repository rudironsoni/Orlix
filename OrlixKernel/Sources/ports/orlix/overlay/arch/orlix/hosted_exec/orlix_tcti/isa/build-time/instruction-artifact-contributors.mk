# SPDX-License-Identifier: GPL-2.0-only
#
# Ordered durable contributors to ORLIX_TCTI_INSTRUCTION_ARTIFACT_SHA256.
# Keep publisher outputs in target_refresh_artifacts.def order, then direct Kbuild inputs.
ORLIX_TCTI_INSTRUCTION_ARTIFACT_INPUTS_DECLARATION := $(lastword $(MAKEFILE_LIST))
ORLIX_TCTI_TARGET_REFRESH_ARTIFACTS_DECLARATION := $(dir $(ORLIX_TCTI_INSTRUCTION_ARTIFACT_INPUTS_DECLARATION))target_refresh_artifacts.def

ifneq ($(origin ORLIX_TCTI_INSTRUCTION_ARTIFACT_INPUTS),undefined)
$(error ORLIX_TCTI_INSTRUCTION_ARTIFACT_INPUTS is repository-defined and cannot be overridden)
endif

orlix_tcti_target_refresh_artifact_parser = awk '/^[[:space:]]*$$/ || /^[[:space:]]*(\#|\/\/|\/\*|\*|\*\/)/ { next } /^[[:space:]]*ORLIX_TCTI_TARGET_REFRESH_ARTIFACT[(][[:alpha:]_][[:alnum:]_]*,[[:alnum:]_.-]+,[[:alpha:]_][[:alnum:]_]*[)][[:space:]]*(\#.*|\/\/.*|\/\*.*\*\/)?[[:space:]]*$$/ { row = $$0; sub(/^[[:space:]]*ORLIX_TCTI_TARGET_REFRESH_ARTIFACT[(]/, "", row); sub(/[)][[:space:]]*(\#.*|\/\/.*|\/\*.*\*\/)?[[:space:]]*$$/, "", row); split(row, fields, ","); if (seen[fields[2]]++) { printf "%s:%d: duplicate target-refresh publisher artifact: %s\\n", FILENAME, FNR, fields[2] > "/dev/stderr"; failed = 1; next } print fields[2]; count++; next } { printf "%s:%d: malformed target-refresh artifact declaration\\n", FILENAME, FNR > "/dev/stderr"; failed = 1 } END { if (failed || !count) exit 1 }'

# The parser is also used by the contract check below. Keep this as the sole
# grammar for X-macro rows so valid whitespace and comments cannot diverge.
ORLIX_TCTI_TARGET_REFRESH_ARTIFACTS := $(shell $(orlix_tcti_target_refresh_artifact_parser) '$(ORLIX_TCTI_TARGET_REFRESH_ARTIFACTS_DECLARATION)')
ifneq ($(.SHELLSTATUS),0)
$(error failed to parse target-refresh publisher declaration: $(ORLIX_TCTI_TARGET_REFRESH_ARTIFACTS_DECLARATION))
endif
ORLIX_TCTI_INSTRUCTION_ARTIFACT_INPUTS := \
	OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/generations/current/manifest \
	$(addprefix OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/generations/current/,$(ORLIX_TCTI_TARGET_REFRESH_ARTIFACTS)) \
	OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/inventory.def \
	OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/target_classification.def \
	OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/target_execution_slice_map.def \
	OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/source_bound_proof.def \
	OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/proof_registry_projection.def

ORLIX_TCTI_INSTRUCTION_ARTIFACT_CONTRACT_PUBLISHER_DECLARATION ?= $(ORLIX_TCTI_TARGET_REFRESH_ARTIFACTS_DECLARATION)
ORLIX_TCTI_INSTRUCTION_ARTIFACT_CONTRACT_KBUILD_DECLARATION ?= OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/Makefile
ORLIX_TCTI_INSTRUCTION_ARTIFACT_CONTRACT_INPUTS ?= $(ORLIX_TCTI_INSTRUCTION_ARTIFACT_INPUTS)

define orlix_tcti_instruction_artifact_contract_check
publisher_declaration="$(ORLIX_TCTI_INSTRUCTION_ARTIFACT_CONTRACT_PUBLISHER_DECLARATION)"; \
kbuild_declaration="$(ORLIX_TCTI_INSTRUCTION_ARTIFACT_CONTRACT_KBUILD_DECLARATION)"; \
canonical_inputs=( $(foreach contributor,$(ORLIX_TCTI_INSTRUCTION_ARTIFACT_CONTRACT_INPUTS),"$(contributor)") ); \
[ -s "$$publisher_declaration" ] || { echo "missing publisher declaration: $$publisher_declaration" >&2; exit 1; }; \
[ -s "$$kbuild_declaration" ] || { echo "missing direct Kbuild declaration: $$kbuild_declaration" >&2; exit 1; }; \
publisher_artifacts=( $$($(orlix_tcti_target_refresh_artifact_parser) "$$publisher_declaration") ); \
[ "$${#publisher_artifacts[@]}" -gt 0 ] || { echo 'empty publisher-owned target-refresh artifact declaration' >&2; exit 1; }; \
direct_artifacts=( $$(grep -oE 'isa/(generations/current/)?[^[:space:]\\]+[.](def|h)' "$$kbuild_declaration" | sed 's#^isa/##' | sort -u) ); \
generation_root='OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/generations/current'; \
isa_root='OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa'; \
seen_canonical=(); \
for canonical_input in "$${canonical_inputs[@]}"; do \
	[ -n "$$canonical_input" ] || { echo 'empty canonical contributor' >&2; exit 1; }; \
	[[ "$$canonical_input" != *//* && "$$canonical_input" != */./* && "$$canonical_input" != */../* && "$$canonical_input" != ./* && "$$canonical_input" != /* ]] || { echo "path alias in canonical contributor: $$canonical_input" >&2; exit 1; }; \
	for seen_input in "$${seen_canonical[@]:-}"; do [ "$$canonical_input" != "$$seen_input" ] || { echo "duplicate canonical contributor: $$canonical_input" >&2; exit 1; }; done; \
	seen_canonical+=("$$canonical_input"); \
done; \
for publisher_artifact in "$${publisher_artifacts[@]}"; do \
	found=0; for canonical_input in "$${canonical_inputs[@]}"; do [ "$$canonical_input" = "$$generation_root/$$publisher_artifact" ] && found=1; done; \
	[ "$$found" -eq 1 ] || { echo "publisher artifact absent from canonical declaration: $$publisher_artifact" >&2; exit 1; }; \
done; \
for direct_artifact in "$${direct_artifacts[@]}"; do \
	found=0; for canonical_input in "$${canonical_inputs[@]}"; do [ "$$canonical_input" = "$$isa_root/$$direct_artifact" ] && found=1; done; \
	[ "$$found" -eq 1 ] || { echo "direct Kbuild input absent from canonical declaration: $$direct_artifact" >&2; exit 1; }; \
done; \
for canonical_input in "$${canonical_inputs[@]}"; do \
	[ "$$canonical_input" = "$$generation_root/manifest" ] && continue; \
	owned=0; for publisher_artifact in "$${publisher_artifacts[@]}"; do [ "$$canonical_input" = "$$generation_root/$$publisher_artifact" ] && owned=1; done; \
	for direct_artifact in "$${direct_artifacts[@]}"; do [ "$$canonical_input" = "$$isa_root/$$direct_artifact" ] && owned=1; done; \
	[ "$$owned" -eq 1 ] || { echo "canonical contributor has no exact publisher or direct Kbuild owner: $$canonical_input" >&2; exit 1; }; \
done;
endef

# This exact digest serialization is shared by every proof consumer.  Keep the
# per-file digest records path-free: the ordered declaration above owns identity.
define orlix_tcti_file_sha256
orlix_tcti_file_sha256() { \
	digest_path="$$1"; \
	if ! digest_output="$$(shasum -a 256 < "$$digest_path" && printf x)"; then return 1; fi; \
	[ "$${digest_output: -1}" = x ] || return 1; \
	digest_output="$${digest_output%x}"; \
	[ "$${#digest_output}" -eq 68 ] || return 1; \
	digest="$${digest_output:0:64}"; \
	[[ "$$digest" =~ ^[[:xdigit:]]{64}$$ ]] || return 1; \
	[ "$${digest_output:64}" = '  -'$$'\n' ] || return 1; \
	printf '%s\n' "$$digest"; \
};
endef

# Compute and assign the canonical digest in one expansion so no callable
# helper can be consumed outside this fail-closed path.
define orlix_tcti_assign_instruction_artifact_sha256
$(1)="$$( \
	proof_artifact_inputs=( $(foreach proof_artifact_input,$(ORLIX_TCTI_INSTRUCTION_ARTIFACT_INPUTS),"$(proof_artifact_input)") ); \
	proof_tmp_root="$${TMPDIR:-$(ORLIX_BUILD_ROOT)/tmp}"; \
	mkdir -p "$$proof_tmp_root" || exit 1; \
	proof_artifact_manifest="$$(mktemp "$$proof_tmp_root/orlix-tcti-instruction-artifacts.XXXXXX")" || exit 1; \
	for proof_artifact_input in "$${proof_artifact_inputs[@]}"; do \
		orlix_tcti_file_sha256 "$$proof_artifact_input" >> "$$proof_artifact_manifest" || { rm -f "$$proof_artifact_manifest"; exit 1; }; \
	done; \
	proof_artifact_digest="$$(orlix_tcti_file_sha256 "$$proof_artifact_manifest")" || { rm -f "$$proof_artifact_manifest"; exit 1; }; \
	rm -f "$$proof_artifact_manifest" || exit 1; \
	printf '%s\n' "$$proof_artifact_digest" \
)" || $(2);
endef

.PHONY: __orlix-tcti-instruction-artifact-inputs-source-check __orlix-tcti-instruction-artifact-cleanup-failure-source-check __orlix-tcti-instruction-artifact-contract-check __orlix-tcti-target-refresh-publisher-list-source-check
__orlix-tcti-instruction-artifact-contract-check:
	@set -euo pipefail; \
	$(orlix_tcti_instruction_artifact_contract_check)

__orlix-tcti-target-refresh-publisher-list-source-check: __orlix-tcti-instruction-artifact-contract-check
	@printf '%s\n' $(ORLIX_TCTI_TARGET_REFRESH_ARTIFACTS)

__orlix-tcti-instruction-artifact-inputs-source-check: __orlix-tcti-instruction-artifact-contract-check
	@set -euo pipefail; \
	[ "$(origin ORLIX_TCTI_INSTRUCTION_ARTIFACT_INPUTS)" = file ] || { echo 'instruction artifact inputs must originate in the canonical declaration' >&2; exit 1; }; \
	contributors=( $(foreach contributor,$(ORLIX_TCTI_INSTRUCTION_ARTIFACT_INPUTS),"$(contributor)") ); \
	for contributor in "$${contributors[@]}"; do [ -s "$$contributor" ] || { echo "missing instruction artifact contributor: $$contributor" >&2; exit 1; }; done; \
	$(orlix_tcti_file_sha256) \
	$(call orlix_tcti_assign_instruction_artifact_sha256,proof_artifact_sha256,exit 1) \
	printf '%s\0' "$${contributors[@]}" | shasum -a 256 | awk '{ print "instruction artifact contributor declaration: " $$1 }'; \
	printf 'instruction artifact sha256: %s\n' "$$proof_artifact_sha256"

__orlix-tcti-instruction-artifact-cleanup-failure-source-check:
	@set -euo pipefail; \
	fixture_root="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-tcti-artifact-cleanup.XXXXXX")"; \
	trap '/bin/rm -rf "$$fixture_root"' EXIT; \
	mkdir -p "$$fixture_root/bin"; \
	printf '%s\n' '#!/bin/sh' 'printf "%s\\n" "$$*" >> "$$ORLIX_TCTI_CLEANUP_FAILURE_LOG"' 'exit 97' > "$$fixture_root/bin/rm"; \
	chmod +x "$$fixture_root/bin/rm"; \
	$(orlix_tcti_file_sha256) \
	proof_artifact_sha256=''; cleanup_action=0; \
	PATH="$$fixture_root/bin:$$PATH"; export PATH; ORLIX_TCTI_CLEANUP_FAILURE_LOG="$$fixture_root/rm.log"; export ORLIX_TCTI_CLEANUP_FAILURE_LOG; TMPDIR="$$fixture_root"; export TMPDIR; \
	$(call orlix_tcti_assign_instruction_artifact_sha256,proof_artifact_sha256,cleanup_action=1) \
	[ "$$cleanup_action" -eq 1 ] || { echo 'instruction artifact cleanup failure did not invoke the caller action' >&2; exit 1; }; \
	[ -z "$$proof_artifact_sha256" ] || { echo 'instruction artifact cleanup failure emitted a digest' >&2; exit 1; }; \
	grep -Eq '^-f .*/orlix-tcti-instruction-artifacts\.' "$$fixture_root/rm.log" || { echo 'instruction artifact cleanup failure did not attempt manifest removal' >&2; exit 1; }; \
	find "$$fixture_root" -type f -name 'orlix-tcti-instruction-artifacts.*' -print -quit | grep -q . || { echo 'forced manifest-removal failure did not retain its manifest fixture' >&2; exit 1; }; \
	printf '%s\n' 'instruction artifact cleanup failure source check: passed'
