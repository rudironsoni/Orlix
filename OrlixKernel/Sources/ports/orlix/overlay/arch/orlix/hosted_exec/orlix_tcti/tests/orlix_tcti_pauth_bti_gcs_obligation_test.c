// SPDX-License-Identifier: GPL-2.0-only
/*
 * This KUnit is intentionally fail-closed.  It proves that every pinned
 * feature-conditioned PAuth, BTI, and GCS leaf remains explicit and cannot
 * fall through to a baseline decoder class. External DDI0602 provenance does
 * not discharge any implementation, rejection, or proof obligation.
 */
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "../isa/pauth_bti_gcs_obligation_ledger.h"
#include "../pointer_authentication.h"
#include "orlix_tcti_test_suites.h"

u64 orlix_tcti_pauth_qemu_oracle_qarma5(u64 data, u64 modifier,
	struct orlix_tcti_pauth_key key);

#define PAUTH_BTI_GCS_RECORD(ordinal, source_id, operation, feature, asl, mask, pattern, behavior) \
	{ ordinal, source_id, operation, feature, asl, mask, pattern, behavior, \
	  ORLIX_TCTI_PAUTH_BTI_GCS_PROVENANCE_EXTERNAL_DDI0602, \
	  ORLIX_TCTI_PAUTH_BTI_GCS_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED, \
	  ORLIX_TCTI_PAUTH_BTI_GCS_PROOF_REQUIRED_UNPROVEN },

static const struct orlix_tcti_pauth_bti_gcs_obligation_record pauth_bti_gcs_rows[] = {
	ORLIX_TCTI_PAUTH_BTI_GCS_OBLIGATION_ROWS(PAUTH_BTI_GCS_RECORD)
};

static void pauth_bti_gcs_inventory_is_complete_and_explicit(struct kunit *test)
{
	size_t index;

	KUNIT_EXPECT_EQ(test, 6U, ARRAY_SIZE(pauth_bti_gcs_rows));
	for (index = 0; index < ARRAY_SIZE(pauth_bti_gcs_rows); index++) {
		const struct orlix_tcti_pauth_bti_gcs_obligation_record *row =
			&pauth_bti_gcs_rows[index];
		size_t prior;

		KUNIT_EXPECT_NE_MSG(test, 0U, row->mask, "%s", row->source_id);
		KUNIT_EXPECT_EQ_MSG(test, row->pattern,
				    row->pattern & row->mask, "%s", row->source_id);
		KUNIT_EXPECT_TRUE_MSG(test, strlen(row->feature_predicate) > 0,
				      "%s", row->source_id);
		KUNIT_EXPECT_TRUE_MSG(test, !strncmp(row->asl_operation,
				    "operations/", 11), "%s", row->source_id);
		KUNIT_EXPECT_EQ_MSG(test,
			ORLIX_TCTI_PAUTH_BTI_GCS_PROVENANCE_EXTERNAL_DDI0602,
			row->semantic_provenance, "%s", row->source_id);
		KUNIT_EXPECT_EQ_MSG(test,
			ORLIX_TCTI_PAUTH_BTI_GCS_IMPLEMENTATION_REQUIRED_UNIMPLEMENTED,
			row->implementation_status, "%s", row->source_id);
		KUNIT_EXPECT_EQ_MSG(test,
			ORLIX_TCTI_PAUTH_BTI_GCS_PROOF_REQUIRED_UNPROVEN,
			row->proof_status, "%s", row->source_id);
		for (prior = 0; prior < index; prior++)
			KUNIT_EXPECT_NE_MSG(test, row->source_ordinal,
					    pauth_bti_gcs_rows[prior].source_ordinal,
					    "duplicate pinned ordinal %s", row->source_id);
	}
}

static void pauth_bti_gcs_unimplemented_leaves_fail_closed(struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pauth_bti_gcs_rows); index++) {
		const struct orlix_tcti_pauth_bti_gcs_obligation_record *row =
			&pauth_bti_gcs_rows[index];
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(row->pattern);

		KUNIT_EXPECT_EQ_MSG(test, row->pattern & row->mask,
				    decoded.instruction & row->mask,
				    "%u %s lost its source encoding", row->source_ordinal,
				    row->source_id);
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				    decoded.decode_class,
				    "%u %s must not use a baseline semantic path",
				    row->source_ordinal, row->source_id);
	}
}

static void pauth_bti_gcs_non_el0_exits_remain_distinct(struct kunit *test)
{
	size_t index;
	unsigned int non_el0 = 0;

	for (index = 0; index < ARRAY_SIZE(pauth_bti_gcs_rows); index++)
		if (pauth_bti_gcs_rows[index].required_behavior ==
		    ORLIX_TCTI_PAUTH_BTI_GCS_NON_EL0_REJECTION)
			non_el0++;

	KUNIT_EXPECT_EQ(test, 2U, non_el0);
}

