# SPDX-License-Identifier: GPL-2.0-only
# Shared, build-local provenance header generation for #122 source bindings.
ORLIX_TCTI_SHASUM ?= shasum

define orlix_tcti_write_proof_registry_provenance
	set -eu; rm -f "$1.tmp."*; tmp="$1.tmp.$$$$"; decode_file="$$tmp.decode"; partition_file="$$tmp.partition"; build_file="$$tmp.build"; \
	trap 'rm -f "$$tmp" "$$decode_file" "$$partition_file" "$$build_file"' 0; \
	$(ORLIX_TCTI_SHASUM) -a 256 < "$2" > "$$decode_file"; $(ORLIX_TCTI_SHASUM) -a 256 < "$3" > "$$partition_file"; $(ORLIX_TCTI_SHASUM) -a 256 < "$4" > "$$build_file"; \
	{ IFS=' ' read -r decode decode_suffix; if IFS= read -r decode_extra; then exit 1; fi; } < "$$decode_file"; \
	{ IFS=' ' read -r partition partition_suffix; if IFS= read -r partition_extra; then exit 1; fi; } < "$$partition_file"; \
	{ IFS=' ' read -r build build_suffix; if IFS= read -r build_extra; then exit 1; fi; } < "$$build_file"; \
	test "$$decode_suffix" = '-' && test "$$partition_suffix" = '-' && test "$$build_suffix" = '-'; \
	test "$${#decode}" -eq 64 && test "$${#partition}" -eq 64 && test "$${#build}" -eq 64; \
	case "$$decode$$partition$$build" in (*[!0-9a-f]*) exit 1;; esac; \
	{ printf '%s\n' '/* generated: do not edit */'; printf '\043define ORLIX_TCTI_DECODE_SOURCE_SHA256 "%s"\n' "$$decode"; printf '\043define ORLIX_TCTI_SYSTEM_ACCESSOR_PARTITION_SOURCE_SHA256 "%s"\n' "$$partition"; printf '\043define ORLIX_TCTI_KUNIT_BUILD_SOURCE_SHA256 "%s"\n' "$$build"; } > "$$tmp"; \
	if cmp -s "$$tmp" "$1"; then rm -f "$$tmp"; else mv "$$tmp" "$1"; fi
endef
