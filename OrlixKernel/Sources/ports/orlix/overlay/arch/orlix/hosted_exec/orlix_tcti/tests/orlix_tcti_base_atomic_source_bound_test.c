// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/syscalls.h>
#include <asm/orlix_tcti.h>
#include <asm/processor.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"

enum orlix_tcti_test_source_family {
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY(symbol, ...) \
	ORLIX_TCTI_TEST_SOURCE_FAMILY_##symbol,
#include "../isa/target_execution_slice_map.def"
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY
};

static const u8 orlix_tcti_test_source_families[] = {
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY(...)
#define ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER(i, name, condition, family) \
	[i] = ORLIX_TCTI_TEST_SOURCE_FAMILY_##family,
#include "../isa/target_execution_slice_map.def"
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_SOURCE
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_COUNTS
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_FAMILY
#undef ORLIX_TCTI_A64_EXECUTION_SLICE_MAP_MEMBER
};

struct orlix_tcti_test_atomic_source {
	u32 ordinal;
	const char *name;
	const char *operation;
	u32 mask;
	u32 pattern;
	const char *condition_tcnd_hex;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(i, name, mnemonic, operation, mask, \
	pattern, condition, ...) { i, name, operation, mask, pattern, condition },
static const struct orlix_tcti_test_atomic_source orlix_tcti_test_sources[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW

static const bool orlix_tcti_test_has_ddi0602_semantics[] = {
#define ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE(...)
#define ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW(i, ...) [i] = true,
#define ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW(i, ...) [i] = false,
#include "../isa/target_asl_availability.def"
#undef ORLIX_TCTI_A64_SEMANTIC_PROVENANCE_SOURCE
#undef ORLIX_TCTI_A64_DDI0602_PROVENANCE_ROW
#undef ORLIX_TCTI_A64_OFFICIAL_SEMANTICS_NOT_SPECIFIED_ROW
};

static bool orlix_tcti_test_is_base_atomic(
	const struct orlix_tcti_test_atomic_source *source)
{
	return source->ordinal < ARRAY_SIZE(orlix_tcti_test_source_families) &&
		orlix_tcti_test_source_families[source->ordinal] ==
			ORLIX_TCTI_TEST_SOURCE_FAMILY_BASE_ATOMICS;
}

static const struct orlix_tcti_test_atomic_source *
orlix_tcti_test_base_atomic_operation(const char *operation)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++)
		if (orlix_tcti_test_is_base_atomic(&orlix_tcti_test_sources[index]) &&
		    !strcmp(orlix_tcti_test_sources[index].operation, operation))
			return &orlix_tcti_test_sources[index];
	return NULL;
}

static const struct orlix_tcti_test_atomic_source *
orlix_tcti_test_source_ordinal(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++)
		if (orlix_tcti_test_sources[index].ordinal == ordinal)
			return &orlix_tcti_test_sources[index];
	return NULL;
}

static u32 orlix_tcti_test_set_variable_field(u32 instruction, u32 mask,
					      u8 shift, u8 bits, u8 value)
{
	u32 field = ((1U << bits) - 1U) << shift;
	u32 variable = field & ~mask;

	return (instruction & ~variable) | ((u32)value << shift & variable);
}

static u32 orlix_tcti_test_legal_instruction(
	const struct orlix_tcti_test_atomic_source *source)
{
	u32 instruction = source->pattern;

	instruction = orlix_tcti_test_set_variable_field(instruction, source->mask,
							 0, 5, 8);
	instruction = orlix_tcti_test_set_variable_field(instruction, source->mask,
							 5, 5, 10);
	instruction = orlix_tcti_test_set_variable_field(instruction, source->mask,
							 10, 5, 4);
	instruction = orlix_tcti_test_set_variable_field(instruction, source->mask,
							 16, 5, 2);
	return instruction;
}

static void orlix_tcti_base_atomic_decodes_exact_source_cohort(struct kunit *test)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_atomic_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		u32 instruction;

		if (!orlix_tcti_test_is_base_atomic(source))
			continue;
		instruction = orlix_tcti_test_legal_instruction(source);
		KUNIT_ASSERT_EQ_MSG(test, source->pattern,
				instruction & source->mask, "%s", source->name);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_EXPECT_NE_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				decoded.decode_class, "%s %#x", source->name,
				instruction);
		KUNIT_EXPECT_EQ_MSG(test, source->ordinal, decoded.source_ordinal,
				"%s %#x", source->name, instruction);
		KUNIT_ASSERT_NOT_NULL_MSG(test, source->condition_tcnd_hex, "%s",
			source->name);
		KUNIT_EXPECT_STREQ_MSG(test, source->condition_tcnd_hex,
			decoded.source_condition_tcnd_hex, "%s", source->name);
		count++;
	}
	KUNIT_EXPECT_EQ(test, 498U, count);
}