static unsigned long pauth_bti_gcs_map_instruction(struct kunit *test,
						    u32 instruction)
{
	u32 program[] = { instruction, 0xd4000001U };
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

static void pauth_bti_gcs_non_el0_rejections_preserve_architectural_state(
	struct kunit *test)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pauth_bti_gcs_rows); index++) {
		const struct orlix_tcti_pauth_bti_gcs_obligation_record *row =
			&pauth_bti_gcs_rows[index];
		struct pt_regs regs = { };
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned long mapped;

		if (row->required_behavior !=
		    ORLIX_TCTI_PAUTH_BTI_GCS_NON_EL0_REJECTION)
			continue;

		mapped = pauth_bti_gcs_map_instruction(test, row->pattern);
		regs.pc = mapped;
		regs.sp = STACK_TOP - 16;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = 0x123456789abcdef0ULL;
		regs.regs[30] = 0x0fedcba987654321ULL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);

		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
				    result.reason, "%u %s", row->source_ordinal,
				    row->source_id);
		KUNIT_EXPECT_EQ_MSG(test, -EOPNOTSUPP, result.status,
				    "%u %s", row->source_ordinal, row->source_id);
		KUNIT_EXPECT_EQ_MSG(test, row->pattern, result.instruction,
				    "%u %s", row->source_ordinal, row->source_id);
		KUNIT_EXPECT_EQ(test, before.pc, result.pc);
		KUNIT_EXPECT_MEMEQ(test, before.regs, regs.regs,
				   sizeof(regs.regs));
		KUNIT_EXPECT_EQ(test, before.pc, regs.pc);
		KUNIT_EXPECT_EQ(test, before.sp, regs.sp);
		KUNIT_EXPECT_EQ(test, before.pstate, regs.pstate);
		KUNIT_EXPECT_EQ(test, before.syscallno, regs.syscallno);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
	}
}

static struct kunit_case pauth_bti_gcs_cases[] = {
	KUNIT_CASE(pauth_bti_gcs_inventory_is_complete_and_explicit),
	KUNIT_CASE(pauth_bti_gcs_unimplemented_leaves_fail_closed),
	KUNIT_CASE(pauth_bti_gcs_non_el0_exits_remain_distinct),
	KUNIT_CASE(pauth_bti_gcs_non_el0_rejections_preserve_architectural_state),
	{}
};

struct kunit_suite orlix_tcti_pauth_bti_gcs_obligation_test_suite = {
	.name = "orlix-tcti-pauth-bti-gcs-obligations",
	.test_cases = pauth_bti_gcs_cases,
};

kunit_test_suite(orlix_tcti_pauth_bti_gcs_obligation_test_suite);

/*
 * The issue #137 cohort remains a separate suite and proof namespace while
 * sharing this already product-compiled KUnit translation unit.
 */

#define PAUTH_SVC 0xd4000001U
#define PAUTH_NZCV (PSR_N_BIT | PSR_Z_BIT | PSR_C_BIT | PSR_V_BIT)

#define PAUTH_RECORD(ordinal, source_id, operation, feature, asl, mask, pattern, behavior) \
	{ ordinal, source_id, operation, feature, asl, mask, pattern, behavior, \
	  ORLIX_TCTI_PAUTH_BTI_GCS_PROVENANCE_EXTERNAL_DDI0602, \
	  ORLIX_TCTI_PAUTH_BTI_GCS_IMPLEMENTED_PRODUCTION, \
	  ORLIX_TCTI_PAUTH_BTI_GCS_PROOF_SOURCE_BOUND_KUNIT },

static const struct orlix_tcti_pauth_bti_gcs_obligation_record pauth_rows[] = {
	ORLIX_TCTI_POINTER_AUTHENTICATION_OBLIGATION_ROWS(PAUTH_RECORD)
};

struct pauth_test_context {
	struct mm_struct *mm;
	struct orlix_tcti_pauth_state saved_state;
};

static int pauth_test_init(struct kunit *test)
{
	struct pauth_test_context *context;

	context = kunit_kzalloc(test, sizeof(*context), GFP_KERNEL);
	if (!context)
		return -ENOMEM;
	context->mm = mm_alloc();
	if (!context->mm)
		return -ENOMEM;
	context->saved_state = current->thread.user_pauth;
	kthread_use_mm(context->mm);
	test->priv = context;
	return 0;
}

