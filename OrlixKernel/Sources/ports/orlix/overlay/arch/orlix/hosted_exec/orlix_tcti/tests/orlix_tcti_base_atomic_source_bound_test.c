// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/err.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/bits.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <asm/orlix_tcti.h>
#include <asm/processor.h>
#include <asm/ptrace.h>

#include "../decode_aarch64.h"
#include "../switch_debug.h"
#include "orlix_tcti_memory_proof.h"
#include "orlix_tcti_native_observation.h"
#include "orlix_tcti_base_atomic_production_capture.h"
#include "target_native_proof_contract_private.h"
#include "target_proof_ingestion_private.h"

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
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
	const char *condition_tcnd_hex;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(i, name, mnemonic, operation, mask, \
	pattern, condition, ...) { i, name, mnemonic, operation, mask, pattern, condition },
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
orlix_tcti_test_base_atomic_name(const char *name)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++)
		if (orlix_tcti_test_is_base_atomic(&orlix_tcti_test_sources[index]) &&
		    !strcmp(orlix_tcti_test_sources[index].name, name))
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
							 5, 5, 30);
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
			neighbour = orlix_tcti_test_source_ordinal(
				decoded.source_ordinal);
			if (neighbour &&
			    decoded.decode_class != ORLIX_TCTI_DECODE_UNSUPPORTED &&
			    decoded.decode_class != ORLIX_TCTI_DECODE_UNDEFINED &&
			    (flipped & neighbour->mask) == neighbour->pattern) {
				legal++;
			} else {
				rejected++;
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
			u64 old[2] = { 1, 1 };
			u64 expected[2] = { 1, 1 };
			u64 operand[2] = { 1, 1 };
			u64 result[2] = {};
			u8 nzcv = 0;
			bool wrote_new = false;

			if (decoded.lse_atomic_op == ORLIX_TCTI_LSE_ATOMIC_SET ||
			    decoded.lse_atomic_op == ORLIX_TCTI_LSE_ATOMIC_CLR) {
				operand[0] = 0;
				operand[1] = 0;
			}

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

static unsigned long orlix_tcti_base_atomic_base_for_ea(
	const struct orlix_tcti_decoded_instruction *decoded, unsigned long ea)
{
	if (decoded->memory_index_mode == ORLIX_TCTI_MEMORY_INDEX_PRE &&
	    decoded->memory_offset < 0)
		return ea + (unsigned long)(-(s64)decoded->memory_offset);
	return ea;
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
		regs.regs[decoded.rn] =
			orlix_tcti_base_atomic_base_for_ea(&decoded, TASK_SIZE);
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
			regs.regs[decoded.rn] = orlix_tcti_base_atomic_base_for_ea(
				&decoded, read_only + 1U);
			regs.sp = regs.regs[decoded.rn];
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
		regs.regs[decoded.rn] =
			orlix_tcti_base_atomic_base_for_ea(&decoded, read_only);
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

struct orlix_tcti_base_atomic_pair {
	u64 low;
	u64 high;
};

struct orlix_tcti_base_atomic_concurrent {
	struct mm_struct *mm;
	unsigned long address;
	const struct orlix_tcti_decoded_instruction *decoded;
	const struct orlix_tcti_base_atomic_pair *first;
	const struct orlix_tcti_base_atomic_pair *second;
	struct completion start;
	struct completion reader_ready;
	struct completion writer_done;
	struct completion reader_done;
	atomic_t writer_running;
	int writer_ret;
	int reader_ret;
	bool torn;
};

/*
 * Hosted kthreads free the kthread struct on threadfn return. Park until
 * kthread_stop so the joiner does not dereference a NULL kthread.
 */
static void orlix_tcti_base_atomic_park_until_stop(void)
{
	while (!kthread_should_stop()) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (!kthread_should_stop())
			schedule();
	}
	__set_current_state(TASK_RUNNING);
}

static int orlix_tcti_base_atomic_pair_writer(void *data)
{
	struct orlix_tcti_base_atomic_concurrent *state = data;
	struct orlix_tcti_base_atomic_pair expected = *state->first;
	unsigned int iteration;
	struct pt_regs regs = {};

	kthread_use_mm(state->mm);
	wait_for_completion(&state->start);
	for (iteration = 0; iteration < 512U && !kthread_should_stop(); iteration++) {
		const struct orlix_tcti_base_atomic_pair *desired =
			!memcmp(&expected, state->first, sizeof(expected)) ?
				state->second : state->first;

		regs.regs[state->decoded->rn] = state->address;
		regs.regs[state->decoded->rs] = expected.low;
		regs.regs[state->decoded->rs + 1U] = expected.high;
		regs.regs[state->decoded->rt] = desired->low;
		regs.regs[state->decoded->rt + 1U] = desired->high;
		state->writer_ret = orlix_tcti_switch_debug_execute_decoded(
			state->mm, &regs, state->decoded, NULL);
		if (state->writer_ret)
			break;
		if (regs.regs[state->decoded->rs] != expected.low ||
		    regs.regs[state->decoded->rs + 1U] != expected.high) {
			state->writer_ret = -EAGAIN;
			break;
		}
		expected = *desired;
		cond_resched();
	}
	atomic_set(&state->writer_running, 0);
	kthread_unuse_mm(state->mm);
	complete(&state->writer_done);
	orlix_tcti_base_atomic_park_until_stop();
	return 0;
}

static int orlix_tcti_base_atomic_pair_reader(void *data)
{
	struct orlix_tcti_base_atomic_concurrent *state = data;
	struct orlix_tcti_base_atomic_pair observed;

	kthread_use_mm(state->mm);
	complete(&state->reader_ready);
	wait_for_completion(&state->start);
	while (atomic_read(&state->writer_running) && !kthread_should_stop()) {
		state->reader_ret = orlix_tcti_read_user_data(state->mm,
			state->address, &observed, sizeof(observed));
		if (state->reader_ret ||
		    (memcmp(&observed, state->first, sizeof(observed)) &&
		     memcmp(&observed, state->second, sizeof(observed)))) {
			state->torn = true;
			break;
		}
		cond_resched();
	}
	kthread_unuse_mm(state->mm);
	complete(&state->reader_done);
	orlix_tcti_base_atomic_park_until_stop();
	return 0;
}

static void orlix_tcti_base_atomic_concurrent_no_tearing(struct kunit *test)
{
	static const struct orlix_tcti_base_atomic_pair first = {
		.low = 0x0123456789abcdefULL,
		.high = 0xfedcba9876543210ULL,
	};
	static const struct orlix_tcti_base_atomic_pair second = {
		.low = 0x1122334455667788ULL,
		.high = 0x8877665544332211ULL,
	};
	const struct orlix_tcti_test_atomic_source *source = NULL;
	struct orlix_tcti_decoded_instruction decoded;
	struct orlix_tcti_base_atomic_concurrent state;
	struct task_struct *writer = NULL;
	struct task_struct *reader = NULL;
	unsigned long mapped;
	size_t index;

	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		if (orlix_tcti_test_is_base_atomic(&orlix_tcti_test_sources[index]) &&
		    !strcmp(orlix_tcti_test_sources[index].name,
			    "CASPAL_CP64_comswappr")) {
			source = &orlix_tcti_test_sources[index];
			break;
		}
	}
	KUNIT_ASSERT_NOT_NULL(test, source);
	decoded = orlix_tcti_decode_aarch64(orlix_tcti_test_legal_instruction(source));
	KUNIT_ASSERT_EQ(test, ORLIX_TCTI_DECODE_LSE_ATOMIC, decoded.decode_class);
	KUNIT_ASSERT_TRUE(test, decoded.pair);
	KUNIT_ASSERT_TRUE(test, decoded.acquire);
	KUNIT_ASSERT_TRUE(test, decoded.release);
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm, mapped,
		&first, sizeof(first)));
	memset(&state, 0, sizeof(state));
	state.mm = current->mm;
	state.address = mapped;
	state.decoded = &decoded;
	state.first = &first;
	state.second = &second;
	init_completion(&state.start);
	init_completion(&state.reader_ready);
	init_completion(&state.writer_done);
	init_completion(&state.reader_done);
	atomic_set(&state.writer_running, 1);
	writer = kthread_run(orlix_tcti_base_atomic_pair_writer, &state,
		"orlix-tcti-base-pair-writer");
	if (IS_ERR(writer)) {
		KUNIT_FAIL(test, "failed to create pair writer");
		writer = NULL;
		goto out_unmap;
	}
	reader = kthread_run(orlix_tcti_base_atomic_pair_reader, &state,
		"orlix-tcti-base-pair-reader");
	if (IS_ERR(reader)) {
		KUNIT_FAIL(test, "failed to create pair reader");
		reader = NULL;
		goto out_stop;
	}
	if (!wait_for_completion_timeout(&state.reader_ready,
		msecs_to_jiffies(5000))) {
		KUNIT_FAIL(test, "pair reader did not become ready");
		goto out_stop;
	}
	complete_all(&state.start);
	if (!wait_for_completion_timeout(&state.writer_done,
		msecs_to_jiffies(5000)))
		KUNIT_FAIL(test, "pair writer timed out");
	if (!wait_for_completion_timeout(&state.reader_done,
		msecs_to_jiffies(5000)))
		KUNIT_FAIL(test, "pair reader timed out");