static void orlix_tcti_base_atomic_binds_pinned_ddi0602_semantics(struct kunit *test)
{
	size_t index;
	size_t count = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_atomic_source *source =
			&orlix_tcti_test_sources[index];

		if (!orlix_tcti_test_is_base_atomic(source))
			continue;
		KUNIT_ASSERT_LT_MSG(test, source->ordinal,
			ARRAY_SIZE(orlix_tcti_test_has_ddi0602_semantics), "%s",
			source->name);
		KUNIT_EXPECT_TRUE_MSG(test,
			orlix_tcti_test_has_ddi0602_semantics[source->ordinal], "%s",
			source->name);
		count++;
	}
	KUNIT_EXPECT_EQ(test, 498U, count);
}

static void orlix_tcti_base_atomic_decodes_register_variants(struct kunit *test)
{
	size_t index;
	size_t variants = 0;
	size_t reserved = 0;
	size_t candidates = 0;
	size_t fields[4] = {};
	u8 value;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_atomic_source *source =
			&orlix_tcti_test_sources[index];
		u32 legal;
		u8 shift;

		if (!orlix_tcti_test_is_base_atomic(source))
			continue;
		legal = orlix_tcti_test_legal_instruction(source);
		for (shift = 0; shift <= 16; shift += shift == 0 ? 5 :
						       shift == 5 ? 5 : 6) {
			u32 field = 0x1fU << shift;
			u8 field_index = shift == 0 ? 0 : shift == 5 ? 1 :
				shift == 10 ? 2 : 3;

			if (!(field & ~source->mask))
				continue;
			fields[field_index]++;
			for (value = 0; value < 32; value++) {
				struct orlix_tcti_decoded_instruction decoded;
				u32 instruction;

				candidates++;
				instruction = orlix_tcti_test_set_variable_field(legal,
					source->mask, shift, 5, value);
				decoded = orlix_tcti_decode_aarch64(instruction);
				if (decoded.decode_class == ORLIX_TCTI_DECODE_UNSUPPORTED) {
					reserved++;
					continue;
				}
				KUNIT_EXPECT_EQ_MSG(test, source->ordinal,
					decoded.source_ordinal, "%s field=%u value=%u",
					source->name, shift, value);
				variants++;
			}
		}
	}
	KUNIT_EXPECT_EQ(test, 458U, fields[0]);
	KUNIT_EXPECT_EQ(test, 498U, fields[1]);
	KUNIT_EXPECT_EQ(test, 31U, fields[2]);
	KUNIT_EXPECT_EQ(test, 456U, fields[3]);
	KUNIT_EXPECT_EQ(test, 46176U, candidates);
	KUNIT_EXPECT_EQ(test, candidates, variants + reserved);
}

static void orlix_tcti_base_atomic_rejects_reserved_registers(struct kunit *test)
{
	size_t index;
	size_t ls64 = 0;
	size_t pair = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_atomic_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;

		if (!orlix_tcti_test_is_base_atomic(source))
			continue;
		instruction = orlix_tcti_test_legal_instruction(source);
		if (!strcmp(source->operation, "LD64B") ||
		    !strcmp(source->operation, "ST64B") ||
		    !strcmp(source->operation, "ST64BV") ||
		    !strcmp(source->operation, "ST64BV0")) {
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(
					(instruction & ~0x1fU) | 9U).decode_class);
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(
					(instruction & ~0x1fU) | 24U).decode_class);
			ls64++;
		}
		if (strstr(source->operation, "CASP") &&
		    (0x1fU & ~source->mask)) {
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
				orlix_tcti_decode_aarch64(
					(instruction & ~0x1fU) | 9U).decode_class);
			pair++;
		}
	}
	KUNIT_EXPECT_EQ(test, 4U, ls64);
	KUNIT_EXPECT_GT(test, pair, 0U);
}

