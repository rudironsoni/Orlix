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
	OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/target_active_execution_profile_artifact.def \
	OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/inventory.def \
	OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/target_classification.def \
	OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/target_execution_slice_map.def \
	OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/source_bound_proof.def \
	OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/proof_registry_projection.def

ORLIX_TCTI_INSTRUCTION_ARTIFACT_CONTRACT_PUBLISHER_DECLARATION ?= $(ORLIX_TCTI_TARGET_REFRESH_ARTIFACTS_DECLARATION)
ORLIX_TCTI_INSTRUCTION_ARTIFACT_CONTRACT_KBUILD_DECLARATION ?= OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/Makefile
ORLIX_TCTI_INSTRUCTION_ARTIFACT_CONTRACT_INPUTS ?= $(ORLIX_TCTI_INSTRUCTION_ARTIFACT_INPUTS)
override ORLIX_TCTI_INSTRUCTION_ARTIFACT_CONTRACT_TEST_ROOT := $(CURDIR)/OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests

define orlix_tcti_instruction_artifact_contract_check
publisher_declaration="$(ORLIX_TCTI_INSTRUCTION_ARTIFACT_CONTRACT_PUBLISHER_DECLARATION)"; \
kbuild_declaration="$(ORLIX_TCTI_INSTRUCTION_ARTIFACT_CONTRACT_KBUILD_DECLARATION)"; \
canonical_inputs=( $(foreach contributor,$(ORLIX_TCTI_INSTRUCTION_ARTIFACT_CONTRACT_INPUTS),"$(contributor)") ); \
[ -s "$$publisher_declaration" ] || { echo "missing publisher declaration: $$publisher_declaration" >&2; exit 1; }; \
[ -s "$$kbuild_declaration" ] || { echo "missing direct Kbuild declaration: $$kbuild_declaration" >&2; exit 1; }; \
publisher_artifacts=( $$($(orlix_tcti_target_refresh_artifact_parser) "$$publisher_declaration") ); \
[ "$${#publisher_artifacts[@]}" -gt 0 ] || { echo 'empty publisher-owned target-refresh artifact declaration' >&2; exit 1; }; \
if awk 'function trim_start(value) { sub(/^[[:space:]]+/, "", value); return value } function check_rule(value) { if (value ~ /[.](def|h)/ && (index(value, "$$(shell") || index(value, "$$(eval") || index(value, "$$(call") || index(value, "$$(foreach") || index(value, "$$(if") || index(value, "$$(wildcard") || index(value, "$$(abspath") || index(value, "$$(realpath") || index(value, "$$(file") || index(value, "$${shell") || index(value, "$${eval") || index(value, "$${call") || index(value, "$${foreach") || index(value, "$${if") || index(value, "$${wildcard") || index(value, "$${abspath") || index(value, "$${realpath") || index(value, "$${file"))) { print value; failed = 1 } } { if (in_rule) { rule = rule " " $$0; continued = ($$0 ~ /\\[[:space:]]*$$/); if (!continued) { check_rule(rule); in_rule = 0; rule = "" }; next } line = trim_start($$0); if (index(line, "$$(obj)") == 1 && index(line, ":") > 0) { in_rule = 1; rule = $$0; continued = ($$0 ~ /\\[[:space:]]*$$/); if (!continued) { check_rule(rule); in_rule = 0; rule = "" } } } END { if (in_rule) { if (continued) { print "unterminated direct Kbuild prerequisite continuation" > "/dev/stderr"; failed = 1 } else check_rule(rule) } exit (failed ? 0 : 1) }' "$$kbuild_declaration" || grep -Eq '/\.\./\$$\([^)]*\)/[^[:space:]\\]+[.](def|h)' "$$kbuild_declaration" || grep -Eq '/\.\./\$$\{[^}]*\}/[^[:space:]\\]+[.](def|h)' "$$kbuild_declaration"; then echo 'dynamic direct Kbuild ISA path is not allowed; use a real artifact-input list' >&2; exit 1; fi; \
direct_test_root="$${ORLIX_TCTI_CONTRACT_FIXTURE_ROOT:-$(ORLIX_TCTI_INSTRUCTION_ARTIFACT_CONTRACT_TEST_ROOT)}"; \
durable_isa_root="$$(cd "$$direct_test_root/../isa" && pwd -P)" || { echo "missing durable ISA source root for direct Kbuild graph: $$direct_test_root/../isa" >&2; exit 1; }; \
durable_isa_real_root="$$(realpath "$$durable_isa_root")" || { echo "cannot resolve durable ISA source root: $$durable_isa_root" >&2; exit 1; }; \
direct_test_prefix="$$direct_test_root/../isa/"; durable_isa_prefix="$$durable_isa_root/"; durable_isa_real_prefix="$$durable_isa_real_root/"; \
graph_root="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-tcti-instruction-artifact-contract.XXXXXX")" || { echo 'cannot create direct Kbuild graph fixture' >&2; exit 1; }; \
trap 'rm -rf "$$graph_root"' EXIT; \
graph_makefile="$$graph_root/graph.mk"; graph_database="$$graph_root/make.database"; graph_paths="$$graph_root/effective.paths"; direct_artifacts_file="$$graph_root/direct.artifacts"; \
mkdir -p "$$graph_root/obj"; \
printf '%s\n' "src=$$direct_test_root" "obj=$$graph_root/obj" "ORLIX_TCTI_PROOF_REGISTRY_PROVENANCE_MAKEFILE=$(CURDIR)/make/tcti-proof-registry-provenance.mk" "include $$kbuild_declaration" '.PHONY: __orlix-tcti-direct-artifact-contract-probe' '__orlix-tcti-direct-artifact-contract-probe:' > "$$graph_makefile"; \
if ! $(MAKE) --no-print-directory --no-builtin-rules -pn -f "$$graph_makefile" __orlix-tcti-direct-artifact-contract-probe > "$$graph_database" 2>"$$graph_root/make.error"; then cat "$$graph_root/make.error" >&2; echo 'cannot evaluate direct Kbuild prerequisite graph' >&2; exit 1; fi; \
awk '/^[^#[:space:]][^:]*:/ { line = $$0; sub(/^[^:]*:[[:space:]]*/, "", line); if (line ~ /^=/) next; count = split(line, fields, /[[:space:]]+/); for (field_index = 1; field_index <= count; field_index++) if (fields[field_index] ~ /\/isa\// && fields[field_index] ~ /[.](def|h)$$/) print fields[field_index]; }' "$$graph_database" | sort -u > "$$graph_paths"; \
normalize_effective_path() { effective_path="$$1"; effective_dir="$${effective_path%/*}"; effective_name="$${effective_path##*/}"; [ "$$effective_dir" != "$$effective_path" ] || return 1; effective_dir="$$(cd "$$effective_dir" 2>/dev/null && pwd -P)" || return 1; printf '%s/%s\n' "$$effective_dir" "$$effective_name"; }; \
: > "$$direct_artifacts_file"; \
while IFS= read -r effective_path; do \
	[ -n "$$effective_path" ] || continue; \
	case "$$effective_path" in \
		"$$direct_test_prefix"*) direct_artifact="$${effective_path#$$direct_test_prefix}" ;; \
		"$$durable_isa_prefix"*) direct_artifact="$${effective_path#$$durable_isa_prefix}" ;; \
		*) echo "direct Kbuild prerequisite is outside the durable ISA source root: $$effective_path" >&2; exit 1 ;; \
	esac; \
	case "$$direct_artifact" in ''|*//*|*/./*|*/../*|./*|/*) echo "path alias in direct Kbuild prerequisite: $$effective_path" >&2; exit 1 ;; esac; \
	normalized_effective_path="$$(normalize_effective_path "$$effective_path")" || { echo "cannot normalize direct Kbuild prerequisite: $$effective_path" >&2; exit 1; }; \
	expected_effective_path="$$(normalize_effective_path "$$durable_isa_root/$$direct_artifact")" || { echo "cannot normalize canonical direct Kbuild prerequisite: $$direct_artifact" >&2; exit 1; }; \
	[ "$$normalized_effective_path" = "$$expected_effective_path" ] || { echo "direct Kbuild prerequisite aliases the durable ISA source root: $$effective_path" >&2; exit 1; }; \
	resolved_effective_path="$$(realpath "$$effective_path" 2>/dev/null)" || { echo "cannot resolve direct Kbuild prerequisite: $$effective_path" >&2; exit 1; }; \
	case "$$resolved_effective_path" in "$$durable_isa_real_prefix"*) ;; *) echo "direct Kbuild prerequisite escapes the durable ISA source root: $$effective_path -> $$resolved_effective_path" >&2; exit 1 ;; esac; \
	[ -s "$$normalized_effective_path" ] || { echo "missing direct Kbuild prerequisite: $$normalized_effective_path" >&2; exit 1; }; \
	printf '%s\n' "$$direct_artifact" >> "$$direct_artifacts_file"; \
done < "$$graph_paths"; \
sort -u "$$direct_artifacts_file" -o "$$direct_artifacts_file"; \
direct_artifacts=( $$(cat "$$direct_artifacts_file") ); \
if [ -n "$${ORLIX_TCTI_CONTRACT_FIXTURE_ROOT:-}" ]; then generation_root="$$durable_isa_root/generations/current"; isa_root="$$durable_isa_root"; else generation_root='OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa/generations/current'; isa_root='OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/isa'; fi; \
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

__orlix-tcti-target-refresh-publisher-list-source-check:
	@set -euo pipefail; \
	{ \
	$(orlix_tcti_instruction_artifact_contract_check) \
	} >/dev/null; \
	printf '%s\n' $(ORLIX_TCTI_TARGET_REFRESH_ARTIFACTS)

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