out_stop:
	/*
	 * Created tasks still reference this stack and current->mm. Open the
	 * start gate, then join the reader before the writer so kthread_stop
	 * never races a finished worker.
	 */
	complete_all(&state.start);
	if (reader)
		kthread_stop(reader);
	if (writer)
		kthread_stop(writer);
	KUNIT_EXPECT_EQ(test, 0, state.writer_ret);
	KUNIT_EXPECT_EQ(test, 0, state.reader_ret);
	KUNIT_EXPECT_FALSE(test, state.torn);

out_unmap:
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

static void orlix_tcti_base_atomic_exclusive_monitor_is_exact(struct kunit *test)
{
	const struct orlix_tcti_test_atomic_source *load =
		orlix_tcti_test_base_atomic_name("LDXR_LR64_ldstexclr");
	const struct orlix_tcti_test_atomic_source *store =
		orlix_tcti_test_base_atomic_name("STXR_SR64_ldstexclr");
	struct orlix_tcti_decoded_instruction load_decoded;
	struct orlix_tcti_decoded_instruction store_decoded;
	struct pt_regs regs = {};
	unsigned long mapped;
	u64 initial = 0x1122334455667788ULL;
	u64 interference = 0xaabbccddeeff0011ULL;
	u64 desired = 0x8877665544332211ULL;
	u64 observed = 0;

	KUNIT_ASSERT_NOT_NULL(test, load);
	KUNIT_ASSERT_NOT_NULL(test, store);
	load_decoded = orlix_tcti_decode_aarch64(
		orlix_tcti_test_legal_instruction(load));
	store_decoded = orlix_tcti_decode_aarch64(
		orlix_tcti_test_legal_instruction(store));
	mapped = ksys_mmap_pgoff(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm, mapped,
		&initial, load_decoded.access_size));
	regs.regs[load_decoded.rn] = mapped;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_switch_debug_execute_decoded(current->mm,
		&regs, &load_decoded, NULL));
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm, mapped,
		&interference, load_decoded.access_size));
	regs.regs[store_decoded.rn] = mapped;
	regs.regs[store_decoded.rt] = desired;
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_switch_debug_execute_decoded(current->mm,
		&regs, &store_decoded, NULL));
	KUNIT_EXPECT_EQ(test, 1ULL, regs.regs[store_decoded.rs]);
	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, mapped,
		&observed, load_decoded.access_size));
	KUNIT_EXPECT_EQ(test, interference, observed);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, PAGE_SIZE));
}

