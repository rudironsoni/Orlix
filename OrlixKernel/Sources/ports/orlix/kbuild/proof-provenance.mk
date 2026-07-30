# Hash the exact durable source candidate consumed by a proof-producing build.
# The NUL-delimited path list includes index and working-tree content, so a
# staged or unstaged source change cannot inherit HEAD's identity.
define orlix_tcti_candidate_source_revision
orlix_tcti_candidate_source_revision() { \
	candidate_root="$$1"; \
	candidate_paths="$$2"; \
	candidate_tmp_root="$${TMPDIR:-$(ORLIX_BUILD_ROOT)/tmp}"; \
	mkdir -p "$$candidate_tmp_root"; \
	candidate_manifest="$$(mktemp "$$candidate_tmp_root/orlix-tcti-candidate-source.XXXXXX")" || return 1; \
	if ! printf 'orlix-tcti-durable-source-candidate-v1\0' > "$$candidate_manifest"; then rm -f "$$candidate_manifest"; return 1; fi; \
	while IFS= read -r -d '' candidate_path; do \
		case "$$candidate_path" in ''|/*|../*|*/../*|*/..) echo "invalid OrlixTCTI durable source candidate path: $$candidate_path" >&2; rm -f "$$candidate_manifest"; return 1 ;; esac; \
		candidate_source="$$candidate_root/$$candidate_path"; \
		printf '%s\0' "$$candidate_path" >> "$$candidate_manifest" || { rm -f "$$candidate_manifest"; return 1; }; \
		if [ -L "$$candidate_source" ]; then \
			candidate_target="$$candidate_manifest.target"; \
			if ! readlink -n "$$candidate_source" > "$$candidate_target"; then rm -f "$$candidate_target" "$$candidate_manifest"; return 1; fi; \
			if ! { printf 'symlink\0'; cat "$$candidate_target"; printf '\0'; } >> "$$candidate_manifest"; then rm -f "$$candidate_target" "$$candidate_manifest"; return 1; fi; \
			rm -f "$$candidate_target" || { rm -f "$$candidate_manifest"; return 1; }; \
		else \
			[ -f "$$candidate_source" ] || { echo "missing OrlixTCTI durable source candidate: $$candidate_source" >&2; rm -f "$$candidate_manifest"; return 1; }; \
			candidate_digest="$$(orlix_tcti_file_sha256 "$$candidate_source")" || { rm -f "$$candidate_manifest"; return 1; }; \
			printf 'file\0%s\0' "$$candidate_digest" >> "$$candidate_manifest" || { rm -f "$$candidate_manifest"; return 1; }; \
		fi; \
	done < "$$candidate_paths"; \
	candidate_revision="$$(orlix_tcti_file_sha256 "$$candidate_manifest")" || { rm -f "$$candidate_manifest"; return 1; }; \
	rm -f "$$candidate_manifest"; \
	printf '%s\n' "$$candidate_revision"; \
};
endef

define orlix_tcti_manifest_sha256
orlix_tcti_manifest_sha256() { \
	orlix_tcti_file_sha256 "$$1"; \
};
endef

define orlix_tcti_proof_profile_sha256
orlix_tcti_proof_profile_sha256() { \
	proof_profile="$$1"; proof_profile_config="$$2"; proof_tmp_root="$${TMPDIR:-$(ORLIX_BUILD_ROOT)/tmp}"; \
	mkdir -p "$$proof_tmp_root"; \
	proof_profile_manifest="$$(mktemp "$$proof_tmp_root/orlix-tcti-proof-profile.XXXXXX")" || return 1; \
	if ! { printf 'profile=%s\n' "$$proof_profile" > "$$proof_profile_manifest"; orlix_tcti_file_sha256 "$$proof_profile_config" >> "$$proof_profile_manifest"; }; then rm -f "$$proof_profile_manifest"; return 1; fi; \
	proof_profile_digest="$$(orlix_tcti_manifest_sha256 "$$proof_profile_manifest")" || { rm -f "$$proof_profile_manifest"; return 1; }; \
	rm -f "$$proof_profile_manifest"; \
	printf '%s\n' "$$proof_profile_digest"; \
};
endef

define orlix_tcti_proof_inputs_sha256
orlix_tcti_proof_inputs_sha256() { \
	proof_prefix="$$1"; shift; proof_tmp_root="$${TMPDIR:-$(ORLIX_BUILD_ROOT)/tmp}"; \
	mkdir -p "$$proof_tmp_root"; \
	proof_inputs_manifest="$$(mktemp "$$proof_tmp_root/orlix-tcti-proof-inputs.XXXXXX")" || return 1; \
	if ! printf '%s' "$$proof_prefix" > "$$proof_inputs_manifest"; then rm -f "$$proof_inputs_manifest"; return 1; fi; \
	for proof_input in "$$@"; do orlix_tcti_file_sha256 "$$proof_input" >> "$$proof_inputs_manifest" || { rm -f "$$proof_inputs_manifest"; return 1; }; done; \
	proof_inputs_digest="$$(orlix_tcti_manifest_sha256 "$$proof_inputs_manifest")" || { rm -f "$$proof_inputs_manifest"; return 1; }; \
	rm -f "$$proof_inputs_manifest"; \
	printf '%s\n' "$$proof_inputs_digest"; \
};
endef
