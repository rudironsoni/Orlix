// SPDX-License-Identifier: GPL-2.0-only
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/elf.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/utsname.h>
#include <target_inventory.h>

#include "../decode_aarch64.h"
#include "orlix_tcti_test_suites.h"
#include "orlix_tcti_source_leaf_rejection_catalog.h"
#include "orlix_tcti_native_observation.h"
#include "target_proof_registry.h"

#define SOURCE_LEAF_SVC 0xd4000001U

enum orlix_tcti_system_leaf_el0_classification {
	ORLIX_TCTI_SYSTEM_LEAF_EL0_VARIANT_REQUIRED,
	ORLIX_TCTI_SYSTEM_LEAF_NON_EL0_REJECTION,
	ORLIX_TCTI_SYSTEM_LEAF_FEATURE_CONDITIONED_EL0_PARTITION_REQUIRED,
};

enum orlix_tcti_system_leaf_relation {
	ORLIX_TCTI_SYSTEM_LEAF_RELATION_NONE,
};

enum orlix_tcti_system_leaf_implementation_status {
	ORLIX_TCTI_SYSTEM_LEAF_PENDING,
	ORLIX_TCTI_SYSTEM_LEAF_REJECTION_IMPLEMENTED,
};

enum orlix_tcti_system_leaf_proof_status {
	ORLIX_TCTI_SYSTEM_LEAF_UNPROVED,
	ORLIX_TCTI_SYSTEM_LEAF_PROVED,
};

struct system_leaf_classification {
	u32 ordinal;
	const char *name;
	const char *operation;
	const char *feature_predicate;
	enum orlix_tcti_system_leaf_el0_classification el0_classification;
	enum orlix_tcti_system_leaf_relation relation;
	const char *asl_operation;
	const char *owner;
	const char *proof_id;
	enum orlix_tcti_system_leaf_implementation_status implementation_status;
	enum orlix_tcti_system_leaf_proof_status proof_status;
};

#define ORLIX_TCTI_SYSTEM_LEAF_CLASSIFICATION(ordinal, name, operation, feature, \
					el0_classification, relation, asl_operation, \
					owner, proof_id, implementation_status, \
					proof_status) \
	{ ordinal, name, operation, feature, el0_classification, relation, \
	  asl_operation, owner, proof_id, implementation_status, proof_status },
static const struct system_leaf_classification system_leaf_classifications[] = {
#include "../isa/system_leaf_classification.def"
};
#undef ORLIX_TCTI_SYSTEM_LEAF_CLASSIFICATION

static void orlix_tcti_system_leaf_catalog_tracks_authoritative_fanout(
	struct kunit *test)
{
	static const u32 expected_ordinals[] = {
		2281U, 2282U, 2283U, 2284U, 2285U, 2286U, 2287U,
	};
	static const char * const expected_features[] = {
		"true", "true", "true", "true", "FEAT_SYSINSTR128",
		"FEAT_SYSREG128", "FEAT_SYSREG128",
	};
	size_t index;

	KUNIT_ASSERT_EQ(test, ARRAY_SIZE(expected_ordinals),
				ARRAY_SIZE(system_leaf_classifications));
	for (index = 0; index < ARRAY_SIZE(system_leaf_classifications); index++) {
		const struct system_leaf_classification *leaf =
			&system_leaf_classifications[index];

		KUNIT_EXPECT_EQ_MSG(test, expected_ordinals[index], leaf->ordinal,
				    "system catalog index %zu", index);
		KUNIT_EXPECT_STREQ_MSG(test, expected_features[index],
				       leaf->feature_predicate,
				       "source ordinal %u", leaf->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_SYSTEM_LEAF_RELATION_NONE,
				    leaf->relation, "source ordinal %u", leaf->ordinal);
		KUNIT_EXPECT_NOT_NULL(test, leaf->name);
		KUNIT_EXPECT_NOT_NULL(test, leaf->operation);
		KUNIT_EXPECT_NOT_NULL(test, leaf->asl_operation);
		KUNIT_EXPECT_NOT_NULL(test, leaf->owner);
		KUNIT_EXPECT_NOT_NULL(test, leaf->proof_id);
	}

	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_SYSTEM_LEAF_EL0_VARIANT_REQUIRED,
			system_leaf_classifications[1].el0_classification);

	for (index = 0; index < ARRAY_SIZE(system_leaf_classifications); index++) {
			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_SYSTEM_LEAF_PENDING,
					    system_leaf_classifications[index].implementation_status,
					    "source ordinal %u",
					    system_leaf_classifications[index].ordinal);
			KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_SYSTEM_LEAF_UNPROVED,
					    system_leaf_classifications[index].proof_status,
					    "source ordinal %u",
					    system_leaf_classifications[index].ordinal);
	}
}

