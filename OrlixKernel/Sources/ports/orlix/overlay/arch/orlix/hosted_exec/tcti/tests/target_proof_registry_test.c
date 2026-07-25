/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_proof_registry.h"

#include <stdio.h>
#include <string.h>

#define EXPECT(value) do { \
	if (!(value)) { \
		fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value); \
		return -1; \
	} \
} while (0)

#define TRUE_CONDITION \
	"54434e440107000000220700000017070000000c010000000101010000000101010000000101010000000101"
#define LSE_CONDITION \
	"54434e4401070000002d0700000017070000000c010000000101010000000101010000000101020000000c00000008464541545f4c5345"
#define DECODE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_decode_test.c"
#define LSE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_lse_decode_test.c"
#define CSSC_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_cssc_min_max_immediate_test.c"
#define ADD_SUB_IMMEDIATE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/tcti/tests/tcti_add_sub_immediate_test.c"
#define ADD_SUB_IMMEDIATE_SUITE "orlix-tcti-add-sub-immediate"
#define CSSC_CONDITION \
	"54434e4401070000002e0700000017070000000c010000000101010000000101010000000101020000000d00000009464541545f43535343"
#define BASELINE \
	(TCTI_TARGET_PROOF_OBLIGATION_DECODE | \
	 TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS | \
	 TCTI_TARGET_PROOF_OBLIGATION_REJECTED_ENCODINGS)
#define ADD_OBLIGATIONS \
	(TCTI_TARGET_PROOF_OBLIGATION_DECODE | \
	 TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS | \
	 TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 TCTI_TARGET_PROOF_OBLIGATION_PC)
#define ADD_FLAGS_OBLIGATIONS \
	(ADD_OBLIGATIONS | TCTI_TARGET_PROOF_OBLIGATION_FLAGS)
#define LSE_OBLIGATIONS \
	(BASELINE | TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 TCTI_TARGET_PROOF_OBLIGATION_MEMORY | \
	 TCTI_TARGET_PROOF_OBLIGATION_PC | \
	 TCTI_TARGET_PROOF_OBLIGATION_FAULTS | \
	 TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY | \
	 TCTI_TARGET_PROOF_OBLIGATION_ORDERING)
#define CSSC_OBLIGATIONS \
	(BASELINE | TCTI_TARGET_PROOF_OBLIGATION_REGISTERS | \
	 TCTI_TARGET_PROOF_OBLIGATION_PC | TCTI_TARGET_PROOF_OBLIGATION_FLAGS)

static const struct tcti_target_proof_binding add_bindings[] = {
	{ "ADD_32_addsub_imm", "ADD", 0xff800000U, 0x11000000U,
	  TRUE_CONDITION, UINT64_C(0x1f), 2173U },
	{ "ADD_64_addsub_imm", "ADD", 0xff800000U, 0x91000000U,
	  TRUE_CONDITION, UINT64_C(0x1f), 2177U },
};
static const struct tcti_target_proof_case add_cases[] = {
	{ "tcti_add_sub_immediate_source_bindings",
	  TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "tcti_add_sub_immediate_all_legal_encodings",
	  TCTI_TARGET_PROOF_OBLIGATION_DECODE |
		  TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS },
	{ "tcti_add_sub_immediate_production_path_arithmetic",
	  TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "tcti_add_sub_immediate_special_register_aliases",
	  TCTI_TARGET_PROOF_OBLIGATION_REGISTERS |
		  TCTI_TARGET_PROOF_OBLIGATION_PC },
	{ "tcti_add_sub_immediate_source_mask_boundaries",
	  TCTI_TARGET_PROOF_OBLIGATION_DECODE },
};
static const struct tcti_target_proof_registry_entry add_entry = {
	"kunit:add-sub-immediate-add", "ADD_addsub_imm",
	TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, ADD_OBLIGATIONS,
	TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
	ADD_SUB_IMMEDIATE_SOURCE, ADD_SUB_IMMEDIATE_SUITE,
	add_cases, sizeof(add_cases) / sizeof(add_cases[0]), add_bindings, 2,
	NULL, ADD_OBLIGATIONS,
};