static void orlix_tcti_base_atomic_classifies_fixed_bit_neighbours(struct kunit *test)
{
	size_t index;
	size_t neighbours = 0;
	size_t legal = 0;
	size_t rejected = 0;
	u8 bit;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_atomic_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;

		if (!orlix_tcti_test_is_base_atomic(source))
			continue;
		instruction = orlix_tcti_test_legal_instruction(source);
		for (bit = 0; bit < 32; bit++) {
			struct orlix_tcti_decoded_instruction decoded;
			const struct orlix_tcti_test_atomic_source *neighbour;
			u32 flipped;

			if (!(source->mask & BIT(bit)))
				continue;
			flipped = instruction ^ BIT(bit);
			decoded = orlix_tcti_decode_aarch64(flipped);
			KUNIT_EXPECT_NE_MSG(test, source->ordinal,
				decoded.source_ordinal, "%s fixed-bit=%u",
				source->name, bit);
			if (decoded.decode_class == ORLIX_TCTI_DECODE_UNSUPPORTED) {
				rejected++;
			} else {
				neighbour = orlix_tcti_test_source_ordinal(
					decoded.source_ordinal);
				KUNIT_ASSERT_NOT_NULL_MSG(test, neighbour,
					"%s fixed-bit=%u ordinal=%u", source->name,
					bit, decoded.source_ordinal);
				KUNIT_EXPECT_EQ_MSG(test, neighbour->pattern,
					flipped & neighbour->mask,
					"%s fixed-bit=%u decoded=%s", source->name,
					bit, neighbour->name);
				legal++;
			}
			neighbours++;
		}
	}
	KUNIT_EXPECT_GT(test, neighbours, 498U * 10U);
	KUNIT_EXPECT_EQ(test, neighbours, legal + rejected);
	KUNIT_EXPECT_GT(test, legal, 0U);
	KUNIT_EXPECT_GT(test, rejected, 0U);
}

static void orlix_tcti_base_atomic_decodes_ordered_access_shapes(struct kunit *test)
{
	size_t index;
	size_t signed_loads = 0;
	size_t simd_q = 0;
	size_t writeback = 0;
	size_t pair_postindex = 0;
	size_t ldap_pair = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_atomic_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;

		if (!orlix_tcti_test_is_base_atomic(source))
			continue;
		decoded = orlix_tcti_decode_aarch64(
			orlix_tcti_test_legal_instruction(source));
		if (strstr(source->operation, "LDAPURSB")) {
			KUNIT_EXPECT_EQ(test, 1, decoded.access_size);
			KUNIT_EXPECT_TRUE(test, decoded.sign_extend_load);
			signed_loads++;
		} else if (strstr(source->operation, "LDAPURSH")) {
			KUNIT_EXPECT_EQ(test, 2, decoded.access_size);
			KUNIT_EXPECT_TRUE(test, decoded.sign_extend_load);
			signed_loads++;
		} else if (strstr(source->operation, "LDAPURSW")) {
			KUNIT_EXPECT_EQ(test, 4, decoded.access_size);
			KUNIT_EXPECT_TRUE(test, decoded.sign_extend_load);
			signed_loads++;
		}
		if (strstr(source->name, "_Q_ldapstl_simd")) {
			KUNIT_EXPECT_EQ(test, 16, decoded.access_size);
			KUNIT_EXPECT_TRUE(test, decoded.simd_fp);
			simd_q++;
		}
		if (strstr(source->name, "_ldapstl_writeback")) {
			KUNIT_EXPECT_EQ(test, decoded.load ?
				ORLIX_TCTI_MEMORY_INDEX_POST :
				ORLIX_TCTI_MEMORY_INDEX_PRE,
				decoded.memory_index_mode);
			KUNIT_EXPECT_EQ(test, decoded.load ? decoded.access_size :
				-(s64)decoded.access_size, decoded.memory_offset);
			writeback++;
		}
		if ((!strcmp(source->operation, "LDIAPP") ||
		     !strcmp(source->operation, "STILP")) &&
		    !(source->pattern & BIT(12))) {
			KUNIT_EXPECT_EQ(test, ORLIX_TCTI_MEMORY_INDEX_POST,
				decoded.memory_index_mode);
			KUNIT_EXPECT_EQ(test, 2 * decoded.access_size,
				decoded.memory_offset);
			pair_postindex++;
		}
		if (!strcmp(source->operation, "LDAP_gen")) {
			KUNIT_EXPECT_TRUE(test, decoded.pair);
			ldap_pair++;
		}
	}
	KUNIT_EXPECT_EQ(test, 5U, signed_loads);
	KUNIT_EXPECT_EQ(test, 2U, simd_q);
	KUNIT_EXPECT_EQ(test, 4U, writeback);
	KUNIT_EXPECT_EQ(test, 4U, pair_postindex);
	KUNIT_EXPECT_EQ(test, 1U, ldap_pair);
}