static void pauth_test_exit(struct kunit *test)
{
	struct pauth_test_context *context = test->priv;

	if (!context)
		return;
	current->thread.user_pauth = context->saved_state;
	kthread_unuse_mm(context->mm);
	mmput(context->mm);
}

static void pauth_set_test_keys(void)
{
	current->thread.user_pauth.apia = (struct orlix_tcti_pauth_key) {
		.high = 0x84be85ce9804e94bULL,
		.low = 0xec2802d4e0a488e9ULL,
	};
	current->thread.user_pauth.apib = (struct orlix_tcti_pauth_key) {
		.high = 0x0123456789abcdefULL,
		.low = 0xfedcba9876543210ULL,
	};
	current->thread.user_pauth.apda = (struct orlix_tcti_pauth_key) {
		.high = 0x1122334455667788ULL,
		.low = 0x99aabbccddeeff00ULL,
	};
	current->thread.user_pauth.apdb = (struct orlix_tcti_pauth_key) {
		.high = 0x8877665544332211ULL,
		.low = 0x00ffeeddccbbaa99ULL,
	};
	current->thread.user_pauth.apga = (struct orlix_tcti_pauth_key) {
		.high = 0x0f1e2d3c4b5a6978ULL,
		.low = 0x8796a5b4c3d2e1f0ULL,
	};
	current->thread.user_pauth.pacm = false;
}

/* Test-local PAUTH field rules. QARMA5 itself comes from the pinned QEMU oracle. */
#define PAUTH_TEST_LOW_ADDRESS_MASK GENMASK_ULL(47, 0)
#define PAUTH_TEST_HIGH_PAC_MASK GENMASK_ULL(63, 56)
#define PAUTH_TEST_LOW_PAC_MASK GENMASK_ULL(54, 48)

static u64 pauth_oracle_canonicalize(u64 pointer, unsigned int select_bit)
{
	u64 result = pointer & PAUTH_TEST_LOW_ADDRESS_MASK;

	if (pointer & BIT_ULL(select_bit))
		result |= ~PAUTH_TEST_LOW_ADDRESS_MASK;
	return result;
}

static u64 pauth_oracle_qarma5_two_modifiers(
	u64 data, u64 modifier1, u64 modifier2, struct orlix_tcti_pauth_key key)
{
	u64 combined = ((modifier2 >> 5) & 0xffffffffULL) << 32;

	combined |= (modifier1 >> 4) & 0xffffffffULL;
	return orlix_tcti_pauth_qemu_oracle_qarma5(data, combined, key);
}

static u64 pauth_oracle_add(u64 pointer, u64 modifier, u64 modifier2,
			    bool use_modifier2, struct orlix_tcti_pauth_key key)
{
	bool upper = pointer & BIT_ULL(63);
	u64 extended = pauth_oracle_canonicalize(pointer, 63);
	u64 pac = use_modifier2 ?
		pauth_oracle_qarma5_two_modifiers(extended, modifier, modifier2, key) :
		orlix_tcti_pauth_qemu_oracle_qarma5(extended, modifier, key);
	u64 extension = pointer >> ORLIX_TCTI_PAUTH_BOTTOM_BIT;

	if (extension != 0 && extension != GENMASK_ULL(15, 0))
		pac ^= BIT_ULL(62);
	return (pac & PAUTH_TEST_HIGH_PAC_MASK) |
		(upper ? BIT_ULL(55) : 0) |
		(pac & PAUTH_TEST_LOW_PAC_MASK) |
		(pointer & PAUTH_TEST_LOW_ADDRESS_MASK);
}

static u64 pauth_oracle_authenticate(u64 pointer, u64 modifier, u64 modifier2,
				     bool use_modifier2, bool key_b,
				     struct orlix_tcti_pauth_key key)
{
	u64 original = pauth_oracle_canonicalize(pointer, 55);
	u64 pac = use_modifier2 ?
		pauth_oracle_qarma5_two_modifiers(original, modifier, modifier2, key) :
		orlix_tcti_pauth_qemu_oracle_qarma5(original, modifier, key);

	if ((pac & PAUTH_TEST_HIGH_PAC_MASK) ==
		(pointer & PAUTH_TEST_HIGH_PAC_MASK) &&
	    (pac & PAUTH_TEST_LOW_PAC_MASK) ==
		(pointer & PAUTH_TEST_LOW_PAC_MASK))
		return original;
	original &= ~GENMASK_ULL(62, 61);
	return original | ((key_b ? 2ULL : 1ULL) << 61);
}

static u64 pauth_oracle_strip(u64 pointer)
{
	return pauth_oracle_canonicalize(pointer, 55);
}