static unsigned long source_leaf_map(struct kunit *test, u32 instruction)
{
	u32 program[] = { instruction, SOURCE_LEAF_SVC };
	unsigned long mapped;
	int ret;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	ret = orlix_tcti_write_user_data(current->mm, mapped, program,
				   sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_EQ(test, 0, ret);
	return mapped;
}

#include "orlix_tcti_system_accessor_partition_test.h"

static bool orlix_tcti_source_leaf_is_base_exception(
	const struct orlix_tcti_source_leaf_rejection *leaf)
{
	return leaf->ordinal == 2166U ||
	       (leaf->ordinal >= 2228U && leaf->ordinal <= 2234U);
}

static const char *source_leaf_proof_id(u32 ordinal)
{
	size_t index;

	for (index = 0;
	     index < ARRAY_SIZE(orlix_tcti_source_leaf_proof_bindings); index++)
		if (orlix_tcti_source_leaf_proof_bindings[index].ordinal == ordinal)
			return orlix_tcti_source_leaf_proof_bindings[index].proof_id;
	return NULL;
}

static void source_leaf_capture_fp_simd(
	struct orlix_tcti_native_fp_simd_state *state)
{
	memset(state, 0, sizeof(*state));
	memcpy(state->v, current->thread.user_simd, sizeof(state->v));
	state->fpcr = current->thread.user_fpcr;
	state->fpsr = current->thread.user_fpsr;
	state->valid = true;
}

static void source_leaf_capture_sve(struct orlix_tcti_native_sve_state *state)
{
	const struct orlix_tcti_sve_state *sve = &current->thread.user_sve;

	*state = (struct orlix_tcti_native_sve_state) {
		.vl_bytes = sve->vl_bytes,
		.z = &sve->z[0][0],
		.p = &sve->p[0][0],
		.ffr = sve->ffr,
		.valid = sve->valid,
	};
}

static void source_leaf_ingest_observation(
	struct kunit *test, const char *proof_id, u32 ordinal,
	struct orlix_tcti_target_native_result_record *record)
{
	const char *case_name =
		"orlix_tcti_source_leaf_base_exceptions_emit_typed_observations";
	struct orlix_tcti_target_kunit_provenance_identity provenance;
	struct orlix_tcti_target_native_ingestion_selector selector = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_target_proof_ingestion_summary summary;
	const struct orlix_tcti_target_proof_registry_entry *entries;
	const struct orlix_tcti_target_proof_registry_entry *entry = NULL;
	const struct orlix_tcti_target_proof_binding *binding = NULL;
	enum orlix_tcti_target_proof_ingestion_error error;
	char kernel_identity[ORLIX_TCTI_TARGET_PROOF_BUILD_ID_MAX];
	size_t entry_count;
	size_t entry_index;
	size_t binding_index;

	entries = orlix_tcti_target_proof_registry_entries(&entry_count);
	KUNIT_ASSERT_NOT_NULL(test, entries);
	for (entry_index = 0; entry_index < entry_count; entry_index++)
		if (!strcmp(entries[entry_index].id, proof_id)) {
			entry = &entries[entry_index];
			break;
		}
	KUNIT_ASSERT_NOT_NULL(test, entry);
	for (binding_index = 0; binding_index < entry->binding_count;
	     binding_index++)
		if (entry->bindings[binding_index].source_ordinal == ordinal) {
			binding = &entry->bindings[binding_index];
			break;
		}
	KUNIT_ASSERT_NOT_NULL(test, binding);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_kunit_provenance_identity(
			entry, case_name, &provenance));
	scnprintf(kernel_identity, sizeof(kernel_identity), "%s|%s|%s",
		 init_utsname()->release, init_utsname()->version,
		 init_utsname()->machine);
	selector = (struct orlix_tcti_target_native_ingestion_selector) {
		.proof_id = proof_id,
		.classification_mask = entry->classification_mask,
		.condition_tcnd_hex = binding->condition_tcnd_hex,
		.kunit_source = provenance.source,
		.kunit_source_sha256 = provenance.source_sha256,
		.kunit_build_source = provenance.build_source,
		.kunit_build_source_sha256 = provenance.build_source_sha256,
		.kunit_suite = provenance.suite,
		.kunit_case = provenance.case_name,
		.executing_kernel_identity = kernel_identity,
	};
	ledger = orlix_tcti_target_proof_ingestion_ledger_create(1);
	KUNIT_ASSERT_NOT_NULL(test, ledger);
	KUNIT_EXPECT_EQ(test, 0, orlix_tcti_target_proof_ingest_native(
		ledger, record, &selector, &error));
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_TARGET_PROOF_INGEST_OK, error);
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_target_proof_ingestion_summary(ledger, &summary));
	KUNIT_EXPECT_EQ(test, 1UL, summary.accepted_records);
	KUNIT_EXPECT_EQ(test, 1UL, summary.native_passed);
	orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
}