static void orlix_tcti_base_atomic_executes_every_source_leaf(struct kunit *test)
{
	unsigned long mapped;
	unsigned long address;
	size_t index;
	size_t executed = 0;
	size_t rcw_executed = 0;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	address = ALIGN(mapped + 128, 64);
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_atomic_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		unsigned long fault_address = 0;
		u32 instruction;
		int ret;

		if (!orlix_tcti_test_is_base_atomic(source))
			continue;
		instruction = orlix_tcti_test_legal_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		if (decoded.decode_class == ORLIX_TCTI_DECODE_UNSUPPORTED)
			continue;
		if (decoded.atomic_rcw) {
			struct orlix_tcti_rcw_el1_state state = {
				.feat_the = true,
				.feat_d128 = decoded.pair,
			};
			u64 old[2] = {};
			u64 expected[2] = {};
			u64 operand[2] = { 1, 1 };
			u64 result[2] = {};
			u8 nzcv = 0;
			bool wrote_new = false;

			KUNIT_ASSERT_EQ_MSG(test, 0,
				orlix_tcti_rcw_evaluate(&decoded, &state, old, expected,
					operand, result, &nzcv, &wrote_new), "%s",
				source->name);
			KUNIT_EXPECT_EQ_MSG(test, 0x2, nzcv, "%s", source->name);
			KUNIT_EXPECT_TRUE_MSG(test, wrote_new, "%s", source->name);
			executed++;
			rcw_executed++;
			continue;
		}
		regs.regs[decoded.rn] = address;
		regs.regs[decoded.rs] = 2;
		regs.regs[decoded.rt] = 8;
		if (decoded.rt2 < 31)
			regs.regs[decoded.rt2] = 4;
		regs.pc = 0x1000;
		ret = orlix_tcti_switch_debug_execute_decoded(current->mm, &regs,
							 &decoded, &fault_address);
		KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s class=%u fault=%#lx",
				source->name, decoded.decode_class, fault_address);
		if (!ret) {
			executed++;
			KUNIT_EXPECT_EQ_MSG(test, 0x1004ULL, regs.pc, "%s",
				source->name);
		}
	}
	KUNIT_EXPECT_EQ(test, 498U, executed);
	KUNIT_EXPECT_EQ(test, 64U, rcw_executed);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static u64 orlix_tcti_test_fp_value(u8 size, bool bfloat, u8 value)
{
	if (bfloat)
		return value == 1 ? 0x3f80U : value == 2 ? 0x4000U : 0x4040U;
	if (size == sizeof(u16))
		return value == 1 ? 0x3c00U : value == 2 ? 0x4000U : 0x4200U;
	if (size == sizeof(u32))
		return value == 1 ? 0x3f800000U :
			value == 2 ? 0x40000000U : 0x40400000U;
	return value == 1 ? 0x3ff0000000000000ULL :
		value == 2 ? 0x4000000000000000ULL : 0x4008000000000000ULL;
}