static u32 pauth_spread_free_bits(u32 value, u32 free_mask)
{
	u32 instruction = 0;
	unsigned int source_bit = 0;
	unsigned int bit;

	for (bit = 0; bit < 32; bit++) {
		if (!(free_mask & BIT(bit)))
			continue;
		if (value & BIT(source_bit))
			instruction |= BIT(bit);
		source_bit++;
	}
	return instruction;
}

static bool pauth_encoding_is_alias(const struct orlix_tcti_pauth_bti_gcs_obligation_record *row,
				    u32 instruction)
{
	return (!strcmp(row->operation, "RETASPPCR_reg") &&
		(instruction & 0x1fU) == 31U);
}

static enum orlix_tcti_pauth_op pauth_expected_operation(const char *operation)
{
	if (!strcmp(operation, "PACM"))
		return ORLIX_TCTI_PAUTH_SET_PACM;
	if (!strcmp(operation, "PACGA"))
		return ORLIX_TCTI_PAUTH_GENERIC;
	if (!strcmp(operation, "XPAC"))
		return ORLIX_TCTI_PAUTH_STRIP;
	if (!strcmp(operation, "BRA") || !strcmp(operation, "BLRA") ||
	    !strcmp(operation, "RETA") ||
	    !strcmp(operation, "RETASPPCR_reg") ||
	    !strcmp(operation, "RETASPPC_imm"))
		return ORLIX_TCTI_PAUTH_BRANCH;
	if (!strcmp(operation, "LDRA"))
		return ORLIX_TCTI_PAUTH_LOAD;
	if (!strncmp(operation, "AUT", 3))
		return ORLIX_TCTI_PAUTH_AUTHENTICATE;
	return ORLIX_TCTI_PAUTH_ADD;
}

static enum orlix_tcti_pauth_key_select
pauth_expected_key(const struct orlix_tcti_pauth_bti_gcs_obligation_record *row)
{
	if (!strcmp(row->operation, "PACGA"))
		return ORLIX_TCTI_PAUTH_KEY_APGA;
	if (strstr(row->operation, "PACDA") || strstr(row->operation, "AUTDA") ||
	    !strncmp(row->source_id, "LDRAA", 5))
		return ORLIX_TCTI_PAUTH_KEY_APDA;
	if (strstr(row->operation, "PACDB") || strstr(row->operation, "AUTDB") ||
	    !strncmp(row->source_id, "LDRAB", 5))
		return ORLIX_TCTI_PAUTH_KEY_APDB;
	if (strstr(row->operation, "PACIB") || strstr(row->operation, "AUTIB") ||
	    strstr(row->operation, "NBIB") || strstr(row->source_id, "BRAB") ||
	    strstr(row->source_id, "BLRAB") || strstr(row->source_id, "RETAB"))
		return ORLIX_TCTI_PAUTH_KEY_APIB;
	return ORLIX_TCTI_PAUTH_KEY_APIA;
}

static void pauth_exact_63_leaf_free_field_matrix(struct kunit *test)
{
	size_t row_index;

	KUNIT_ASSERT_EQ(test, 63U, ARRAY_SIZE(pauth_rows));
	for (row_index = 0; row_index < ARRAY_SIZE(pauth_rows); row_index++) {
		const struct orlix_tcti_pauth_bti_gcs_obligation_record *row =
			&pauth_rows[row_index];
		u32 free_mask = ~row->mask;
		unsigned int free_count = hweight32(free_mask);
		u32 variants = BIT(free_count);
		u32 variant;
		size_t prior;
		enum orlix_tcti_pauth_op expected_operation =
			pauth_expected_operation(row->operation);
		enum orlix_tcti_pauth_key_select expected_key =
			pauth_expected_key(row);

		KUNIT_ASSERT_LE_MSG(test, free_count, 20U, "%s", row->source_id);
		KUNIT_EXPECT_EQ_MSG(test, row->pattern, row->pattern & row->mask,
				    "%u %s", row->source_ordinal, row->source_id);
		KUNIT_EXPECT_EQ_MSG(test,
			ORLIX_TCTI_PAUTH_BTI_GCS_IMPLEMENTED_PRODUCTION,
			row->implementation_status, "%s", row->source_id);
		KUNIT_EXPECT_EQ_MSG(test,
			ORLIX_TCTI_PAUTH_BTI_GCS_PROOF_SOURCE_BOUND_KUNIT,
			row->proof_status, "%s", row->source_id);
		for (prior = 0; prior < row_index; prior++)
			KUNIT_EXPECT_NE_MSG(test, row->source_ordinal,
				pauth_rows[prior].source_ordinal, "duplicate %s",
				row->source_id);

		for (variant = 0; variant < variants; variant++) {
			u32 instruction = row->pattern |
				pauth_spread_free_bits(variant, free_mask);
			struct orlix_tcti_decoded_instruction decoded;

			if (pauth_encoding_is_alias(row, instruction))
				continue;
			decoded = orlix_tcti_decode_aarch64(instruction);
			KUNIT_ASSERT_EQ_MSG(test,
				ORLIX_TCTI_DECODE_POINTER_AUTHENTICATION,
				decoded.decode_class, "%u %s %#x", row->source_ordinal,
				row->source_id, instruction);
			if (!variant) {
				KUNIT_EXPECT_EQ_MSG(test, expected_operation,
					decoded.pauth_op, "%s", row->source_id);
				KUNIT_EXPECT_EQ_MSG(test, expected_key, decoded.pauth_key,
					"%s", row->source_id);
			}
		}
	}
}