static void source_leaf_emit_observation(
	struct kunit *test, const struct orlix_tcti_source_leaf_rejection *leaf,
	enum orlix_tcti_native_obligation obligation)
{
	struct orlix_tcti_native_observation_spec spec = {};
	struct orlix_tcti_native_observation *observation;
	struct orlix_tcti_target_native_result_record *record = NULL;
	struct orlix_tcti_native_fp_simd_state fp_simd;
	struct orlix_tcti_native_sve_state sve;
	struct orlix_tcti_native_memory_state memory;
	struct orlix_tcti_native_fault_witness fault;
	struct pt_regs regs = {};
	u32 observed_instruction = 0;
	unsigned long mapped = source_leaf_map(test, leaf->pattern);
	const char *proof_id = source_leaf_proof_id(leaf->ordinal);

	regs.pc = mapped;
	regs.sp = STACK_TOP - 16;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[0] = 0x123456789abcdef0ULL;
	spec.source_ordinal = leaf->ordinal;
	spec.obligation = obligation;
	spec.result.reason = ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION;
	spec.result.status = 0;
	spec.result.fault_access = ORLIX_TCTI_ACCESS_FETCH;
	spec.result.pc = mapped;
	spec.result.instruction = leaf->pattern;
	orlix_tcti_native_gpr_capture(&spec.gpr, &regs);
	if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY) {
		spec.expected.memory.address = mapped;
		spec.expected.memory.size = sizeof(leaf->pattern);
		spec.expected.memory.bytes = (const u8 *)&leaf->pattern;
	} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD) {
		source_leaf_capture_fp_simd(&spec.expected.fp_simd);
	} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_SVE) {
		source_leaf_capture_sve(&spec.expected.sve);
	} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_FAULT) {
		spec.expected.fault = (struct orlix_tcti_native_fault_witness) {
			.address = mapped,
			.access = ORLIX_TCTI_ACCESS_FETCH,
			.valid = true,
			.occurred = true,
			.precise = true,
		};
	}
	observation = orlix_tcti_native_observation_create(&spec);
	KUNIT_ASSERT_NOT_NULL(test, observation);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_native_observation_execute(
		observation, current, &regs, current->mm));
	if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY) {
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(
			current->mm, mapped, &observed_instruction,
			sizeof(observed_instruction)));
		memory = (struct orlix_tcti_native_memory_state) {
			.address = mapped,
			.size = sizeof(observed_instruction),
			.bytes = (const u8 *)&observed_instruction,
		};
		KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_memory(observation, &memory));
	} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD) {
		source_leaf_capture_fp_simd(&fp_simd);
		KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_fp_simd(observation,
				&fp_simd));
	} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_SVE) {
		source_leaf_capture_sve(&sve);
		KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_sve(observation, &sve));
	} else if (obligation == ORLIX_TCTI_NATIVE_OBLIGATION_FAULT) {
		fault = spec.expected.fault;
		KUNIT_ASSERT_EQ(test, 0,
			orlix_tcti_native_observation_add_fault(observation, &fault));
	}
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_compare(observation));
	KUNIT_ASSERT_EQ(test, 0,
		orlix_tcti_native_observation_export(observation, &record));
	KUNIT_ASSERT_NOT_NULL(test, record);
	KUNIT_ASSERT_NOT_NULL(test, proof_id);
	source_leaf_ingest_observation(test, proof_id, leaf->ordinal, record);
	orlix_tcti_target_native_result_record_destroy(record);
	orlix_tcti_native_observation_destroy(observation);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_source_leaf_base_exceptions_emit_typed_observations(
	struct kunit *test)
{
	static const enum orlix_tcti_native_obligation obligations[] = {
		ORLIX_TCTI_NATIVE_OBLIGATION_DECODE,
		ORLIX_TCTI_NATIVE_OBLIGATION_LEGAL_ENCODINGS,
		ORLIX_TCTI_NATIVE_OBLIGATION_REJECTED_ENCODINGS,
		ORLIX_TCTI_NATIVE_OBLIGATION_GPR,
		ORLIX_TCTI_NATIVE_OBLIGATION_RESULT,
		ORLIX_TCTI_NATIVE_OBLIGATION_MEMORY,
		ORLIX_TCTI_NATIVE_OBLIGATION_FAULT,
		ORLIX_TCTI_NATIVE_OBLIGATION_FP_SIMD,
		ORLIX_TCTI_NATIVE_OBLIGATION_SVE,
	};
	struct orlix_tcti_sve_state *saved_sve;
	unsigned long *saved_simd;
	unsigned long saved_fpcr = current->thread.user_fpcr;
	unsigned long saved_fpsr = current->thread.user_fpsr;
	unsigned long saved_simd_valid = current->thread.user_simd_valid;
	size_t leaf_index;
	size_t obligation_index;