struct orlix_tcti_base_atomic_ordering_state {
	struct mm_struct *mm;
	unsigned long data_address;
	unsigned long flag_address;
	const struct orlix_tcti_decoded_instruction *store;
	const struct orlix_tcti_decoded_instruction *load;
	struct completion start;
	struct completion producer_done;
	struct completion consumer_done;
	int producer_ret;
	int consumer_ret;
	bool forbidden;
};

static int orlix_tcti_base_atomic_ordering_producer(void *data)
{
	struct orlix_tcti_base_atomic_ordering_state *state = data;
	struct pt_regs regs = {};
	u64 payload = 1;

	kthread_use_mm(state->mm);
	wait_for_completion(&state->start);
	state->producer_ret = orlix_tcti_write_user_data(state->mm,
		state->data_address, &payload, sizeof(payload));
	if (!state->producer_ret) {
		regs.regs[state->store->rn] = state->flag_address;
		regs.regs[state->store->rt] = 1;
		state->producer_ret = orlix_tcti_switch_debug_execute_decoded(
			state->mm, &regs, state->store, NULL);
	}
	kthread_unuse_mm(state->mm);
	complete(&state->producer_done);
	orlix_tcti_base_atomic_park_until_stop();
	return 0;
}

static int orlix_tcti_base_atomic_ordering_consumer(void *data)
{
	struct orlix_tcti_base_atomic_ordering_state *state = data;
	struct pt_regs regs = {};
	u64 payload = 0;
	unsigned int spin;

	kthread_use_mm(state->mm);
	wait_for_completion(&state->start);
	for (spin = 0; spin < 100000U && !kthread_should_stop(); spin++) {
		regs.regs[state->load->rn] = state->flag_address;
		state->consumer_ret = orlix_tcti_switch_debug_execute_decoded(
			state->mm, &regs, state->load, NULL);
		if (state->consumer_ret || regs.regs[state->load->rt])
			break;
		cpu_relax();
		cond_resched();
	}
	if (!state->consumer_ret && regs.regs[state->load->rt]) {
		state->consumer_ret = orlix_tcti_read_user_data(state->mm,
			state->data_address, &payload, sizeof(payload));
		state->forbidden = !state->consumer_ret && !payload;
	} else if (!state->consumer_ret) {
		state->consumer_ret = -ETIMEDOUT;
	}
	kthread_unuse_mm(state->mm);
	complete(&state->consumer_done);
	orlix_tcti_base_atomic_park_until_stop();
	return 0;
}