static bool pauth_source_allocates(u32 instruction)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pauth_rows); index++)
		if ((instruction & pauth_rows[index].mask) == pauth_rows[index].pattern)
			return true;
	return false;
}

static void pauth_expect_source_allocation(struct kunit *test, u32 instruction)
{
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(instruction);
	bool allocated = pauth_source_allocates(instruction);

	if (allocated)
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_POINTER_AUTHENTICATION,
			decoded.decode_class, "%#x", instruction);
	else
		KUNIT_EXPECT_NE_MSG(test, ORLIX_TCTI_DECODE_POINTER_AUTHENTICATION,
			decoded.decode_class, "%#x", instruction);
}

static void pauth_reserved_invalid_matrix(struct kunit *test)
{
	static const u32 branch_prefixes[] = {
		0xd61f0000U, 0xd63f0000U, 0xd65f0000U,
		0xd71f0000U, 0xd73f0000U,
	};
	u32 selector;
	u32 rn;
	u32 rd;
	u32 index;
	size_t prefix;

	/* Exhaust every HINT immediate, including the neighboring BTI domain. */
	for (selector = 0; selector < 128; selector++)
		pauth_expect_source_allocation(test,
			0xd503201fU | (selector << 5));

	/* Exhaust every DP1 selector and both free register fields. */
	for (selector = 0; selector < 64; selector++)
		for (rn = 0; rn < 32; rn++)
			for (rd = 0; rd < 32; rd++)
				pauth_expect_source_allocation(test, 0xdac10000U |
					(selector << 10) | (rn << 5) | rd);

	/* Exhaust the PACGA DP2 operation selector; register fields are legal-free. */
	for (selector = 0; selector < 64; selector++)
		pauth_expect_source_allocation(test,
			0x9ac00000U | (selector << 10));

	/* Exhaust every low-half branch encoding in the five owning major forms. */
	for (prefix = 0; prefix < ARRAY_SIZE(branch_prefixes); prefix++)
		for (selector = 0; selector <= 0xffffU; selector++)
			pauth_expect_source_allocation(test,
				branch_prefixes[prefix] | selector);

	/* Exhaust immediate-form top selectors with representative immediates. */
	for (selector = 0; selector < 2048; selector++)
		pauth_expect_source_allocation(test, (selector << 21) | 0x1fU);

	/* Exhaust LDRA key/sign/index selectors; imm/base/destination are legal-free. */
	for (selector = 0; selector < 16; selector++)
		pauth_expect_source_allocation(test, 0xf8000000U |
			((selector & 12U) << 20) | ((selector & 3U) << 10));

	/* Rm=31 aliases the allocated RETAA/RETAB leaves. */
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_POINTER_AUTHENTICATION,
		orlix_tcti_decode_aarch64(0xd65f0bffU).decode_class);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_POINTER_AUTHENTICATION,
		orlix_tcti_decode_aarch64(0xd65f0fffU).decode_class);
	/* The neighboring BTI family remains outside issue #137. */
	for (index = 0; index < 4; index++)
		KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
			orlix_tcti_decode_aarch64(0xd503241fU | (index << 6)).decode_class);
}

static void pauth_qarma5_architectural_vector(struct kunit *test)
{
	const struct orlix_tcti_pauth_key key = {
		.high = 0x84be85ce9804e94bULL,
		.low = 0xec2802d4e0a488e9ULL,
	};

	/* QARMA-64 r=5 reference vector used by Arm's ComputePAC model. */
	KUNIT_EXPECT_EQ(test, 0x3ee99a6c82af0c38ULL,
		orlix_tcti_pauth_qemu_oracle_qarma5(0xfb623599da6e8127ULL,
			0x477d469dec0b8762ULL, key));
}