	KUNIT_ASSERT_EQ(test, 0UL, (unsigned long)ELF_HWCAP);
	KUNIT_ASSERT_EQ(test, 0UL, (unsigned long)ELF_HWCAP2);
	saved_sve = kmemdup(&current->thread.user_sve,
			    sizeof(current->thread.user_sve), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, saved_sve);
	saved_simd = kmemdup(current->thread.user_simd,
			     sizeof(current->thread.user_simd), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, saved_simd);
	for (leaf_index = 0; leaf_index < ARRAY_SIZE(current->thread.user_simd);
	     leaf_index++)
		current->thread.user_simd[leaf_index] =
			0xa5a5000000000000ULL ^ leaf_index;
	current->thread.user_simd_valid = 1;
	current->thread.user_fpcr = BIT(22) | BIT(24);
	current->thread.user_fpsr = BIT(27) | BIT(4);
	memset(&current->thread.user_sve, 0, sizeof(current->thread.user_sve));
	current->thread.user_sve.vl_bytes = ORLIX_TCTI_SVE_MIN_VL_BYTES;
	current->thread.user_sve.valid = true;
	current->thread.user_sve.z[31][ORLIX_TCTI_SVE_MIN_VL_BYTES - 1] = 0xa5;
	current->thread.user_sve.p[15][1] = 0x5a;
	current->thread.user_sve.ffr[1] = 0x3c;
	for (leaf_index = 0;
	     leaf_index < orlix_tcti_source_leaf_rejection_count(); leaf_index++) {
		struct orlix_tcti_source_leaf_rejection entry;
		const struct orlix_tcti_source_leaf_rejection *leaf =
			orlix_tcti_source_leaf_rejection_at(leaf_index, &entry);

		KUNIT_ASSERT_NOT_NULL(test, leaf);
		if (!orlix_tcti_source_leaf_is_base_exception(leaf))
			continue;
		for (obligation_index = 0;
		     obligation_index < ARRAY_SIZE(obligations); obligation_index++)
			source_leaf_emit_observation(test, leaf,
				obligations[obligation_index]);
	}
	memcpy(&current->thread.user_sve, saved_sve,
	       sizeof(current->thread.user_sve));
	memcpy(current->thread.user_simd, saved_simd,
	       sizeof(current->thread.user_simd));
	current->thread.user_simd_valid = saved_simd_valid;
	current->thread.user_fpcr = saved_fpcr;
	current->thread.user_fpsr = saved_fpsr;
	kfree(saved_simd);
	kfree(saved_sve);
}

static void orlix_tcti_source_leaf_rejections_match_pinned_tuples(struct kunit *test)
{
	size_t index;

	for (index = 0; index < orlix_tcti_source_leaf_rejection_count(); index++) {
		struct orlix_tcti_source_leaf_rejection entry;
		const struct orlix_tcti_source_leaf_rejection *leaf =
			orlix_tcti_source_leaf_rejection_at(index, &entry);
		u32 variable_mask;
		u32 variable_fields = 0;
		u32 variable_count = 0;
		unsigned int variable_width;

		KUNIT_ASSERT_NOT_NULL(test, leaf);

		KUNIT_EXPECT_EQ_MSG(test, leaf->pattern,
				    leaf->pattern & leaf->mask,
				    "%s source ordinal %u", leaf->name,
				    leaf->ordinal);
		variable_mask = ~leaf->mask;
		variable_width = hweight32(variable_mask);
		KUNIT_ASSERT_LE_MSG(test, variable_width, 16U,
				    "%s source ordinal %u has %u variable bits",
				    leaf->name, leaf->ordinal, variable_width);
		do {
			u32 instruction = leaf->pattern | variable_fields;
			struct orlix_tcti_decoded_instruction decoded =
				orlix_tcti_decode_aarch64(instruction);

			KUNIT_ASSERT_EQ_MSG(test, leaf->pattern,
					    instruction & leaf->mask,
					    "%s source ordinal %u variable fields %#x",
					    leaf->name, leaf->ordinal,
					    variable_fields);
			KUNIT_ASSERT_EQ_MSG(test,
				    orlix_tcti_source_leaf_is_base_exception(leaf) ?
					    ORLIX_TCTI_DECODE_UNDEFINED :
					    ORLIX_TCTI_DECODE_UNSUPPORTED,
					    decoded.decode_class,
					    "%s (%s) source ordinal %u accepted encoding %#x",
					    leaf->name, leaf->operation,
					    leaf->ordinal, instruction);
			variable_count++;
			variable_fields =
				(variable_fields - variable_mask) & variable_mask;
		} while (variable_fields);
		KUNIT_EXPECT_EQ_MSG(test, 1U << variable_width, variable_count,
				    "%s source ordinal %u variable-field coverage",
				    leaf->name, leaf->ordinal);
	}
}