static int real_manifest_bindings_are_exact(void)
{
	enum tcti_target_proof_registry_error error;
	int result;
	struct tcti_target_proof_reference reference = {
		"kunit:add-sub-immediate-add", "ADD_32_addsub_imm", "ADD",
		"ADD_addsub_imm", 0xff800000U, 0x11000000U,
		TRUE_CONDITION, 1,
	};

	result = tcti_target_proof_registry_validate(&add_entry, 1, &error);
	if (result)
		fprintf(stderr, "registry validation error: %d\n", error);
	EXPECT(result == 0);
	EXPECT(tcti_target_proof_registry_lookup(&add_entry, 1, &reference) ==
	       TCTI_TARGET_PROOF_REGISTRY_OK);
	reference.encoding_pattern = 0x91000000U;
	EXPECT(tcti_target_proof_registry_lookup(&add_entry, 1, &reference) ==
	       TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH);
	reference.encoding_pattern = 0x11000000U;
	reference.condition_tcnd_hex = "00";
	EXPECT(tcti_target_proof_registry_lookup(&add_entry, 1, &reference) ==
	       TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH);
	{
		struct tcti_target_proof_binding invalid_binding = add_bindings[0];
		struct tcti_target_proof_registry_entry invalid = add_entry;
		enum tcti_target_proof_registry_error error;

		invalid_binding.source_ordinal = 2174U;
		invalid.bindings = &invalid_binding;
		invalid.binding_count = 1;
		EXPECT(tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
		EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH);
	}
	return 0;
}

