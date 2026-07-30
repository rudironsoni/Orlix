override ORLIX_GNU_MAKE_CONTRACT_ENTRYPOINT := OrlixKernel/Makefile
include $(CURDIR)/make/gnu-make-contract.mk

ifeq ($(ORLIX_GNU_MAKE_CONTRACT_READY),1)

include OrlixKernel/Sources/ports/orlix/kbuild/kernel-rules.mk
include $(CURDIR)/make/runtime.mk

.PHONY: orlix-tcti-native-proof-symbol-check \
	orlix-tcti-native-proof-symbol-check-dependency-regression

orlix-tcti-native-proof-symbol-check: kunit
	@set -euo pipefail; \
	$(orlix_tcti_file_sha256) \
	$(call orlix_tcti_assign_instruction_artifact_sha256,proof_artifact_sha256,exit 1) \
	[ -z "$(ORLIX_TCTI_NATIVE_PROOF_GATE_ARTIFACT_SHA256)" ] || [ "$$proof_artifact_sha256" = "$(ORLIX_TCTI_NATIVE_PROOF_GATE_ARTIFACT_SHA256)" ] || { echo 'native-proof instruction artifact digest diverges from the regression shell' >&2; exit 1; }; \
	kunit_root="$$(cd '$(ORLIX_KUNIT_BUILD_DIR)' && pwd -P)"; \
	object_root="$$kunit_root/arch/$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti"; \
	source_root="$$(cd '$(ORLIX_LINUX_OVERLAY)' && pwd -P)"; \
	port_root="$$(cd '$(ORLIX_KERNEL_PORT_DIR)' && pwd -P)"; \
	fault='$(ORLIX_TCTI_NATIVE_PROOF_GATE_FAULT)'; regression_input=''; records="$$(mktemp)"; \
	trap 'test -z "$$regression_input" || rm -f "$$regression_input"; rm -f "$$records"' EXIT; \
	cmd_record_paths() { \
		awk '\
			function emit(token) { if (token != "") printf "%s%c", token, 0; } \
			function emit_paths(line,    position, character, escaped, token) { \
				escaped = 0; token = ""; \
				for (position = 1; position <= length(line); position++) { \
					character = substr(line, position, 1); \
					if (escaped) { token = token character; escaped = 0; } \
					else if (character == "\\") escaped = 1; \
					else if (character ~ /[[:space:]]/) { emit(token); token = ""; } \
					else token = token character; \
				} \
				if (escaped) { print "unterminated Make escape in " FILENAME > "/dev/stderr"; exit 1; } \
				emit(token); \
			} \
			in_record { line = $$0; } \
			$$0 ~ "^" record " :=" { \
				if (found++) { print "duplicate " record " record in " FILENAME > "/dev/stderr"; exit 1; } \
				in_record = 1; line = substr($$0, length(record) + 4); \
			} \
			in_record { \
				sub(/^[[:space:]]+/, "", line); \
				continued = line ~ /\\$$/; \
				if (continued) sub(/[[:space:]]*\\[[:space:]]*$$/, "", line); \
				if (substr(line, 1, 2) != "$$(") emit_paths(line); \
				if (!continued) in_record = 0; \
				next; \
			} \
			END { if (found != 1 || in_record) exit 1; }' record="$$1" "$$2"; \
	}; \
	check_input() { \
		input_kind="$$1"; materialized="$$2"; expected="$$3"; \
		case "$$fault:$$input_kind" in \
			missing-source:source|missing-header:header) materialized="$$materialized.missing" ;; \
			changed-source:source|changed-header:header) regression_input="$$(mktemp)"; cp "$$materialized" "$$regression_input"; printf '\\n' >> "$$regression_input"; materialized="$$regression_input" ;; \
			stale-source:source|stale-header:header) regression_input="$$(mktemp)"; cp "$$materialized" "$$regression_input"; touch "$$regression_input"; materialized="$$regression_input" ;; \
		esac; \
		[ -f "$$materialized" ] || { echo "missing native-proof materialized $$input_kind: $$materialized" >&2; exit 1; }; \
		cmp -s "$$expected" "$$materialized" || { echo "KUnit $$input_kind is not this worktree's native-proof source: $$materialized" >&2; exit 1; }; \
		[ "$$object" -nt "$$materialized" ] || { echo "stale native-proof KUnit object for $$input_kind: $$materialized" >&2; exit 1; }; \
	}; \
	for source in target_native_proof_contract.c native_capture.c target_proof_ingestion.c; do \
		object="$$object_root/$${source%.c}.o"; \
		[ -s "$$object" ] || { echo "missing native-proof KUnit object: $$object" >&2; exit 1; }; \
		command_file="$$object_root/$$(dirname "$${source%.c}")/.$$(basename "$${source%.c}").o.cmd"; \
		[ -s "$$command_file" ] || { echo "missing native-proof Kbuild dependency artifact: $$command_file" >&2; exit 1; }; \
		object_key="arch/$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/$${source%.c}.o"; \
		cmd_record_paths "source_$$object_key" "$$command_file" > "$$records"; \
		source_count=0; \
		while IFS= read -r -d '' source_input; do \
			source_count=$$((source_count + 1)); \
			[ "$$source_count" -eq 1 ] || { echo "multiple native-proof Kbuild source inputs: $$command_file" >&2; exit 1; }; \
			case "$$source_input" in \
				/*) materialized="$$source_input" ;; \
				*) materialized="$$(cd "$$kunit_root/$$(dirname "$$source_input")" && pwd -P)/$$(basename "$$source_input")" ;; \
			esac; \
			case "$$materialized" in "$$port_root"/*) ;; *) echo "selected native-proof Kbuild source is outside the durable port: $$materialized" >&2; exit 1 ;; esac; \
			expected="$$source_root/$${materialized#"$$port_root"/}"; \
			[ -f "$$expected" ] || { echo "missing native-proof durable source: $$expected" >&2; exit 1; }; \
			[ -f "$$materialized" ] || { echo "missing native-proof materialized source: $$materialized" >&2; exit 1; }; \
			check_input source "$$materialized" "$$expected"; \
		done < "$$records"; \
		[ "$$source_count" -eq 1 ] || { echo "missing native-proof Kbuild source record: $$command_file" >&2; exit 1; }; \
		cmd_record_paths "deps_$$object_key" "$$command_file" > "$$records"; \
		[ -s "$$records" ] || { echo "empty native-proof Kbuild dependency closure: $$command_file" >&2; exit 1; }; \
		for_dependency_count=0; header_count=0; \
		while IFS= read -r -d '' dependency; do \
			for_dependency_count=$$((for_dependency_count + 1)); \
			case "$$dependency" in \
				/*) materialized="$$dependency" ;; \
				*) materialized="$$(cd "$$kunit_root/$$(dirname "$$dependency")" && pwd -P)/$$(basename "$$dependency")" ;; \
			esac; \
			case "$$materialized" in "$$port_root"/*) ;; *) continue ;; esac; \
			expected="$$source_root/$${materialized#"$$port_root"/}"; \
			[ -f "$$expected" ] || continue; \
			[ -f "$$materialized" ] || { echo "missing native-proof materialized input: $$materialized" >&2; exit 1; }; \
			case "$$expected" in *.h) header_count=$$((header_count + 1)); check_input header "$$materialized" "$$expected" ;; *) check_input dependency "$$materialized" "$$expected" ;; esac; \
		done < "$$records"; \
		[ "$$for_dependency_count" -gt 0 ] || { echo "empty native-proof Kbuild dependency closure: $$command_file" >&2; exit 1; }; \
		case "$$fault" in *-header) [ "$$header_count" -gt 0 ] || { echo "missing compiler-derived native-proof durable header: $$command_file" >&2; exit 1; } ;; esac; \
		if nm -g --defined-only "$$object" | awk '{ print $$NF }' | grep -E '(^|_)(authority|pending|reserve|commit|rollback|note_execution|normal_resume_commit|resume_user_captured)($|_)'; then \
			echo "native-proof authority leaked from $$object" >&2; exit 1; \
		fi; \
	done; \
	if rg -n '#include[[:space:]]+["<][^">]+\.c[">]' \
		'$(ORLIX_LINUX_OVERLAY)/arch/$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/target_native_proof_contract.c' \
		'$(ORLIX_LINUX_OVERLAY)/arch/$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/native_capture.c' \
		'$(ORLIX_LINUX_OVERLAY)/arch/$(ORLIX_PORT_ARCH)/hosted_exec/orlix_tcti/target_proof_ingestion.c'; then \
		echo 'native-proof fixture inputs must not include C files' >&2; exit 1; \
	fi; \
	echo "verified native-proof KUnit objects from $(ORLIX_KUNIT_BUILD_DIR)"

orlix-tcti-native-proof-symbol-check-dependency-regression:
	@set -euo pipefail; \
	$(orlix_tcti_file_sha256) \
	$(call orlix_tcti_assign_instruction_artifact_sha256,proof_artifact_sha256,exit 1) \
	output="$$(mktemp)"; \
	trap 'rm -f "$$output"' EXIT; \
	$(MAKE) --no-print-directory -f OrlixKernel/Makefile kunit; \
	for fault in missing-source changed-source stale-source missing-header changed-header stale-header; do \
		if $(MAKE) --no-print-directory -f OrlixKernel/Makefile -o kunit ORLIX_TCTI_NATIVE_PROOF_GATE_FAULT="$$fault" ORLIX_TCTI_NATIVE_PROOF_GATE_ARTIFACT_SHA256="$$proof_artifact_sha256" orlix-tcti-native-proof-symbol-check >"$$output" 2>&1; then \
			echo "native-proof dependency regression unexpectedly passed: $$fault" >&2; exit 1; \
		fi; \
		case "$$fault" in \
			missing-source) expected='missing native-proof materialized source:' ;; \
			changed-source) expected="KUnit source is not this worktree's native-proof source:" ;; \
			stale-source) expected='stale native-proof KUnit object for source:' ;; \
			missing-header) expected='missing native-proof materialized header:' ;; \
			changed-header) expected="KUnit header is not this worktree's native-proof source:" ;; \
			stale-header) expected='stale native-proof KUnit object for header:' ;; \
		esac; \
		if ! rg -F --quiet "$$expected" "$$output"; then \
			cat "$$output" >&2; \
			echo "native-proof dependency regression did not reach injected fault: $$fault" >&2; exit 1; \
		fi; \
	done; \
	echo 'verified native-proof gate rejects changed, missing, and stale compiler-derived sources and headers'

endif