static void orlix_tcti_base_atomic_executes_every_fp_leaf(struct kunit *test)
{
	u64 saved_simd[ARRAY_SIZE(current->thread.user_simd)];
	unsigned long saved_valid = current->thread.user_simd_valid;
	unsigned long saved_fpcr = current->thread.user_fpcr;
	unsigned long saved_fpsr = current->thread.user_fpsr;
	unsigned long mapped;
	size_t index;
	size_t executed = 0;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	memcpy(saved_simd, current->thread.user_simd, sizeof(saved_simd));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_atomic_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		unsigned long fault_address = 0;
		bool bfloat;
		u64 old;
		u64 operand;
		u64 expected;
		u64 observed = 0;
		int ret;

		if (!orlix_tcti_test_is_base_atomic(source) ||
		    (!strstr(source->operation, "LDF") &&
		     !strstr(source->operation, "STF") &&
		     !strstr(source->operation, "LDBF") &&
		     !strstr(source->operation, "STBF")))
			continue;
		decoded = orlix_tcti_decode_aarch64(
			orlix_tcti_test_legal_instruction(source));
		KUNIT_ASSERT_TRUE(test, decoded.atomic_fp);
		bfloat = strstr(source->operation, "BF") != NULL;
		old = orlix_tcti_test_fp_value(decoded.access_size, bfloat, 1);
		operand = orlix_tcti_test_fp_value(decoded.access_size, bfloat, 2);
		expected = strstr(source->operation, "ADD") ?
			orlix_tcti_test_fp_value(decoded.access_size, bfloat, 3) :
			strstr(source->operation, "MAX") ? operand : old;
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm,
			mapped, &old, decoded.access_size));
		current->thread.user_simd[decoded.rs * 2U] = operand;
		current->thread.user_simd_valid = 1;
		current->thread.user_fpcr = 0;
		current->thread.user_fpsr = BIT(27) | BIT(4);
		regs.regs[decoded.rn] = mapped;
		regs.pc = 0x2000;
		ret = orlix_tcti_switch_debug_execute_decoded(current->mm, &regs,
			&decoded, &fault_address);
		KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s", source->name);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
			mapped, &observed, decoded.access_size));
		KUNIT_EXPECT_EQ_MSG(test, expected, observed, "%s", source->name);
		if (!decoded.atomic_store_only)
			KUNIT_EXPECT_EQ_MSG(test, old,
				current->thread.user_simd[decoded.rt * 2U], "%s",
				source->name);
		KUNIT_EXPECT_EQ_MSG(test, BIT(27) | BIT(4),
			current->thread.user_fpsr, "%s", source->name);
		KUNIT_EXPECT_EQ_MSG(test, 0x2004ULL, regs.pc, "%s", source->name);
		executed++;
	}
	KUNIT_EXPECT_EQ(test, 120U, executed);
	memcpy(current->thread.user_simd, saved_simd, sizeof(saved_simd));
	current->thread.user_simd_valid = saved_valid;
	current->thread.user_fpcr = saved_fpcr;
	current->thread.user_fpsr = saved_fpsr;
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_base_atomic_rcw_conditional_writes_and_flags(
	struct kunit *test)
{
	static const struct orlix_tcti_rcw_el1_state protected_scalar = {
		.feat_the = true,
		.tcr2_el1_enabled = true,
		.tcr2_el1_pnch = true,
	};
	static const struct orlix_tcti_rcw_el1_state unprotected_scalar = {
		.feat_the = true,
	};
	static const struct orlix_tcti_rcw_el1_state d128 = {
		.rcwmask_el1 = { 0, BIT_ULL(63) },
		.feat_the = true,
		.feat_d128 = true,
	};
	const struct orlix_tcti_test_atomic_source *source;
	struct orlix_tcti_decoded_instruction decoded;
	u64 old[2] = {};
	u64 expected[2] = {};
	u64 operand[2] = {};
	u64 result[2] = {};
	u8 nzcv;
	bool wrote_new;

	/* Protected-state mask failure: no new value, Z=1 and C=1. */
	source = orlix_tcti_test_base_atomic_operation("RCWSET");
	KUNIT_ASSERT_NOT_NULL(test, source);
	decoded = orlix_tcti_decode_aarch64(orlix_tcti_test_legal_instruction(source));
	old[0] = BIT_ULL(52) | BIT_ULL(0);
	operand[0] = BIT_ULL(2);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_rcw_evaluate(&decoded,
		&protected_scalar, old,
		expected, operand, result, &nzcv, &wrote_new));
	KUNIT_EXPECT_EQ(test, old[0], result[0]);
	KUNIT_EXPECT_EQ(test, 0x6, nzcv);
	KUNIT_EXPECT_FALSE(test, wrote_new);

	/* RCWS state failure with protection disabled clears C and preserves memory. */
	source = orlix_tcti_test_base_atomic_operation("RCWSSET");
	KUNIT_ASSERT_NOT_NULL(test, source);
	decoded = orlix_tcti_decode_aarch64(orlix_tcti_test_legal_instruction(source));
	memset(old, 0, sizeof(old));
	memset(result, 0, sizeof(result));
	operand[0] = BIT_ULL(0);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_rcw_evaluate(&decoded,
		&unprotected_scalar, old,
		expected, operand, result, &nzcv, &wrote_new));
	KUNIT_EXPECT_EQ(test, 0ULL, result[0]);
	KUNIT_EXPECT_EQ(test, 0, nzcv);
	KUNIT_EXPECT_FALSE(test, wrote_new);

	/* CAS compare failure is N=1,C=1 and returns the old value. */
	source = orlix_tcti_test_base_atomic_operation("RCWCAS");
	KUNIT_ASSERT_NOT_NULL(test, source);
	decoded = orlix_tcti_decode_aarch64(orlix_tcti_test_legal_instruction(source));
	old[0] = 0x44;
	expected[0] = 0x33;
	operand[0] = 0x55;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_rcw_evaluate(&decoded,
		&unprotected_scalar, old,
		expected, operand, result, &nzcv, &wrote_new));
	KUNIT_EXPECT_EQ(test, old[0], result[0]);
	KUNIT_EXPECT_EQ(test, 0xa, nzcv);
	KUNIT_EXPECT_FALSE(test, wrote_new);

	/* The same owning core proves the D128 pair domain and its high mask lane. */
	source = orlix_tcti_test_base_atomic_operation("RCWSETP");
	KUNIT_ASSERT_NOT_NULL(test, source);
	decoded = orlix_tcti_decode_aarch64(orlix_tcti_test_legal_instruction(source));
	old[0] = BIT_ULL(0);
	old[1] = BIT_ULL(50);
	operand[0] = 0;
	operand[1] = BIT_ULL(63);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_rcw_evaluate(&decoded, &d128, old,
		expected, operand, result, &nzcv, &wrote_new));
	KUNIT_EXPECT_EQ(test, old[0], result[0]);
	KUNIT_EXPECT_EQ(test, old[1] | BIT_ULL(63), result[1]);
	KUNIT_EXPECT_EQ(test, 0x2, nzcv);
	KUNIT_EXPECT_TRUE(test, wrote_new);
}