static void orlix_tcti_base_atomic_forbidden_ordering_outcomes(struct kunit *test)
{
	static const struct {
		const char *store;
		const char *load;
		bool rcpc;
		bool limited;
	} profiles[] = {
		{ "STLR_SL64_ldstord", "LDAR_LR64_ldstord", false, false },
		{ "STLR_SL64_ldstord", "LDAPR_64L_memop", true, false },
		{ "STLLR_SL64_ldstord", "LDLAR_LR64_ldstord", false, true },
	};
	unsigned long mapped;
	size_t index;

	mapped = ksys_mmap_pgoff(0, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	for (index = 0; index < ARRAY_SIZE(profiles); index++) {
		const struct orlix_tcti_test_atomic_source *store_source =
			orlix_tcti_test_base_atomic_name(profiles[index].store);
		const struct orlix_tcti_test_atomic_source *load_source =
			orlix_tcti_test_base_atomic_name(profiles[index].load);
		struct orlix_tcti_decoded_instruction store;
		struct orlix_tcti_decoded_instruction load;
		struct orlix_tcti_base_atomic_ordering_state state = {};
		struct task_struct *producer;
		struct task_struct *consumer;
		u64 zero = 0;

		KUNIT_ASSERT_NOT_NULL(test, store_source);
		KUNIT_ASSERT_NOT_NULL(test, load_source);
		store = orlix_tcti_decode_aarch64(
			orlix_tcti_test_legal_instruction(store_source));
		load = orlix_tcti_decode_aarch64(
			orlix_tcti_test_legal_instruction(load_source));
		KUNIT_ASSERT_TRUE(test, store.release);
		KUNIT_ASSERT_TRUE(test, load.acquire);
		KUNIT_EXPECT_EQ(test, profiles[index].rcpc, load.rcpc_acquire);
		KUNIT_EXPECT_EQ(test, profiles[index].limited,
			load.limited_ordering);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm,
			mapped, &zero, sizeof(zero)));
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm,
			mapped + PAGE_SIZE, &zero, sizeof(zero)));
		state.mm = current->mm;
		state.data_address = mapped;
		state.flag_address = mapped + PAGE_SIZE;
		state.store = &store;
		state.load = &load;
		init_completion(&state.start);
		init_completion(&state.producer_done);
		init_completion(&state.consumer_done);
		producer = kthread_run(orlix_tcti_base_atomic_ordering_producer,
			&state, "orlix-tcti-order-producer");
		KUNIT_ASSERT_FALSE(test, IS_ERR(producer));
		consumer = kthread_run(orlix_tcti_base_atomic_ordering_consumer,
			&state, "orlix-tcti-order-consumer");
		if (IS_ERR(consumer)) {
			complete_all(&state.start);
			kthread_stop(producer);
			KUNIT_FAIL(test, "failed to create ordering consumer");
			break;
		}
		complete_all(&state.start);
		if (!wait_for_completion_timeout(&state.producer_done,
			msecs_to_jiffies(5000)))
			KUNIT_FAIL(test, "ordering producer timed out");
		if (!wait_for_completion_timeout(&state.consumer_done,
			msecs_to_jiffies(5000)))
			KUNIT_FAIL(test, "ordering consumer timed out");
		kthread_stop(consumer);
		kthread_stop(producer);
		KUNIT_EXPECT_EQ_MSG(test, 0, state.producer_ret, "profile=%zu",
			index);
		KUNIT_EXPECT_EQ_MSG(test, 0, state.consumer_ret, "profile=%zu",
			index);
		KUNIT_EXPECT_FALSE_MSG(test, state.forbidden, "profile=%zu", index);
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(mapped, 2 * PAGE_SIZE));
}

#define BAS_SVC 0xd4000001U
#define BAS_RN 1U
#define BAS_RN_MISMATCH 4U
#define BAS_RT 0U
#define BAS_RT2 2U
#define BAS_RS 3U

static bool orlix_tcti_base_atomic_name_has(
	const struct orlix_tcti_test_atomic_source *source, const char *token)
{
	return strstr(source->name, token) != NULL;
}

static bool orlix_tcti_base_atomic_is_pair_exclusive(
	const struct orlix_tcti_test_atomic_source *source)
{
	return orlix_tcti_base_atomic_name_has(source, "ldstexclp") ||
		!strcmp(source->operation, "CASP") ||
		!strcmp(source->operation, "CASPT") ||
		orlix_tcti_base_atomic_name_has(source, "memop_128") ||
		orlix_tcti_base_atomic_name_has(source, "rcwcomswappr");
}

static bool orlix_tcti_base_atomic_is_exclusive_family(
	const struct orlix_tcti_test_atomic_source *source)
{
	return orlix_tcti_base_atomic_name_has(source, "ldstexcl") ||
		orlix_tcti_base_atomic_name_has(source, "ldstord");
}