static int registry_fails_closed_on_unknown_operations_and_bits(void)
{
	struct tcti_target_proof_registry_entry invalid = add_entry;
	enum tcti_target_proof_registry_error error;

	invalid.operation_id = "ADD";
	EXPECT(tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_UNKNOWN_FAMILY_REQUIREMENTS);
	invalid = add_entry;
	invalid.classification_mask |= 1U << 31;
	EXPECT(tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	invalid = add_entry;
	invalid.obligations |= 1U << 31;
	EXPECT(tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	return 0;
}

static int required_operation_maps_once(void)
{
	struct tcti_target_proof_registry_entry duplicate[] = {
		add_entry, add_entry,
	};
	enum tcti_target_proof_registry_error error;

	duplicate[1].id = "kunit:add-sub-immediate-duplicate";
	EXPECT(tcti_target_proof_registry_validate(duplicate, 2, &error) == -1);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	return 0;
}

static int explicit_binding_set_is_mandatory_and_unique(void)
{
	struct tcti_target_proof_registry_entry invalid = add_entry;
	struct tcti_target_proof_binding duplicate[] = {
		add_bindings[0], add_bindings[0],
	};
	enum tcti_target_proof_registry_error error;

	invalid.bindings = NULL;
	invalid.binding_count = 0;
	EXPECT(tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	invalid = add_entry;
	invalid.bindings = duplicate;
	invalid.binding_count = 2;
	EXPECT(tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	return 0;
}

static int lse_cannot_omit_atomicity(void)
{
	static const struct tcti_target_proof_binding bindings[] = {
		{ "LDADD_32_memop", "LDADD", 0xffe0fc00U, 0xb8200000U,
		  LSE_CONDITION, UINT64_C(1), 3099U },
	};
	static const struct tcti_target_proof_case cases[] = {
		{ "tcti_lse_execute_rmw_matrix",
		  LSE_OBLIGATIONS & ~TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY },
	};
	const struct tcti_target_proof_registry_entry entry = {
		"kunit:lse-ldadd", "LDADD",
		TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0,
		LSE_OBLIGATIONS & ~TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY,
		TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
		LSE_SOURCE, "orlix-tcti-lse-decode", cases, 1, bindings, 1,
		NULL,
		LSE_OBLIGATIONS & ~TCTI_TARGET_PROOF_OBLIGATION_ATOMICITY,
	};
	enum tcti_target_proof_registry_error error;

	EXPECT(tcti_target_proof_registry_validate(&entry, 1, &error) == -1);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_INSUFFICIENT_OBLIGATIONS);
	return 0;
}

static int kunit_provenance_must_be_registered(void)
{
	static const struct tcti_target_proof_case fabricated[] = {
		{ "case", ADD_OBLIGATIONS },
	};
	struct tcti_target_proof_registry_entry invalid = add_entry;
	struct tcti_target_proof_binding binding = add_bindings[0];
	enum tcti_target_proof_registry_error error;

	binding.kunit_case_mask = UINT64_C(1);
	invalid.kunit_cases = fabricated;
	invalid.kunit_case_count = 1;
	invalid.bindings = &binding;
	invalid.binding_count = 1;
	EXPECT(tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_INVALID_KUNIT_PROVENANCE);
	invalid = add_entry;
	invalid.kunit_suite = "FEAT_FIRST";
	EXPECT(tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_INVALID_KUNIT_PROVENANCE);
	invalid = add_entry;
	invalid.kunit_source = "/private/tmp/fabricated-kunit-source.c";
	EXPECT(tcti_target_proof_registry_validate(&invalid, 1, &error) == -1);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_INVALID_KUNIT_PROVENANCE);
	return 0;
}

static int registered_case_cannot_overclaim_obligations(void)
{
	static const struct tcti_target_proof_binding bindings[] = {
		{ "LDADD_32_memop", "LDADD", 0xffe0fc00U, 0xb8200000U,
		  LSE_CONDITION, UINT64_C(1), 3099U },
	};
	static const struct tcti_target_proof_case cases[] = {
		{ "tcti_gadget_executes_complete_add_sub_immediate_family",
		  LSE_OBLIGATIONS },
	};
	const struct tcti_target_proof_registry_entry entry = {
		"kunit:lse-overclaim", "LDADD",
		TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0, LSE_OBLIGATIONS,
		TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE,
		DECODE_SOURCE, "orlix-tcti-decode", cases, 1, bindings, 1,
		NULL, LSE_OBLIGATIONS,
	};
	enum tcti_target_proof_registry_error error;

	EXPECT(tcti_target_proof_registry_validate(&entry, 1, &error) == -1);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_INVALID_KUNIT_PROVENANCE);
	return 0;
}

static int typed_kselftest_provenance_is_source_and_build_bound(void)
{
	struct tcti_target_kselftest_provenance provenance = {
		"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/tcti_lse_atomic_probe.c",
		"dfe85ec0e2761dca4e15a57a0900e89ae82232351bf2f9e5deef1573f8f9bb3f",
		"OrlixKernel/Sources/ports/orlix/overlay/tools/testing/selftests/orlix/Makefile",
		"1d858a5791a52f39bd0b5616b57c8003eda2c95cda61fc2091b25d0dbfa81a9c",
		"tcti_lse_atomic_probe", "main",
	};

	EXPECT(tcti_target_kselftest_provenance_validate(&provenance) == 0);
	provenance.source_sha256 =
		"0fe85ec0e2761dca4e15a57a0900e89ae82232351bf2f9e5deef1573f8f9bb3f";
	EXPECT(tcti_target_kselftest_provenance_validate(&provenance) == -1);
	provenance.source_sha256 =
		"dfe85ec0e2761dca4e15a57a0900e89ae82232351bf2f9e5deef1573f8f9bb3f";
	provenance.build_source_sha256 =
		"0d858a5791a52f39bd0b5616b57c8003eda2c95cda61fc2091b25d0dbfa81a9c";
	EXPECT(tcti_target_kselftest_provenance_validate(&provenance) == -1);
	return 0;
}

static int privileged_profiles_and_count_caps_fail_closed(void)
{
	struct tcti_target_proof_registry_entry excessive = add_entry;
	enum tcti_target_proof_registry_error error;
	uint32_t requirements;

	EXPECT(tcti_target_proof_operation_requirements("SVC", 2,
						       &requirements) == 0);
	EXPECT((requirements & TCTI_TARGET_PROOF_OBLIGATION_FAULTS) != 0);
	EXPECT((requirements & TCTI_TARGET_PROOF_OBLIGATION_REGISTERS) != 0);
	EXPECT((requirements & TCTI_TARGET_PROOF_OBLIGATION_MEMORY) != 0);
	EXPECT(tcti_target_proof_operation_requirements("UDF_perm_undef", 3,
						       &requirements) == 0);
	EXPECT((requirements & TCTI_TARGET_PROOF_OBLIGATION_FAULTS) != 0);
	EXPECT((requirements & TCTI_TARGET_PROOF_OBLIGATION_LEGAL_ENCODINGS) == 0);
	EXPECT(tcti_target_proof_operation_requirements("not-mapped", 1,
						       &requirements) == -1);
	EXPECT(tcti_target_proof_registry_validate(&add_entry, 4351,
						  &error) == -1);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	excessive.binding_count = 4351;
	EXPECT(tcti_target_proof_registry_validate(&excessive, 1, &error) == -1);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	excessive = add_entry;
	excessive.kunit_case_count = 65;
	EXPECT(tcti_target_proof_registry_validate(&excessive, 1, &error) == -1);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_INVALID_ENTRY);
	EXPECT(tcti_target_proof_source_size_allowed(2U * 1024U * 1024U));
	EXPECT(!tcti_target_proof_source_size_allowed(
		2U * 1024U * 1024U + 1U));
	EXPECT(!tcti_target_proof_source_size_allowed(UINT64_MAX));
	return 0;
}