static void orlix_tcti_base_atomic_executes_ls64_state(struct kunit *test)
{
	unsigned long saved_accdata = current->thread.user_accdata_el1;
	unsigned long mapped;
	unsigned long address;
	size_t index;
	size_t executed = 0;

	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	address = ALIGN(mapped + 128, 64);
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_atomic_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		unsigned long fault_address = 0;
		u64 initial[8];
		u64 observed[8] = {};
		u8 lane;

		if (!orlix_tcti_test_is_base_atomic(source) ||
		    (strcmp(source->operation, "LD64B") &&
		     strncmp(source->operation, "ST64B", 5)))
			continue;
		decoded = orlix_tcti_decode_aarch64(
			orlix_tcti_test_legal_instruction(source));
		for (lane = 0; lane < ARRAY_SIZE(initial); lane++) {
			initial[lane] = 0x1100000000000000ULL + lane;
			regs.regs[decoded.rt + lane] = 0x2200000000000000ULL + lane;
		}
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm,
			address, initial, sizeof(initial)));
		current->thread.user_accdata_el1 = 0xa5a55a5aU;
		regs.regs[decoded.rn] = address;
		regs.pc = 0x3000;
		KUNIT_EXPECT_EQ_MSG(test, 0,
			orlix_tcti_switch_debug_execute_decoded(current->mm, &regs,
				&decoded, &fault_address), "%s", source->name);
		if (decoded.load) {
			for (lane = 0; lane < ARRAY_SIZE(initial); lane++)
				KUNIT_EXPECT_EQ_MSG(test, initial[lane],
					regs.regs[decoded.rt + lane], "%s lane=%u",
					source->name, lane);
		} else {
			KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
				address, observed, sizeof(observed)));
			KUNIT_EXPECT_EQ_MSG(test,
				decoded.ls64_accdata ? 0x22000000a5a55a5aULL :
					0x2200000000000000ULL,
				observed[0], "%s", source->name);
			for (lane = 1; lane < ARRAY_SIZE(observed); lane++)
				KUNIT_EXPECT_EQ_MSG(test,
					0x2200000000000000ULL + lane, observed[lane],
					"%s lane=%u", source->name, lane);
			if (decoded.ls64_status)
				KUNIT_EXPECT_EQ_MSG(test, 0ULL, regs.regs[decoded.rs],
					"%s", source->name);
		}
		KUNIT_EXPECT_EQ_MSG(test, 0x3004ULL, regs.pc, "%s", source->name);
		executed++;
	}
	KUNIT_EXPECT_EQ(test, 4U, executed);
	current->thread.user_accdata_el1 = saved_accdata;
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_base_atomic_faults_are_precise(struct kunit *test)
{
	const struct orlix_tcti_decoded_instruction clrex =
		orlix_tcti_decode_aarch64(0xd503305fU);
	struct orlix_tcti_rcw_el1_state rcw_state;
	unsigned long read_only;
	u8 initial[64];
	size_t index;
	size_t unmapped_checked = 0;
	size_t alignment_checked = 0;
	size_t read_only_checked = 0;

	memset(initial, 0xa5, sizeof(initial));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_rcw_el1_state_read(&rcw_state));
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_EXCLUSIVE_MONITOR_CLEAR,
		clrex.decode_class);
	read_only = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(read_only));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm,
		read_only, initial, sizeof(initial)));
	KUNIT_ASSERT_EQ(test, 0, sys_mprotect(read_only, PAGE_SIZE, PROT_READ));

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_atomic_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		struct pt_regs before;
		unsigned long fault_address = 0;
		u8 observed[sizeof(initial)] = {};
		u8 total_size;
		bool rcw_available;
		bool exclusive_store;
		bool read_only_access;
		int ret;

		if (!orlix_tcti_test_is_base_atomic(source))
			continue;
		decoded = orlix_tcti_decode_aarch64(
			orlix_tcti_test_legal_instruction(source));
		total_size = decoded.access_size * (decoded.pair ? 2U : 1U);
		rcw_available = !decoded.atomic_rcw ||
			(rcw_state.feat_the && decoded.pair == rcw_state.feat_d128);
		exclusive_store = decoded.exclusive && !decoded.load;
		read_only_access = decoded.load &&
			(decoded.decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE ||
			 decoded.decode_class == ORLIX_TCTI_DECODE_LS64);

		/* Unmapped access: feature-disabled RCW is Undefined before memory. */
		regs.regs[decoded.rn] = TASK_SIZE;
		regs.regs[decoded.rs] = 0x1122334455667788ULL;
		regs.regs[decoded.rt] = 0x8877665544332211ULL;
		regs.sp = TASK_SIZE;
		regs.pc = 0x4000;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
		before = regs;
		ret = orlix_tcti_switch_debug_execute_decoded(current->mm, &regs,
			&decoded, &fault_address);
		if (!rcw_available) {
			KUNIT_EXPECT_TRUE_MSG(test, ret == -EOPNOTSUPP || ret == -ENOEXEC,
				"%s ret=%d", source->name, ret);
			KUNIT_EXPECT_EQ_MSG(test, 0, memcmp(&before, &regs, sizeof(regs)),
				"%s", source->name);
		} else if (exclusive_store) {
			KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s", source->name);
			KUNIT_EXPECT_EQ_MSG(test, 1ULL, regs.regs[decoded.rs], "%s",
				source->name);
			KUNIT_EXPECT_EQ_MSG(test, before.pc + sizeof(u32), regs.pc, "%s",
				source->name);
		} else {
			KUNIT_EXPECT_LT_MSG(test, ret, 0, "%s", source->name);
			KUNIT_EXPECT_EQ_MSG(test, 0, memcmp(&before, &regs, sizeof(regs)),
				"%s", source->name);
		}
		KUNIT_EXPECT_EQ_MSG(test, TASK_SIZE, fault_address, "%s", source->name);
		unmapped_checked++;

		/* Natural-alignment failure is precise and commits no architectural state. */
		if (total_size > 1U) {
			memset(&regs, 0, sizeof(regs));
			regs.regs[decoded.rn] = read_only + 1U;
			regs.sp = read_only + 1U;
			regs.pc = 0x5000;
			regs.pstate = PSR_MODE_EL0t | PSR_Z_BIT | PSR_V_BIT;
			before = regs;
			fault_address = 0;
			ret = orlix_tcti_switch_debug_execute_decoded(current->mm, &regs,
				&decoded, &fault_address);
			KUNIT_EXPECT_EQ_MSG(test, -EFAULT, ret, "%s", source->name);
			KUNIT_EXPECT_EQ_MSG(test, 0, memcmp(&before, &regs, sizeof(regs)),
				"%s", source->name);
			KUNIT_EXPECT_EQ_MSG(test, read_only + 1U, fault_address, "%s",
				source->name);
			KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
				read_only, observed, sizeof(observed)));
			KUNIT_EXPECT_EQ_MSG(test, 0, memcmp(initial, observed,
				sizeof(initial)), "%s", source->name);
			alignment_checked++;
		}

		/* Read-only mappings admit loads and reject writes without partial state. */
		memset(&regs, 0, sizeof(regs));
		regs.regs[decoded.rn] = read_only;
		regs.regs[decoded.rs] = 0x0102030405060708ULL;
		regs.regs[decoded.rt] = 0x1112131415161718ULL;
		regs.sp = read_only;
		regs.pc = 0x6000;
		regs.pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_V_BIT;
		before = regs;
		fault_address = 0;
		ret = orlix_tcti_switch_debug_execute_decoded(current->mm, &regs,
			&decoded, &fault_address);
		if (!rcw_available) {
			KUNIT_EXPECT_TRUE_MSG(test, ret == -EOPNOTSUPP || ret == -ENOEXEC,
				"%s ret=%d", source->name, ret);
			KUNIT_EXPECT_EQ_MSG(test, 0, memcmp(&before, &regs, sizeof(regs)),
				"%s", source->name);
		} else if (read_only_access) {
			KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s", source->name);
			KUNIT_EXPECT_EQ_MSG(test, before.pc + sizeof(u32), regs.pc, "%s",
				source->name);
		} else if (exclusive_store) {
			KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s", source->name);
			KUNIT_EXPECT_EQ_MSG(test, 1ULL, regs.regs[decoded.rs], "%s",
				source->name);
		} else {
			KUNIT_EXPECT_LT_MSG(test, ret, 0, "%s", source->name);
			KUNIT_EXPECT_EQ_MSG(test, 0, memcmp(&before, &regs, sizeof(regs)),
				"%s", source->name);
		}
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm,
			read_only, observed, sizeof(observed)));
		KUNIT_EXPECT_EQ_MSG(test, 0, memcmp(initial, observed, sizeof(initial)),
			"%s", source->name);
		read_only_checked++;

		/* CLREX production semantics isolate every exclusive-monitor row. */
		memset(&regs, 0, sizeof(regs));
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_switch_debug_execute_decoded(
			current->mm, &regs, &clrex, NULL));
	}
	KUNIT_EXPECT_EQ(test, 498U, unmapped_checked);
	KUNIT_EXPECT_GT(test, alignment_checked, 0U);
	KUNIT_EXPECT_EQ(test, 498U, read_only_checked);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(read_only, PAGE_SIZE));
}