static bool orlix_tcti_base_atomic_is_exclusive_store(
	const struct orlix_tcti_test_atomic_source *source)
{
	return orlix_tcti_base_atomic_is_exclusive_family(source) &&
		source->mnemonic[0] == 'S' &&
		orlix_tcti_base_atomic_name_has(source, "ldstexcl");
}

static bool orlix_tcti_base_atomic_is_load(
	const struct orlix_tcti_test_atomic_source *source)
{
	return source->mnemonic[0] == 'L' ||
		!strncmp(source->mnemonic, "CAS", 3) ||
		!strncmp(source->mnemonic, "SWP", 3) ||
		!strncmp(source->mnemonic, "RCW", 3);
}

static u32 orlix_tcti_base_atomic_fill_regs(
	const struct orlix_tcti_test_atomic_source *source, u32 instruction,
	bool load)
{
	u32 slots = (0x1fU << 16) | (0x1fU << 10) | (0x1fU << 5) | 0x1fU;
	u32 clear = slots & ~source->mask;
	u32 fields = (BAS_RN << 5) | BAS_RT;

	if (!strcmp(source->operation, "CASP") ||
	    !strcmp(source->operation, "CASPT") ||
	    orlix_tcti_base_atomic_name_has(source, "memop_128") ||
	    orlix_tcti_base_atomic_name_has(source, "rcwcomswappr")) {
		fields = (BAS_RN << 5) | 8U | (2U << 16);
	} else if (orlix_tcti_base_atomic_name_has(source, "ldstexclp")) {
		fields = (BAS_RN << 5) | BAS_RT | (BAS_RT2 << 10);
		if (load)
			fields |= 31U << 16;
		else
			fields |= BAS_RS << 16;
	} else if (orlix_tcti_base_atomic_is_exclusive_family(source)) {
		if (load)
			fields |= 31U << 16;
		else
			fields |= BAS_RS << 16;
		if (orlix_tcti_base_atomic_is_pair_exclusive(source))
			fields |= BAS_RT2 << 10;
		else
			fields |= 31U << 10;
	} else if (!strcmp(source->mnemonic, "ST64B") ||
		   !strcmp(source->mnemonic, "ST64BV") ||
		   !strcmp(source->mnemonic, "ST64BV0") ||
		   !strcmp(source->mnemonic, "LD64B")) {
		/* Even Rt, Rn outside Xt..Xt+7 so the base address is not stored. */
		fields = (BAS_RN << 5) | 2U;
	} else if (orlix_tcti_base_atomic_name_has(source, "ldapstl_unscaled") ||
		   orlix_tcti_base_atomic_name_has(source, "ldapstl_simd") ||
		   orlix_tcti_base_atomic_name_has(source, "ldapstl_writeback") ||
		   (!strncmp(source->mnemonic, "LDAPR", 5) &&
		    orlix_tcti_base_atomic_name_has(source, "_memop"))) {
		fields = (BAS_RN << 5) | BAS_RT;
	} else if (orlix_tcti_base_atomic_name_has(source, "ldiappstilp")) {
		fields = (BAS_RN << 5) | BAS_RT | (BAS_RS << 16);
	} else if (!strncmp(source->mnemonic, "STF", 3) ||
		   !strncmp(source->mnemonic, "STBF", 4)) {
		fields = (BAS_RN << 5) | (BAS_RS << 16) | 31U;
	} else {
		fields |= BAS_RS << 16;
	}
	return (instruction & ~clear) | (fields & clear);
}

static u32 orlix_tcti_base_atomic_fill_regs_rn(
	const struct orlix_tcti_test_atomic_source *source, u32 instruction,
	bool load, u32 rn)
{
	u32 insn = orlix_tcti_base_atomic_fill_regs(source, instruction, load);
	u32 clear = (0x1fU << 5) & ~source->mask;

	return (insn & ~clear) | ((rn << 5) & clear);
}

static u32 orlix_tcti_base_atomic_production_instruction(
	const struct orlix_tcti_test_atomic_source *source)
{
	return orlix_tcti_base_atomic_fill_regs(source, source->pattern,
		orlix_tcti_base_atomic_is_load(source) &&
		orlix_tcti_base_atomic_name_has(source, "ldstexcl"));
}

static u32 orlix_tcti_base_atomic_exclusive_setup_load(
	const struct orlix_tcti_test_atomic_source *source)
{
	u32 load = source->pattern;

	load |= BIT(22);
	load &= ~BIT(15);
	return orlix_tcti_base_atomic_fill_regs(source, load, true);
}

static bool orlix_tcti_base_atomic_decode_is_atomic(
	enum orlix_tcti_decode_class decode_class)
{
	return decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_EXCLUSIVE ||
		decode_class == ORLIX_TCTI_DECODE_LSE_ATOMIC ||
		decode_class == ORLIX_TCTI_DECODE_LS64 ||
		decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE ||
		decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE ||
		decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_PAIR;
}