static void orlix_tcti_source_leaf_rejections_are_structured_el0_exits(
	struct kunit *test)
{
	size_t index;

	for (index = 0; index < orlix_tcti_source_leaf_rejection_count(); index++) {
		struct orlix_tcti_source_leaf_rejection entry;
		const struct orlix_tcti_source_leaf_rejection *leaf =
			orlix_tcti_source_leaf_rejection_at(index, &entry);
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		u64 before_simd[ARRAY_SIZE(current->thread.user_simd)];
		unsigned long before_simd_valid;
		u32 observed_instruction = 0;
		unsigned long mapped;
		int ret;

		KUNIT_ASSERT_NOT_NULL(test, leaf);
		mapped = source_leaf_map(test, leaf->pattern);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = 0x123456789abcdef0ULL;
		before = regs;
		memcpy(before_simd, current->thread.user_simd, sizeof(before_simd));
		before_simd_valid = current->thread.user_simd_valid;
		result = orlix_tcti_resume_user(current, &regs, current->mm);

		KUNIT_EXPECT_EQ_MSG(test,
				    orlix_tcti_source_leaf_is_base_exception(leaf) ?
					    ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION :
					    ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason, "%s source ordinal %u",
				    leaf->name, leaf->ordinal);
		KUNIT_EXPECT_EQ_MSG(test,
				    orlix_tcti_source_leaf_is_base_exception(leaf) ?
					    0L : -EOPNOTSUPP,
				    result.status,
				    "%s source ordinal %u", leaf->name,
				    leaf->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, leaf->pattern, result.instruction,
				    "%s source ordinal %u", leaf->name,
				    leaf->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, before.pc, result.pc,
				    "%s source ordinal %u", leaf->name,
				    leaf->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, 0UL, result.fault_address,
				    "%s source ordinal %u", leaf->name,
				    leaf->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_ACCESS_FETCH,
				    result.fault_access, "%s source ordinal %u",
				    leaf->name, leaf->ordinal);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_MEMEQ(test, before_simd, current->thread.user_simd,
				   sizeof(before_simd));
		KUNIT_EXPECT_EQ(test, before_simd_valid,
				current->thread.user_simd_valid);
		ret = orlix_tcti_read_user_data(current->mm, mapped,
						&observed_instruction,
						sizeof(observed_instruction));
		KUNIT_ASSERT_EQ(test, 0, ret);
		KUNIT_EXPECT_EQ_MSG(test, leaf->pattern, observed_instruction,
				    "%s source ordinal %u", leaf->name,
				    leaf->ordinal);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static struct kunit_case orlix_tcti_source_leaf_classification_test_cases[] = {
	KUNIT_CASE(orlix_tcti_system_leaf_catalog_tracks_authoritative_fanout),
	KUNIT_CASE(orlix_tcti_system_accessor_partition_binds_source_metadata),
	KUNIT_CASE(orlix_tcti_system_accessor_partition_matches_decoder_contract),
	KUNIT_CASE(orlix_tcti_system_accessor_partition_rejections_are_structured_el0_exits),
	KUNIT_CASE(orlix_tcti_source_leaf_rejections_match_pinned_tuples),
	KUNIT_CASE(orlix_tcti_source_leaf_rejections_are_structured_el0_exits),
	KUNIT_CASE(orlix_tcti_source_leaf_base_exceptions_emit_typed_observations),
	{}
};

struct kunit_suite orlix_tcti_source_leaf_classification_test_suite = {
	.name = "orlix-tcti-source-leaf-classification",
	.test_cases = orlix_tcti_source_leaf_classification_test_cases,
};
kunit_test_suite(orlix_tcti_source_leaf_classification_test_suite);