static void orlix_tcti_base_atomic_atomicity_is_page_serialized(struct kunit *test)
{
	size_t index;
	size_t atomic = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_atomic_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;

		if (!orlix_tcti_test_is_base_atomic(source))
			continue;
		decoded = orlix_tcti_decode_aarch64(
			orlix_tcti_test_legal_instruction(source));
		if (decoded.decode_class == ORLIX_TCTI_DECODE_LSE_ATOMIC ||
		    decoded.exclusive)
			atomic++;
	}
	KUNIT_EXPECT_GT(test, atomic, 0U);
}

static void orlix_tcti_base_atomic_order_classes_are_exact(struct kunit *test)
{
	size_t index;
	size_t rcsc = 0;
	size_t rcpc = 0;
	size_t limited = 0;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_atomic_source *source =
			&orlix_tcti_test_sources[index];
		struct orlix_tcti_decoded_instruction decoded;

		if (!orlix_tcti_test_is_base_atomic(source))
			continue;
		decoded = orlix_tcti_decode_aarch64(
			orlix_tcti_test_legal_instruction(source));
		KUNIT_EXPECT_FALSE_MSG(test,
			decoded.rcpc_acquire && decoded.limited_ordering, "%s",
			source->name);
		if (decoded.rcpc_acquire)
			rcpc++;
		else if (decoded.limited_ordering)
			limited++;
		else if (decoded.acquire || decoded.release)
			rcsc++;
	}
	KUNIT_EXPECT_GT(test, rcsc, 0U);
	KUNIT_EXPECT_GT(test, rcpc, 0U);
	KUNIT_EXPECT_GT(test, limited, 0U);
}