/*
 * Product Kconfig selects FEAT_THE and the scalar RCW domain (D128=n).
 * Pair RCW is UNDEFINED in that domain; scalar RCW is UNDEFINED if D128=y.
 */
static bool orlix_tcti_base_atomic_rcw_defined_in_product(
	const struct orlix_tcti_decoded_instruction *decoded,
	const struct orlix_tcti_rcw_el1_state *state)
{
	if (!decoded->atomic_rcw)
		return true;
	return state->feat_the && decoded->pair == state->feat_d128;
}

static int orlix_tcti_base_atomic_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void orlix_tcti_base_atomic_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static unsigned long orlix_tcti_base_atomic_map_program(struct kunit *test,
							const u32 *program,
							size_t words)
{
	u8 bytes[16] = {};

	KUNIT_ASSERT_LE(test, words * sizeof(u32), sizeof(bytes));
	memcpy(bytes, program, words * sizeof(u32));
	return orlix_tcti_memory_proof_map_bytes(test, bytes, sizeof(bytes),
						 PROT_READ | PROT_EXEC);
}

static void orlix_tcti_base_atomic_expect_success(struct kunit *test,
	const struct orlix_tcti_test_atomic_source *source,
	const struct orlix_tcti_result *result, const struct pt_regs *regs,
	unsigned long code)
{
	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result->reason,
			    "%s ordinal %u reason", source->name, source->ordinal);
	KUNIT_EXPECT_EQ(test, 0L, result->status);
	KUNIT_EXPECT_EQ(test, BAS_SVC, result->instruction);
	KUNIT_EXPECT_TRUE_MSG(test,
			      regs->pc == code + sizeof(u32) ||
			      regs->pc == code + 2 * sizeof(u32) ||
			      regs->pc == code + 3 * sizeof(u32),
			      "%s ordinal %u pc %#llx", source->name,
			      source->ordinal, (unsigned long long)regs->pc);
}

static void orlix_tcti_base_atomic_capture_run(struct kunit *test,
	const struct orlix_tcti_test_atomic_source *source, u32 obligation,
	struct pt_regs *regs, unsigned long code, bool expect_fault)
{
	const void *token;
	struct orlix_tcti_native_capture_session *capture = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_result result;
	int ret;

	token = orlix_tcti_base_atomic_production_capture_token(source->ordinal,
								obligation);
	KUNIT_EXPECT_TRUE_MSG(test, token != NULL, "%s token %u", source->name,
			      obligation);
	if (!token)
		return;
	ret = orlix_tcti_native_capture_begin(token, source->ordinal, obligation,
					      &capture);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s begin %u", source->name, obligation);
	result = orlix_tcti_resume_user(current, regs, current->mm);
	if (expect_fault)
		orlix_tcti_memory_proof_expect_user_fault(test, &result, code,
							  regs->pc, source->name);
	else
		orlix_tcti_base_atomic_expect_success(test, source, &result, regs,
						      code);
	ret = orlix_tcti_native_capture_take_wire(capture, &wire);
	KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s wire %u", source->name, obligation);
	if (!ret) {
		ledger = orlix_tcti_target_proof_ingestion_ledger_create(1U);
		KUNIT_ASSERT_NOT_NULL(test, ledger);
		KUNIT_EXPECT_EQ_MSG(test, 0,
			orlix_tcti_target_proof_ingest_native(ledger, &wire, NULL),
			"%s ingest %u", source->name, obligation);
		KUNIT_EXPECT_EQ(test, 1U, ledger->native_passed);
		KUNIT_EXPECT_LT_MSG(test,
			orlix_tcti_target_proof_ingest_native(ledger, &wire, NULL),
			0, "%s replay %u", source->name, obligation);
		KUNIT_EXPECT_EQ(test, 1U, ledger->native_passed);
		orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
	}
	orlix_tcti_native_wire_record_destroy(&wire);
	orlix_tcti_native_capture_destroy(capture);
}

static void orlix_tcti_base_atomic_seed(struct kunit *test,
	const struct orlix_tcti_test_atomic_source *source, struct pt_regs *regs,
	unsigned long code, unsigned long data, const u64 *stored, bool write_data)
{
	memset(regs, 0, sizeof(*regs));
	regs->pc = code;
	regs->pstate = PSR_MODE_EL0t;
	regs->syscallno = NO_SYSCALL;
	regs->regs[BAS_RN] = data;
	regs->regs[BAS_RT] = stored[0];
	regs->regs[BAS_RT2] = stored[1];
	regs->regs[BAS_RS] = stored[0];
	regs->regs[8] = stored[0];
	regs->regs[9] = stored[1];
	regs->regs[2] = stored[1];
	current->thread.user_simd_valid = 1;
	current->thread.user_simd[0] = stored[0];
	current->thread.user_simd[1] = stored[1];
	current->thread.user_simd[BAS_RS * 2] = stored[0];
	current->thread.user_simd[BAS_RS * 2 + 1] = stored[1];
	if (write_data)
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm,
			data, stored, 16));
}

