// SPDX-License-Identifier: GPL-2.0-only
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <target_inventory.h>

#include "../decode_aarch64.h"
#include "orlix_tcti_test_suites.h"
#include "orlix_tcti_native_observation.h"
#include "target_native_proof_contract_private.h"
#include "orlix_tcti_source_leaf_rejection_catalog.h"
#include "target_native_proof_registry_private.h"
#include "target_completion_audit.h"

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
	KUNIT_EXPECT_FALSE(test, IS_ERR_VALUE(mapped));
	if (IS_ERR_VALUE(mapped))
		return 0;
	ret = orlix_tcti_write_user_data(current->mm, mapped, program,
				   sizeof(program));
	KUNIT_EXPECT_EQ(test, 0, ret);
	if (ret)
		goto unmap;
	ret = sys_mprotect(mapped, PAGE_SIZE, PROT_READ | PROT_EXEC);
	KUNIT_EXPECT_EQ(test, 0, ret);
	if (ret)
		goto unmap;
	return mapped;

unmap:
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	return 0;
}

#include "orlix_tcti_system_accessor_partition_test.h"

static bool orlix_tcti_source_leaf_is_base_exception(
	const struct orlix_tcti_source_leaf_rejection *leaf)
{
	return leaf->ordinal == 2166U ||
	       (leaf->ordinal >= 2228U && leaf->ordinal <= 2234U);
}

struct source_leaf_official_semantics_not_specified {
	u32 ordinal;
	const char *leaf;
	const char *locator;
	u64 source_offset;
	u64 source_length;
	const char *sha256;
};

#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(...)
#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(...)
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(ordinal, leaf, \
		locator, offset, length, digest) \
	{ ordinal, leaf, locator, offset, length, digest },
static const struct source_leaf_official_semantics_not_specified
	source_leaf_official_semantics_not_specified_rows[] = {
#include "../isa/target_asl_availability.def"
};
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE

struct source_leaf_target_classification {
	const char *leaf;
	const char *classification;
	const char *relation;
	const char *canonical;
	const char *evidence;
	const char *proof;
};

#define ORLIX_TCTI_A64_TARGET_CLASSIFICATION(leaf, classification, relation, \
					      canonical, evidence, proof) \
	{ #leaf, #classification, #relation, canonical, \
	  evidence, proof },
static const struct source_leaf_target_classification
	source_leaf_target_classifications[] = {
#include "../isa/target_classification.def"
};
#undef ORLIX_TCTI_A64_TARGET_CLASSIFICATION

static void orlix_tcti_source_leaf_tenter_has_official_unspecified_semantics_provenance(
	struct kunit *test)
{
	static const char expected_digest[] =
		"28fb16d9885379aa6e05267c659d8b7dab31e819051d85e1dbde8347bc2fdce8";
	const struct source_leaf_official_semantics_not_specified *row = NULL;
	size_t index;

	for (index = 0;
	     index < ARRAY_SIZE(source_leaf_official_semantics_not_specified_rows);
	     index++)
		if (source_leaf_official_semantics_not_specified_rows[index].ordinal ==
		    2235U) {
			row = &source_leaf_official_semantics_not_specified_rows[index];
			break;
		}

	KUNIT_ASSERT_NOT_NULL(test, row);
	KUNIT_EXPECT_STREQ(test, "TENTER_te_exception", row->leaf);
	KUNIT_EXPECT_STREQ(test, "Instructions.json#operations/TENTER/operation",
			   row->locator);
	KUNIT_EXPECT_EQ(test, 115113790ULL, row->source_offset);
	KUNIT_EXPECT_EQ(test, 16ULL, row->source_length);
	KUNIT_EXPECT_STREQ(test, expected_digest, row->sha256);
}

static void orlix_tcti_source_leaf_tenter_remains_unclassified(
	struct kunit *test)
{
	const struct source_leaf_target_classification *row = NULL;
	size_t index;

	for (index = 0; index < ARRAY_SIZE(source_leaf_target_classifications);
	     index++)
		if (!strcmp(source_leaf_target_classifications[index].leaf,
			    "TENTER_te_exception")) {
			row = &source_leaf_target_classifications[index];
			break;
		}

	KUNIT_ASSERT_NOT_NULL(test, row);
	KUNIT_EXPECT_STREQ(test, "UNCLASSIFIED", row->classification);
	KUNIT_EXPECT_STREQ(test, "ORLIX_TCTI_A64_TARGET_RELATION_NONE",
			   row->relation);
	KUNIT_EXPECT_STREQ(test, "", row->canonical);
	KUNIT_EXPECT_STREQ(test, "", row->evidence);
	KUNIT_EXPECT_STREQ(test, "", row->proof);
}