static struct kunit_case orlix_tcti_base_atomic_source_bound_cases[] = {
	KUNIT_CASE(orlix_tcti_base_atomic_decodes_exact_source_cohort),
	KUNIT_CASE(orlix_tcti_base_atomic_binds_pinned_ddi0602_semantics),
	KUNIT_CASE(orlix_tcti_base_atomic_decodes_register_variants),
	KUNIT_CASE(orlix_tcti_base_atomic_rejects_reserved_registers),
	KUNIT_CASE(orlix_tcti_base_atomic_classifies_fixed_bit_neighbours),
	KUNIT_CASE(orlix_tcti_base_atomic_decodes_ordered_access_shapes),
	KUNIT_CASE(orlix_tcti_base_atomic_executes_every_source_leaf),
	KUNIT_CASE(orlix_tcti_base_atomic_executes_every_fp_leaf),
	KUNIT_CASE(orlix_tcti_base_atomic_rcw_conditional_writes_and_flags),
	KUNIT_CASE(orlix_tcti_base_atomic_executes_ls64_state),
	KUNIT_CASE(orlix_tcti_base_atomic_faults_are_precise),
	KUNIT_CASE(orlix_tcti_base_atomic_atomicity_is_page_serialized),
	KUNIT_CASE(orlix_tcti_base_atomic_order_classes_are_exact),
	{}
};

static struct kunit_suite orlix_tcti_base_atomic_source_bound_suite = {
	.name = "orlix-tcti-base-atomic-source-bound",
	.test_cases = orlix_tcti_base_atomic_source_bound_cases,
};

kunit_test_suite(orlix_tcti_base_atomic_source_bound_suite);