static void orlix_tcti_base_atomic_place_address(struct pt_regs *regs,
	const struct orlix_tcti_decoded_instruction *decoded, unsigned long data)
{
	if (decoded->memory_index_mode == ORLIX_TCTI_MEMORY_INDEX_PRE &&
	    decoded->memory_offset < 0)
		regs->regs[BAS_RN] = data +
			(unsigned long)(-(s64)decoded->memory_offset);
	else
		regs->regs[BAS_RN] = data;
}

static void orlix_tcti_base_atomic_production_resume(struct kunit *test)
{
	struct orlix_tcti_rcw_el1_state rcw_state;
	size_t index;
	size_t seen = 0;

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_rcw_el1_state_read(&rcw_state));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_atomic_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		struct orlix_tcti_result result;
		unsigned long code;
		unsigned long data;
		u64 stored[2] = { 0x0102030480000001ULL, 0x1112131415161718ULL };
		u32 program[3];
		size_t words = 0;
		u8 sentinel[32];

		if (!orlix_tcti_test_is_base_atomic(source))
			continue;
		seen++;
		instruction = orlix_tcti_base_atomic_production_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		KUNIT_ASSERT_TRUE_MSG(test,
			orlix_tcti_base_atomic_decode_is_atomic(decoded.decode_class),
			"%s ordinal %u class %u insn %#x", source->name,
			source->ordinal, decoded.decode_class, instruction);
		memset(sentinel, 0x5a, sizeof(sentinel));
		data = orlix_tcti_memory_proof_map_bytes(test, sentinel,
			sizeof(sentinel), PROT_READ | PROT_WRITE);
		if (orlix_tcti_base_atomic_is_exclusive_store(source))
			program[words++] =
				orlix_tcti_base_atomic_exclusive_setup_load(source);
		program[words++] = instruction;
		program[words++] = BAS_SVC;
		code = orlix_tcti_base_atomic_map_program(test, program, words);
		orlix_tcti_base_atomic_seed(test, source, &regs, code, data,
					    stored, true);
		orlix_tcti_base_atomic_place_address(&regs, &decoded, data);
		if (!orlix_tcti_base_atomic_rcw_defined_in_product(&decoded,
								  &rcw_state)) {
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			KUNIT_EXPECT_EQ_MSG(test,
				ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason,
				"%s ordinal %u undefined RCW domain", source->name,
				source->ordinal);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
			continue;
		}
		orlix_tcti_base_atomic_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &regs, code,
			false);
		orlix_tcti_base_atomic_seed(test, source, &regs, code, data,
					    stored, true);
		orlix_tcti_base_atomic_place_address(&regs, &decoded, data);
		orlix_tcti_base_atomic_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_MEMORY, &regs, code,
			false);
		orlix_tcti_base_atomic_seed(test, source, &regs, code, data,
					    stored, true);
		orlix_tcti_base_atomic_place_address(&regs, &decoded, data);
		orlix_tcti_base_atomic_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_PC, &regs, code, false);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, 498U, seen);
}

static void orlix_tcti_base_atomic_unmapped_faults(struct kunit *test)
{
	struct orlix_tcti_rcw_el1_state rcw_state;
	size_t index;
	size_t seen = 0;

	KUNIT_ASSERT_EQ(test, 0, orlix_tcti_rcw_el1_state_read(&rcw_state));
	for (index = 0; index < ARRAY_SIZE(orlix_tcti_test_sources); index++) {
		const struct orlix_tcti_test_atomic_source *source =
			&orlix_tcti_test_sources[index];
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct pt_regs regs = {};
		struct orlix_tcti_result result;
		unsigned long code;
		u32 program[3];
		size_t words = 0;
		u64 stored[2] = { 1, 2 };

		if (!orlix_tcti_test_is_base_atomic(source))
			continue;
		seen++;
		if (orlix_tcti_base_atomic_is_exclusive_store(source))
			continue;
		instruction = orlix_tcti_base_atomic_production_instruction(source);
		decoded = orlix_tcti_decode_aarch64(instruction);
		program[words++] = instruction;
		program[words++] = BAS_SVC;
		code = orlix_tcti_base_atomic_map_program(test, program, words);
		orlix_tcti_base_atomic_seed(test, source, &regs, code, 0x2000UL,
					    stored, false);
		if (!orlix_tcti_base_atomic_rcw_defined_in_product(&decoded,
								  &rcw_state)) {
			result = orlix_tcti_resume_user(current, &regs, current->mm);
			KUNIT_EXPECT_EQ_MSG(test,
				ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION, result.reason,
				"%s ordinal %u undefined RCW before memory",
				source->name, source->ordinal);
			KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
			continue;
		}
		orlix_tcti_base_atomic_capture_run(test, source,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_FAULTS, &regs, code,
			true);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	}
	KUNIT_EXPECT_EQ(test, 498U, seen);
}