static void pauth_insert_authenticate_strip_and_failure(struct kunit *test)
{
	static const struct {
		u64 pointer;
		u64 modifier;
		u64 modifier2;
		bool use_modifier2;
		bool key_b;
		struct orlix_tcti_pauth_key key;
	} vectors[] = {
		{ 0x0000123456789abcULL, 0x477d469dec0b8762ULL, 0, false, false,
		  { 0x84be85ce9804e94bULL, 0xec2802d4e0a488e9ULL } },
		{ 0xffff923456789abcULL, 0x1020304050607080ULL,
		  0x8877665544332211ULL, true, true,
		  { 0x0123456789abcdefULL, 0xfedcba9876543210ULL } },
		{ 0x000056789abcdef0ULL, 0xfedcba9876543210ULL,
		  0x0123456789abcdefULL, true, false,
		  { 0x1122334455667788ULL, 0x99aabbccddeeff00ULL } },
	};
	size_t index;

	for (index = 0; index < ARRAY_SIZE(vectors); index++) {
		const typeof(vectors[0]) *vector = &vectors[index];
		u64 signed_pointer = pauth_oracle_add(vector->pointer,
			vector->modifier, vector->modifier2, vector->use_modifier2,
			vector->key);
		u64 corrupted = signed_pointer ^ BIT_ULL(56);

		KUNIT_EXPECT_NE(test, vector->pointer, signed_pointer);
		KUNIT_EXPECT_EQ(test, signed_pointer,
			orlix_tcti_pauth_add(vector->pointer, vector->modifier,
				vector->modifier2, vector->use_modifier2, &vector->key));
		KUNIT_EXPECT_EQ(test, pauth_oracle_authenticate(signed_pointer,
			vector->modifier, vector->modifier2, vector->use_modifier2,
			vector->key_b, vector->key),
			orlix_tcti_pauth_authenticate(signed_pointer, vector->modifier,
				vector->modifier2, vector->use_modifier2, vector->key_b,
				&vector->key));
		KUNIT_EXPECT_EQ(test, pauth_oracle_authenticate(corrupted,
			vector->modifier, vector->modifier2, vector->use_modifier2,
			vector->key_b, vector->key),
			orlix_tcti_pauth_authenticate(corrupted, vector->modifier,
				vector->modifier2, vector->use_modifier2, vector->key_b,
				&vector->key));
		KUNIT_EXPECT_EQ(test, pauth_oracle_strip(signed_pointer),
			orlix_tcti_pauth_strip(signed_pointer));
	}
}

static unsigned long pauth_map_page(struct kunit *test, int protection)
{
	unsigned long address = ksys_mmap_pgoff(0, PAGE_SIZE, protection,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(address));
	return address;
}

static void pauth_write_program(struct kunit *test, unsigned long address,
				u32 first, u32 second)
{
	u32 program[] = { first, second };
	int ret = sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_WRITE);

	KUNIT_ASSERT_EQ(test, 0, ret);
	ret = orlix_tcti_write_user_data(current->mm, address, program,
					 sizeof(program));
	KUNIT_ASSERT_EQ(test, 0, ret);
	KUNIT_ASSERT_EQ(test, 0,
		sys_mprotect(address, PAGE_SIZE, PROT_READ | PROT_EXEC));
}

static struct pt_regs pauth_regs(unsigned long pc)
{
	struct pt_regs regs = { };

	regs.pc = pc;
	regs.sp = 0x0000123456700000ULL;
	regs.pstate = PSR_MODE_EL0t | PAUTH_NZCV;
	regs.syscallno = NO_SYSCALL;
	return regs;
}

static void pauth_expect_svc(struct kunit *test,
			     const struct orlix_tcti_result *result,
			     unsigned long svc_address)
{
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result->reason);
	KUNIT_EXPECT_EQ(test, 0L, result->status);
	KUNIT_EXPECT_EQ(test, svc_address, result->pc);
	KUNIT_EXPECT_EQ(test, PAUTH_SVC, result->instruction);
}