static int logical_shifted_register_registry_is_source_bound(void)
{
	static const char *const operations[] = {
		"AND_log_shift", "BIC_log_shift", "ORR_log_shift",
		"ORN_log_shift", "EOR_log_shift", "EON",
		"ANDS_log_shift", "BICS",
	};
	const struct tcti_target_proof_registry_entry *entries;
	enum tcti_target_proof_registry_error error;
	size_t count;
	size_t index;

	entries = tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	EXPECT(count >= sizeof(operations) / sizeof(operations[0]));
	EXPECT(tcti_target_proof_registry_validate(entries, count, &error) == 0);
	EXPECT(tcti_target_proof_registry_source_bound_projection_validate(
		       entries, count, &error) == 0);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_OK);
	for (index = 0; index < sizeof(operations) / sizeof(operations[0]); index++) {
		uint32_t requirements;

		EXPECT(!strcmp(entries[index].operation_id, operations[index]));
		EXPECT(entries[index].classification_mask ==
		       TCTI_TARGET_PROOF_CLASS_REQUIRED_EL0);
		EXPECT(entries[index].linux_interface ==
		       TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE);
		EXPECT(entries[index].binding_count == 2);
		EXPECT(tcti_target_proof_operation_requirements(
			       operations[index], 1, &requirements) == 0);
		EXPECT(entries[index].obligations == requirements);
	}
	return 0;
}

static int cssc_min_max_immediate_registry_is_source_bound(void)
{
	static const struct {
		const char *proof_id;
		const char *operation;
		const char *leaf;
		const char *mnemonic;
		uint32_t pattern;
	} bindings[] = {
		{ "kunit:cssc-min-max-immediate-smax", "SMAX_imm",
		  "SMAX_32_minmax_imm", "SMAX", 0x11c00000U },
		{ "kunit:cssc-min-max-immediate-umax", "UMAX_imm",
		  "UMAX_32U_minmax_imm", "UMAX", 0x11c40000U },
		{ "kunit:cssc-min-max-immediate-smin", "SMIN_imm",
		  "SMIN_32_minmax_imm", "SMIN", 0x11c80000U },
		{ "kunit:cssc-min-max-immediate-umin", "UMIN_imm",
		  "UMIN_32U_minmax_imm", "UMIN", 0x11cc0000U },
		{ "kunit:cssc-min-max-immediate-smax", "SMAX_imm",
		  "SMAX_64_minmax_imm", "SMAX", 0x91c00000U },
		{ "kunit:cssc-min-max-immediate-umax", "UMAX_imm",
		  "UMAX_64U_minmax_imm", "UMAX", 0x91c40000U },
		{ "kunit:cssc-min-max-immediate-smin", "SMIN_imm",
		  "SMIN_64_minmax_imm", "SMIN", 0x91c80000U },
		{ "kunit:cssc-min-max-immediate-umin", "UMIN_imm",
		  "UMIN_64U_minmax_imm", "UMIN", 0x91cc0000U },
	};
	const struct tcti_target_proof_registry_entry *entries;
	enum tcti_target_proof_registry_error error;
	size_t count;
	size_t index;

	entries = tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	EXPECT(count >= sizeof(bindings) / sizeof(bindings[0]) / 2);
	EXPECT(tcti_target_proof_registry_validate(entries, count, &error) == 0);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_OK);
	for (index = 0; index < sizeof(bindings) / sizeof(bindings[0]); index++) {
		struct tcti_target_proof_reference reference = {
			bindings[index].proof_id, bindings[index].leaf,
			bindings[index].mnemonic, bindings[index].operation,
			0xfffc0000U, bindings[index].pattern, CSSC_CONDITION, 1,
		};
		uint32_t requirements;

		EXPECT(tcti_target_proof_operation_requirements(
			       bindings[index].operation, 1, &requirements) == 0);
		EXPECT(requirements == CSSC_OBLIGATIONS);
		EXPECT(tcti_target_proof_registry_lookup(entries, count, &reference) ==
		       TCTI_TARGET_PROOF_REGISTRY_OK);
		reference.encoding_pattern ^= 0x40000U;
		EXPECT(tcti_target_proof_registry_lookup(entries, count, &reference) ==
		       TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH);
	}
	return 0;
}