static void orlix_tcti_base_atomic_monitor_mismatch_fails_store(struct kunit *test)
{
	const struct orlix_tcti_test_atomic_source *store =
		orlix_tcti_test_base_atomic_name("STXR_SR64_ldstexclr");
	const struct orlix_tcti_test_atomic_source *load =
		orlix_tcti_test_base_atomic_name("LDXR_LR64_ldstexclr");
	u32 program[3];
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	unsigned long code;
	unsigned long data;
	u64 old = 0x1111;
	u64 desired = 0x2222;
	u64 observed = 0;

	KUNIT_ASSERT_NOT_NULL(test, store);
	KUNIT_ASSERT_NOT_NULL(test, load);
	data = orlix_tcti_memory_proof_map_bytes(test, &old, sizeof(old),
						 PROT_READ | PROT_WRITE);
	program[0] = orlix_tcti_base_atomic_fill_regs(load, load->pattern, true);
	program[1] = orlix_tcti_base_atomic_fill_regs_rn(store, store->pattern,
							 false, BAS_RN_MISMATCH);
	program[2] = BAS_SVC;
	code = orlix_tcti_base_atomic_map_program(test, program, 3);
	memset(&regs, 0, sizeof(regs));
	regs.pc = code;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[BAS_RN] = data;
	regs.regs[BAS_RN_MISMATCH] = data + 8;
	regs.regs[BAS_RT] = desired;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason);
	KUNIT_EXPECT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, data,
							   &observed, 8));
	KUNIT_EXPECT_EQ(test, old, observed);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static void orlix_tcti_base_atomic_casp_whole_pair(struct kunit *test)
{
	const struct orlix_tcti_test_atomic_source *leaf =
		orlix_tcti_test_base_atomic_name("CASP_CP64_comswappr");
	u32 instruction;
	struct pt_regs regs = {};
	struct orlix_tcti_result result;
	unsigned long code;
	unsigned long data;
	u64 old[2] = { 0x1111111111111111ULL, 0x2222222222222222ULL };
	u64 desired[2] = { 0xaaaaaaaaaaaaaaaaULL, 0xbbbbbbbbbbbbbbbbULL };
	u64 observed[2] = {};

	KUNIT_ASSERT_NOT_NULL(test, leaf);
	instruction = orlix_tcti_base_atomic_fill_regs(leaf, leaf->pattern, false);
	data = orlix_tcti_memory_proof_map_bytes(test, old, sizeof(old),
						 PROT_READ | PROT_WRITE);
	code = orlix_tcti_base_atomic_map_program(test,
		(u32[]){ instruction, BAS_SVC }, 2);
	memset(&regs, 0, sizeof(regs));
	regs.pc = code;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[BAS_RN] = data;
	regs.regs[2] = old[0];
	regs.regs[3] = old[1];
	regs.regs[8] = desired[0];
	regs.regs[9] = desired[1];
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	orlix_tcti_base_atomic_expect_success(test, leaf, &result, &regs, code);
	KUNIT_EXPECT_EQ(test, 0, orlix_tcti_read_user_data(current->mm, data,
							   observed, 16));
	KUNIT_EXPECT_EQ(test, desired[0], observed[0]);
	KUNIT_EXPECT_EQ(test, desired[1], observed[1]);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
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
	KUNIT_CASE(orlix_tcti_base_atomic_concurrent_no_tearing),
	KUNIT_CASE(orlix_tcti_base_atomic_exclusive_monitor_is_exact),
	KUNIT_CASE(orlix_tcti_base_atomic_forbidden_ordering_outcomes),
	KUNIT_CASE(orlix_tcti_base_atomic_production_resume),
	KUNIT_CASE(orlix_tcti_base_atomic_unmapped_faults),
	KUNIT_CASE(orlix_tcti_base_atomic_monitor_mismatch_fails_store),
	KUNIT_CASE(orlix_tcti_base_atomic_casp_whole_pair),
	{}
};

static struct kunit_suite orlix_tcti_base_atomic_source_bound_suite = {
	.name = "orlix-tcti-base-atomic-source-bound",
	.init = orlix_tcti_base_atomic_test_init,
	.exit = orlix_tcti_base_atomic_test_exit,
	.test_cases = orlix_tcti_base_atomic_source_bound_cases,
};

kunit_test_suite(orlix_tcti_base_atomic_source_bound_suite);