static void pauth_production_data_key_modifier_and_flags(struct kunit *test)
{
	unsigned long program = pauth_map_page(test, PROT_READ | PROT_WRITE);
	struct orlix_tcti_result result;
	struct pt_regs regs;
	const u64 pointer = 0x0000123456789abcULL;
	const u64 modifier = 0x477d469dec0b8762ULL;
	u64 signed_pointer;

	pauth_set_test_keys();
	/* PACIA x0, x1 */
	pauth_write_program(test, program, 0xdac10020U, PAUTH_SVC);
	regs = pauth_regs(program);
	regs.regs[0] = pointer;
	regs.regs[1] = modifier;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	pauth_expect_svc(test, &result, program + sizeof(u32));
	signed_pointer = regs.regs[0];
	KUNIT_EXPECT_NE(test, pointer, signed_pointer);
	KUNIT_EXPECT_EQ(test, PSR_MODE_EL0t | PAUTH_NZCV, regs.pstate);

	/* AUTIA x0, x1 */
	pauth_write_program(test, program, 0xdac11020U, PAUTH_SVC);
	regs = pauth_regs(program);
	regs.regs[0] = signed_pointer;
	regs.regs[1] = modifier;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	pauth_expect_svc(test, &result, program + sizeof(u32));
	KUNIT_EXPECT_EQ(test, pointer, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, PSR_MODE_EL0t | PAUTH_NZCV, regs.pstate);

	/* A wrong modifier uses architectural key-A poison bits. */
	regs = pauth_regs(program);
	regs.regs[0] = signed_pointer;
	regs.regs[1] = modifier ^ 1;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	pauth_expect_svc(test, &result, program + sizeof(u32));
	KUNIT_EXPECT_EQ(test, 1ULL, (regs.regs[0] >> 61) & 3ULL);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(program, PAGE_SIZE));
}

static void pauth_production_generic_branch_link_return_pacm(struct kunit *test)
{
	unsigned long program = pauth_map_page(test, PROT_READ | PROT_WRITE);
	unsigned long target = pauth_map_page(test, PROT_READ | PROT_WRITE);
	struct orlix_tcti_result result;
	struct pt_regs regs;
	u64 signed_target;
	u64 expected_generic;

	pauth_set_test_keys();
	pauth_write_program(test, target, PAUTH_SVC, PAUTH_SVC);

	/* PACGA x0, x1, x2 */
	pauth_write_program(test, program, 0x9ac23020U, PAUTH_SVC);
	regs = pauth_regs(program);
	regs.regs[1] = 0x0123456789abcdefULL;
	regs.regs[2] = 0xfedcba9876543210ULL;
	expected_generic = orlix_tcti_pauth_qemu_oracle_qarma5(regs.regs[1],
		regs.regs[2], current->thread.user_pauth.apga) & GENMASK_ULL(63, 32);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	pauth_expect_svc(test, &result, program + sizeof(u32));
	KUNIT_EXPECT_EQ(test, expected_generic, regs.regs[0]);

	/* PACGA's Rm == 31 reads XZR, never SP. */
	pauth_write_program(test, program, 0x9adf3020U, PAUTH_SVC);
	regs = pauth_regs(program);
	regs.regs[1] = 0x0123456789abcdefULL;
	regs.sp = 0xfedcba9876543210ULL;
	expected_generic = orlix_tcti_pauth_qemu_oracle_qarma5(regs.regs[1], 0,
		current->thread.user_pauth.apga) & GENMASK_ULL(63, 32);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	pauth_expect_svc(test, &result, program + sizeof(u32));
	KUNIT_EXPECT_EQ(test, expected_generic, regs.regs[0]);

	/* BLRAA x1, x2 authenticates the target and writes the exact link PC. */
	signed_target = pauth_oracle_add(target, 0xabcULL, 0, false,
		current->thread.user_pauth.apia);
	pauth_write_program(test, program, 0xd73f0822U, PAUTH_SVC);
	regs = pauth_regs(program);
	regs.regs[1] = signed_target;
	regs.regs[2] = 0xabcULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	pauth_expect_svc(test, &result, target);
	KUNIT_EXPECT_EQ(test, program + sizeof(u32), regs.regs[30]);
	KUNIT_EXPECT_EQ(test, PSR_MODE_EL0t | PAUTH_NZCV, regs.pstate);
	regs = pauth_regs(program);
	regs.regs[1] = signed_target;
	regs.regs[2] = 0xabdULL;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_LT(test, result.status, 0L);
	KUNIT_EXPECT_EQ(test, PSR_MODE_EL0t | PAUTH_NZCV, regs.pstate);

	/* PACM makes RETAA consume SP and X16 as the two modifiers. */
	pauth_write_program(test, program, 0xd50324ffU, 0xd65f0bffU);
	regs = pauth_regs(program);
	regs.regs[16] = 0x1020304050607080ULL;
	regs.regs[30] = pauth_oracle_add(target, regs.sp, regs.regs[16], true,
		current->thread.user_pauth.apia);
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	pauth_expect_svc(test, &result, target);
	KUNIT_EXPECT_TRUE(test, current->thread.user_pauth.pacm);
	KUNIT_EXPECT_EQ(test, PSR_MODE_EL0t | PAUTH_NZCV, regs.pstate);

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(target, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(program, PAGE_SIZE));
}