static int add_sub_immediate_registry_is_source_bound(void)
{
	static const struct {
		const char *proof_id;
		const char *operation;
		const char *leaf;
		const char *mnemonic;
		uint32_t pattern;
		uint32_t obligations;
	} bindings[] = {
		{ "kunit:add-sub-immediate-add", "ADD_addsub_imm",
		  "ADD_32_addsub_imm", "ADD", 0x11000000U,
		  ADD_OBLIGATIONS },
		{ "kunit:add-sub-immediate-adds", "ADDS_addsub_imm",
		  "ADDS_32S_addsub_imm", "ADDS", 0x31000000U,
		  ADD_FLAGS_OBLIGATIONS },
		{ "kunit:add-sub-immediate-sub", "SUB_addsub_imm",
		  "SUB_32_addsub_imm", "SUB", 0x51000000U,
		  ADD_OBLIGATIONS },
		{ "kunit:add-sub-immediate-subs", "SUBS_addsub_imm",
		  "SUBS_32S_addsub_imm", "SUBS", 0x71000000U,
		  ADD_FLAGS_OBLIGATIONS },
		{ "kunit:add-sub-immediate-add", "ADD_addsub_imm",
		  "ADD_64_addsub_imm", "ADD", 0x91000000U,
		  ADD_OBLIGATIONS },
		{ "kunit:add-sub-immediate-adds", "ADDS_addsub_imm",
		  "ADDS_64S_addsub_imm", "ADDS", 0xb1000000U,
		  ADD_FLAGS_OBLIGATIONS },
		{ "kunit:add-sub-immediate-sub", "SUB_addsub_imm",
		  "SUB_64_addsub_imm", "SUB", 0xd1000000U,
		  ADD_OBLIGATIONS },
		{ "kunit:add-sub-immediate-subs", "SUBS_addsub_imm",
		  "SUBS_64S_addsub_imm", "SUBS", 0xf1000000U,
		  ADD_FLAGS_OBLIGATIONS },
	};
	const struct tcti_target_proof_registry_entry *entries;
	enum tcti_target_proof_registry_error error;
	size_t count;
	size_t index;

	entries = tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	EXPECT(tcti_target_proof_registry_validate(entries, count, &error) == 0);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_OK);
	for (index = 0; index < sizeof(bindings) / sizeof(bindings[0]); index++) {
		struct tcti_target_proof_reference reference = {
			bindings[index].proof_id, bindings[index].leaf,
			bindings[index].mnemonic, bindings[index].operation,
			0xff800000U, bindings[index].pattern,
			TRUE_CONDITION, 1,
		};
		uint32_t requirements;

		EXPECT(tcti_target_proof_operation_requirements(
			       bindings[index].operation, 1, &requirements) == 0);
		EXPECT(requirements == bindings[index].obligations);
		EXPECT(tcti_target_proof_registry_lookup(entries, count, &reference) ==
		       TCTI_TARGET_PROOF_REGISTRY_OK);
		reference.encoding_pattern ^= 0x1000000U;
		EXPECT(tcti_target_proof_registry_lookup(entries, count, &reference) ==
		       TCTI_TARGET_PROOF_REGISTRY_BINDING_MISMATCH);
	}
	return 0;
}