static void orlix_tcti_source_leaf_base_exceptions_have_exact_semantics(
	struct kunit *test)
{
	static const struct {
		u32 ordinal;
		const char *name;
		const char *operation;
		u32 mask;
		u32 pattern;
		enum orlix_tcti_decode_class decode_class;
		enum orlix_tcti_exit_reason exit_reason;
		long status;
	} leaves[] = {
		{ 2166U, "UDF_only_perm_undef", "UDF", 0xffff0000U, 0x00000000U,
		  ORLIX_TCTI_DECODE_UNDEFINED,
		  ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION, 0L },
		{ 2227U, "SVC_EX_exception", "SVC", 0xffe0001fU, 0xd4000001U,
		  ORLIX_TCTI_DECODE_SVC, ORLIX_TCTI_EXIT_SYSCALL, 0L },
		{ 2228U, "HVC_EX_exception", "HVC", 0xffe0001fU, 0xd4000002U,
		  ORLIX_TCTI_DECODE_UNDEFINED,
		  ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION, 0L },
		{ 2229U, "SMC_EX_exception", "SMC", 0xffe0001fU, 0xd4000003U,
		  ORLIX_TCTI_DECODE_UNDEFINED,
		  ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION, 0L },
		{ 2230U, "BRK_EX_exception", "BRK", 0xffe0001fU, 0xd4200000U,
		  ORLIX_TCTI_DECODE_BRK, ORLIX_TCTI_EXIT_BREAKPOINT, 0L },
		{ 2231U, "HLT_EX_exception", "HLT", 0xffe0001fU, 0xd4400000U,
		  ORLIX_TCTI_DECODE_HLT,
		  ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION, 0L },
		{ 2232U, "DCPS1_DC_exception", "DCPS1", 0xffe0001fU, 0xd4a00001U,
		  ORLIX_TCTI_DECODE_UNDEFINED,
		  ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION, 0L },
		{ 2233U, "DCPS2_DC_exception", "DCPS2", 0xffe0001fU, 0xd4a00002U,
		  ORLIX_TCTI_DECODE_UNDEFINED,
		  ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION, 0L },
		{ 2234U, "DCPS3_DC_exception", "DCPS3", 0xffe0001fU, 0xd4a00003U,
		  ORLIX_TCTI_DECODE_UNDEFINED,
		  ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION, 0L },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(leaves); index++) {
		const struct orlix_tcti_source_leaf_manifest_row *source =
			orlix_tcti_source_leaf_manifest_row(leaves[index].ordinal);
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped;

		KUNIT_ASSERT_NOT_NULL(test, source);
		KUNIT_EXPECT_STREQ(test, leaves[index].name, source->name);
		KUNIT_EXPECT_STREQ(test, leaves[index].operation, source->operation);
		KUNIT_EXPECT_EQ(test, leaves[index].mask, source->mask);
		KUNIT_EXPECT_EQ(test, leaves[index].pattern, source->pattern);
		decoded = orlix_tcti_decode_aarch64(source->pattern);
		KUNIT_EXPECT_EQ(test, leaves[index].decode_class,
				decoded.decode_class);
		mapped = source_leaf_map(test, source->pattern);
		if (!mapped)
			return;
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = 0x123456789abcdef0ULL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		KUNIT_EXPECT_EQ(test, leaves[index].exit_reason, result.reason);
		KUNIT_EXPECT_EQ(test, leaves[index].status, result.status);
		KUNIT_EXPECT_EQ(test, source->pattern, result.instruction);
		KUNIT_EXPECT_EQ(test, before.pc, result.pc);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
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
		unsigned long mapped;

		KUNIT_ASSERT_NOT_NULL(test, leaf);
		mapped = source_leaf_map(test, leaf->pattern);
		if (!mapped)
			return;
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = 0x123456789abcdef0ULL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);

		KUNIT_EXPECT_EQ_MSG(test,
					orlix_tcti_source_leaf_is_base_exception(leaf) ?
						ORLIX_TCTI_EXIT_UNDEFINED_INSTRUCTION :
						ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
					result.reason, "%s source ordinal %u",
					leaf->name, leaf->ordinal);
		KUNIT_EXPECT_EQ_MSG(test,
					orlix_tcti_source_leaf_is_base_exception(leaf) ? 0L :
						-EOPNOTSUPP, result.status,
				    "%s source ordinal %u", leaf->name,
				    leaf->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, leaf->pattern, result.instruction,
				    "%s source ordinal %u", leaf->name,
				    leaf->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, before.pc, result.pc,
				    "%s source ordinal %u", leaf->name,
				    leaf->ordinal);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static struct kunit_case orlix_tcti_source_leaf_classification_test_cases[] = {
	KUNIT_CASE(orlix_tcti_system_leaf_catalog_tracks_authoritative_fanout),
	KUNIT_CASE(orlix_tcti_system_accessor_partition_binds_source_metadata),
	KUNIT_CASE(orlix_tcti_system_accessor_partition_matches_decoder_contract),
	KUNIT_CASE(orlix_tcti_system_accessor_partition_rejections_are_structured_el0_exits),
	KUNIT_CASE(orlix_tcti_system_accessor_partition_implemented_production_observations),
	KUNIT_CASE(orlix_tcti_source_leaf_base_exceptions_have_exact_semantics),
	KUNIT_CASE(orlix_tcti_source_leaf_tenter_has_official_unspecified_semantics_provenance),
	KUNIT_CASE(orlix_tcti_source_leaf_tenter_remains_unclassified),
	KUNIT_CASE(orlix_tcti_source_leaf_rejections_match_pinned_tuples),
	KUNIT_CASE(orlix_tcti_source_leaf_rejections_are_structured_el0_exits),
	{}
};

static int orlix_tcti_source_leaf_classification_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_source_leaf_classification_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

struct kunit_suite orlix_tcti_source_leaf_classification_test_suite = {
	.name = "orlix-tcti-source-leaf-classification",
	.init = orlix_tcti_source_leaf_classification_test_init,
	.exit = orlix_tcti_source_leaf_classification_test_exit,
	.test_cases = orlix_tcti_source_leaf_classification_test_cases,
};
kunit_test_suite(orlix_tcti_source_leaf_classification_test_suite);