static void pauth_production_ldra_and_auth_failure_fault(struct kunit *test)
{
	unsigned long program = pauth_map_page(test, PROT_READ | PROT_WRITE);
	unsigned long data = pauth_map_page(test, PROT_READ | PROT_WRITE);
	const u64 value = 0x0123456789abcdefULL;
	struct orlix_tcti_result result;
	struct pt_regs regs;
	u64 signed_base;
	int ret;

	pauth_set_test_keys();
	ret = orlix_tcti_write_user_data(current->mm, data, &value, sizeof(value));
	KUNIT_ASSERT_EQ(test, 0, ret);

	/* LDRAA x0, [x1] */
	pauth_write_program(test, program, 0xf8200420U, PAUTH_SVC);
	signed_base = pauth_oracle_add(data, 0, 0, false,
		current->thread.user_pauth.apda);
	regs = pauth_regs(program);
	regs.regs[1] = signed_base;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	pauth_expect_svc(test, &result, program + sizeof(u32));
	KUNIT_EXPECT_EQ(test, value, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, signed_base, regs.regs[1]);

	/* Pre-indexed LDRAA authenticates first, adds +8, then writes back. */
	pauth_write_program(test, program, 0xf8201c20U, PAUTH_SVC);
	signed_base = pauth_oracle_add(data - 8, 0, 0, false,
		current->thread.user_pauth.apda);
	regs = pauth_regs(program);
	regs.regs[1] = signed_base;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	pauth_expect_svc(test, &result, program + sizeof(u32));
	KUNIT_EXPECT_EQ(test, value, regs.regs[0]);
	KUNIT_EXPECT_EQ(test, data, regs.regs[1]);

	/* Wrong-key authentication produces a noncanonical read fault. */
	pauth_write_program(test, program, 0xf8a00420U, PAUTH_SVC);
	regs = pauth_regs(program);
	regs.regs[1] = signed_base;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_USER_FAULT, result.reason);
	KUNIT_EXPECT_LT(test, result.status, 0L);
	KUNIT_EXPECT_EQ(test, program, regs.pc);
	KUNIT_EXPECT_EQ(test, PSR_MODE_EL0t | PAUTH_NZCV, regs.pstate);

	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(program, PAGE_SIZE));
}

static void pauth_non_el0_rejections_preserve_architectural_state(struct kunit *test)
{
	unsigned long program = pauth_map_page(test, PROT_READ | PROT_WRITE);
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pauth_rows); index++) {
		const struct orlix_tcti_pauth_bti_gcs_obligation_record *row =
			&pauth_rows[index];
		struct pt_regs regs = pauth_regs(program);
		struct pt_regs before;
		struct orlix_tcti_result result;
		unsigned int reg;

		pauth_write_program(test, program, row->pattern, PAUTH_SVC);
		for (reg = 0; reg < ARRAY_SIZE(regs.regs); reg++)
			regs.regs[reg] = 0x1020304050607080ULL + reg;
		regs.pstate = PSR_MODE_EL1h | PAUTH_NZCV;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);

		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
			result.reason, "%u %s", row->source_ordinal, row->source_id);
		KUNIT_EXPECT_EQ_MSG(test, -EOPNOTSUPP, result.status, "%u %s",
			row->source_ordinal, row->source_id);
		KUNIT_EXPECT_MEMEQ(test, &before, &regs, sizeof(regs));
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(program, PAGE_SIZE));
}

static struct kunit_case pauth_cases[] = {
	KUNIT_CASE(pauth_exact_63_leaf_free_field_matrix),
	KUNIT_CASE(pauth_reserved_invalid_matrix),
	KUNIT_CASE(pauth_qarma5_architectural_vector),
	KUNIT_CASE(pauth_insert_authenticate_strip_and_failure),
	KUNIT_CASE(pauth_production_data_key_modifier_and_flags),
	KUNIT_CASE(pauth_production_generic_branch_link_return_pacm),
	KUNIT_CASE(pauth_production_ldra_and_auth_failure_fault),
	KUNIT_CASE(pauth_non_el0_rejections_preserve_architectural_state),
	{}
};

struct kunit_suite orlix_tcti_pointer_authentication_source_bound_test_suite = {
	.name = "orlix-tcti-pointer-authentication-source-bound",
	.init = pauth_test_init,
	.exit = pauth_test_exit,
	.test_cases = pauth_cases,
};

kunit_test_suite(orlix_tcti_pointer_authentication_source_bound_test_suite);


MODULE_LICENSE("GPL");