static int lse_registry_binds_180_leaves_without_claiming_completion(void)
{
	const struct tcti_target_proof_registry_entry *entries;
	enum tcti_target_proof_registry_error error;
	size_t binding_count = 0;
	size_t entry_count = 0;
	size_t count;
	size_t index;

	entries = tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	EXPECT(tcti_target_proof_registry_validate(entries, count, &error) == 0);
	EXPECT(tcti_target_proof_registry_source_bound_projection_validate(
		       entries, count, &error) == 0);
	for (index = 0; index < count; index++) {
		const struct tcti_target_proof_registry_entry *entry =
			&entries[index];

		if (strncmp(entry->id, "kunit:lse-base-", 15) &&
		    strncmp(entry->id, "kunit:lse128-", 13))
			continue;
		EXPECT(entry->obligations == LSE_OBLIGATIONS);
		EXPECT(entry->unproved_obligations == entry->obligations);
		EXPECT(entry->linux_interface ==
		       TCTI_TARGET_PROOF_LINUX_INTERFACE_NOT_APPLICABLE);
		entry_count++;
		binding_count += entry->binding_count;
	}
	EXPECT(entry_count == 34);
	EXPECT(binding_count == 180);
	return 0;
}

static int lse_registry_cannot_clear_unproved_duties_statically(void)
{
	const struct tcti_target_proof_registry_entry *entries;
	struct tcti_target_proof_registry_entry entry;
	enum tcti_target_proof_registry_error error;
	size_t count;

	entries = tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	entry = entries[count - 1];
	EXPECT(entry.unproved_obligations != 0);
	entry.unproved_obligations = 0;
	EXPECT(tcti_target_proof_registry_validate(&entry, 1, &error) == -1);
	EXPECT(error == TCTI_TARGET_PROOF_REGISTRY_INSUFFICIENT_OBLIGATIONS);
	return 0;
}

static int source_registration_discharges_zero_semantic_obligations(void)
{
	const struct tcti_target_proof_registry_entry *entries;
	size_t binding_count = 0;
	size_t count;
	size_t index;

	entries = tcti_target_proof_registry_entries(&count);
	EXPECT(entries != NULL);
	for (index = 0; index < count; index++) {
		const struct tcti_target_proof_registry_entry *entry =
			&entries[index];

		EXPECT(entry->obligations != 0);
		EXPECT(entry->unproved_obligations == entry->obligations);
		EXPECT((entry->obligations & ~entry->unproved_obligations) == 0);
		binding_count += entry->binding_count;
	}
	EXPECT(binding_count == 212);
	return 0;
}

int main(void)
{
	static const struct {
		const char *name;
		int (*run)(void);
	} tests[] = {
		{ "real_manifest_bindings_are_exact",
		  real_manifest_bindings_are_exact },
		{ "registry_fails_closed_on_unknown_operations_and_bits",
		  registry_fails_closed_on_unknown_operations_and_bits },
		{ "required_operation_maps_once", required_operation_maps_once },
		{ "explicit_binding_set_is_mandatory_and_unique",
		  explicit_binding_set_is_mandatory_and_unique },
		{ "lse_cannot_omit_atomicity", lse_cannot_omit_atomicity },
		{ "kunit_provenance_must_be_registered",
		  kunit_provenance_must_be_registered },
		{ "registered_case_cannot_overclaim_obligations",
		  registered_case_cannot_overclaim_obligations },
		{ "typed_kselftest_provenance_is_source_and_build_bound",
		  typed_kselftest_provenance_is_source_and_build_bound },
		{ "privileged_profiles_and_count_caps_fail_closed",
		  privileged_profiles_and_count_caps_fail_closed },
		{ "logical_shifted_register_registry_is_source_bound",
		  logical_shifted_register_registry_is_source_bound },
		{ "cssc_min_max_immediate_registry_is_source_bound",
		  cssc_min_max_immediate_registry_is_source_bound },
		{ "add_sub_immediate_registry_is_source_bound",
		  add_sub_immediate_registry_is_source_bound },
		{ "lse_registry_binds_180_leaves_without_claiming_completion",
		  lse_registry_binds_180_leaves_without_claiming_completion },
		{ "lse_registry_cannot_clear_unproved_duties_statically",
		  lse_registry_cannot_clear_unproved_duties_statically },
		{ "source_registration_discharges_zero_semantic_obligations",
		  source_registration_discharges_zero_semantic_obligations },
	};
	size_t index;

	for (index = 0; index < sizeof(tests) / sizeof(tests[0]); index++) {
		if (tests[index].run())
			return 1;
		printf("PASS %s\n", tests[index].name);
	}
	return 0;
}
