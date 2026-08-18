// SPDX-License-Identifier: GPL-2.0-only
/*
 * Exact production-path proof for the 209 BASE_LOAD_STORE leaves owned by
 * GitHub #134. Shared helpers stay here so later memory families can reuse
 * them without taking these ordinals.
 */
#include <asm/ptrace.h>
#include <asm/orlix_tcti.h>
#include <kunit/test.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/string.h>
#include <linux/syscalls.h>

#include "../decode_aarch64.h"
#include "orlix_tcti_memory_proof.h"
#include "target_execution_slice_map.h"
#include "target_native_proof_contract_private.h"
#include "orlix_tcti_native_observation.h"
#include "orlix_tcti_base_load_store_production_capture.h"
#include "target_proof_ingestion_private.h"

#define BLS_SVC 0xd4000001U

struct bls_source_leaf {
	u32 ordinal;
	const char *name;
	const char *mnemonic;
	const char *operation;
	u32 mask;
	u32 pattern;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(ordinal, id, mnemonic, operation, mask, \
					     pattern, feature_predicate, offset, length) \
	{ ordinal, id, mnemonic, operation, mask, pattern },
static const struct bls_source_leaf bls_source_leaves[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

static const struct bls_source_leaf *bls_source_leaf(u32 ordinal)
{
	size_t index;

	for (index = 0; index < ARRAY_SIZE(bls_source_leaves); index++)
		if (bls_source_leaves[index].ordinal == ordinal)
			return &bls_source_leaves[index];
	return NULL;
}

static bool bls_name_has(const char *name, const char *token)
{
	return strstr(name, token) != NULL;
}

static bool bls_is_prefetch(const struct bls_source_leaf *leaf)
{
	return !strncmp(leaf->mnemonic, "PRFM", 4) ||
		!strcmp(leaf->mnemonic, "PRFUM") ||
		!strcmp(leaf->mnemonic, "RPRFM");
}

static bool bls_is_pair(const struct bls_source_leaf *leaf)
{
	return bls_name_has(leaf->name, "ldstpair") ||
		bls_name_has(leaf->name, "ldstnapair");
}

static bool bls_is_regoff(const struct bls_source_leaf *leaf)
{
	return bls_name_has(leaf->name, "ldst_regoff");
}

static bool bls_is_loadlit(const struct bls_source_leaf *leaf)
{
	return bls_name_has(leaf->name, "loadlit");
}

static bool bls_is_load(const struct bls_source_leaf *leaf)
{
	return leaf->mnemonic[0] == 'L';
}

static bool bls_is_simd(const struct bls_source_leaf *leaf)
{
	return bls_name_has(leaf->name, "_S_") ||
		bls_name_has(leaf->name, "_D_") ||
		bls_name_has(leaf->name, "_Q_") ||
		bls_name_has(leaf->name, "_B_ldst") ||
		bls_name_has(leaf->name, "_H_ldst") ||
		bls_name_has(leaf->name, "_BL_") ||
		!strcmp(leaf->operation, "LDR_imm_fpsimd") ||
		!strcmp(leaf->operation, "STR_imm_fpsimd") ||
		!strcmp(leaf->operation, "LDR_reg_fpsimd") ||
		!strcmp(leaf->operation, "STR_reg_fpsimd") ||
		!strcmp(leaf->operation, "LDUR_fpsimd") ||
		!strcmp(leaf->operation, "STUR_fpsimd") ||
		!strcmp(leaf->operation, "LDP_fpsimd") ||
		!strcmp(leaf->operation, "STP_fpsimd") ||
		!strcmp(leaf->operation, "LDNP_fpsimd") ||
		!strcmp(leaf->operation, "STNP_fpsimd") ||
		!strcmp(leaf->operation, "LDTP_fpsimd") ||
		!strcmp(leaf->operation, "STTP_fpsimd") ||
		!strcmp(leaf->operation, "LDTNP_fpsimd") ||
		!strcmp(leaf->operation, "STTNP_fpsimd") ||
		!strcmp(leaf->operation, "LDR_lit_fpsimd");
}

static bool bls_is_gcs(const struct bls_source_leaf *leaf)
{
	return !strcmp(leaf->mnemonic, "GCSSTR") ||
		!strcmp(leaf->mnemonic, "GCSSTTR");
}

static bool bls_is_stgp(const struct bls_source_leaf *leaf)
{
	return !strcmp(leaf->mnemonic, "STGP");
}

static bool bls_is_rprfm(const struct bls_source_leaf *leaf)
{
	return !strcmp(leaf->mnemonic, "RPRFM");
}

static u32 bls_test_instruction(const struct bls_source_leaf *leaf)
{
	u32 instruction = leaf->pattern;

	instruction |= 1U << 5; /* Rn = x1 */
	if (bls_is_rprfm(leaf))
		instruction |= 24U; /* Rt 24-31 selects RPRFM */
	if (bls_is_pair(leaf)) {
		instruction |= 2U << 10; /* Rt2 = x2 */
		instruction |= 1U << 15; /* imm7 = 1, observable writeback */
	}
	if (bls_is_regoff(leaf)) {
		instruction |= 3U << 16; /* Rm = x3 */
		if (bls_name_has(leaf->name, "32BL") ||
		    bls_name_has(leaf->name, "_BL_"))
			instruction |= 3U << 13;
		else
			instruction |= 2U << 13;
	} else if (bls_name_has(leaf->name, "ldst_pos")) {
		instruction |= 1U << 10; /* imm12 = 1 */
	} else if (!bls_is_pair(leaf) && !bls_is_loadlit(leaf) &&
		   !bls_is_gcs(leaf) &&
		   (bls_name_has(leaf->name, "ldst_immpost") ||
		    bls_name_has(leaf->name, "ldst_immpre") ||
		    bls_name_has(leaf->name, "ldst_unscaled") ||
		    bls_name_has(leaf->name, "ldst_unpriv"))) {
		instruction |= 8U << 12; /* imm9 = 8 */
	}
	if (bls_is_loadlit(leaf))
		instruction = (leaf->pattern & leaf->mask) | (2U << 5);
	return instruction;
}

static bool bls_decode_is_memory(enum orlix_tcti_decode_class decode_class)
{
	return decode_class == ORLIX_TCTI_DECODE_LOAD_LITERAL ||
		decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_PAIR ||
		decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE ||
		decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE ||
		decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET ||
		decode_class == ORLIX_TCTI_DECODE_HINT;
}

static int bls_test_init(struct kunit *test)
{
	struct mm_struct *mm = mm_alloc();

	if (!mm)
		return -ENOMEM;
	kthread_use_mm(mm);
	test->priv = mm;
	return 0;
}

static void bls_test_exit(struct kunit *test)
{
	struct mm_struct *mm = test->priv;

	if (!mm)
		return;
	kthread_unuse_mm(mm);
	mmput(mm);
}

static void bls_for_each_member(struct kunit *test,
	void (*visit)(struct kunit *test, const struct bls_source_leaf *leaf))
{
	const struct orlix_tcti_execution_slice_map *map =
		orlix_tcti_execution_slice_map_canonical();
	size_t index;
	size_t seen = 0;

	KUNIT_ASSERT_NOT_NULL(test, map);
	for (index = 0; index < map->counts.leaf_count; index++) {
		const struct orlix_tcti_execution_slice_member *member =
			&map->members[index];
		const struct orlix_tcti_execution_slice_family *family =
			&map->families[member->family_index];
		const struct bls_source_leaf *leaf;

		if (family->issue_id != 134U)
			continue;
		seen++;
		leaf = bls_source_leaf(member->ordinal);
		KUNIT_ASSERT_NOT_NULL(test, leaf);
		visit(test, leaf);
	}
	KUNIT_EXPECT_EQ(test, 209U, seen);
}

static void bls_visit_decode(struct kunit *test, const struct bls_source_leaf *leaf)
{
	u32 instruction = bls_test_instruction(leaf);
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(instruction);

	KUNIT_EXPECT_EQ_MSG(test, leaf->pattern, instruction & leaf->mask,
			    "%s ordinal %u", leaf->name, leaf->ordinal);
	KUNIT_EXPECT_TRUE_MSG(test, bls_decode_is_memory(decoded.decode_class),
		"%s ordinal %u class %u insn %#x", leaf->name, leaf->ordinal,
		decoded.decode_class, instruction);
	if (bls_is_prefetch(leaf))
		KUNIT_EXPECT_TRUE_MSG(test,
			decoded.decode_class == ORLIX_TCTI_DECODE_HINT ||
			decoded.prefetch,
			"%s ordinal %u prefetch", leaf->name, leaf->ordinal);
	else if (!bls_is_loadlit(leaf) &&
		 decoded.decode_class != ORLIX_TCTI_DECODE_HINT) {
		KUNIT_EXPECT_EQ_MSG(test, bls_is_load(leaf), decoded.load,
				    "%s ordinal %u load", leaf->name,
				    leaf->ordinal);
		KUNIT_EXPECT_EQ_MSG(test, bls_is_simd(leaf), decoded.simd_fp,
				    "%s ordinal %u simd", leaf->name,
				    leaf->ordinal);
		KUNIT_EXPECT_GT_MSG(test, decoded.access_size, 0,
				    "%s ordinal %u size", leaf->name,
				    leaf->ordinal);
	}
}

static void bls_source_decode(struct kunit *test)
{
	bls_for_each_member(test, bls_visit_decode);
}

static unsigned long bls_map_program_bytes(struct kunit *test, u32 instruction,
					   const void *literal, size_t literal_len)
{
	u8 program[32] = {};
	u32 svc = BLS_SVC;
	unsigned long mapped;

	memcpy(program, &instruction, sizeof(instruction));
	memcpy(program + sizeof(u32), &svc, sizeof(svc));
	if (literal && literal_len) {
		KUNIT_ASSERT_LE(test, literal_len, sizeof(program) - 2 * sizeof(u32));
		memcpy(program + 2 * sizeof(u32), literal, literal_len);
	}
	mapped = orlix_tcti_memory_proof_map_bytes(test, program, sizeof(program),
						   PROT_READ | PROT_EXEC);
	KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(mapped));
	return mapped;
}

static unsigned long bls_map_program(struct kunit *test, u32 instruction)
{
	return bls_map_program_bytes(test, instruction, NULL, 0);
}

static void bls_expect_success(struct kunit *test, const struct bls_source_leaf *leaf,
			       const struct orlix_tcti_result *result,
			       const struct pt_regs *regs, unsigned long code)
{
	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result->reason,
			    "%s ordinal %u reason", leaf->name, leaf->ordinal);
	KUNIT_EXPECT_EQ(test, 0L, result->status);
	KUNIT_EXPECT_EQ(test, BLS_SVC, result->instruction);
	KUNIT_EXPECT_TRUE_MSG(test,
			      regs->pc == code + sizeof(u32) ||
			      regs->pc == code + 2 * sizeof(u32),
			      "%s ordinal %u pc %#llx", leaf->name, leaf->ordinal,
			      (unsigned long long)regs->pc);
}

static u64 bls_expected_loaded(const struct orlix_tcti_decoded_instruction *decoded,
			       u64 stored)
{
	if (decoded->result_size == sizeof(u32) && !decoded->sign_extend_load)
		return (u32)stored;
	if (decoded->sign_extend_load && decoded->access_size == sizeof(u32))
		return (s64)(s32)stored;
	if (decoded->sign_extend_load && decoded->access_size == 1)
		return (s64)(s8)stored;
	if (decoded->sign_extend_load && decoded->access_size == 2)
		return (s64)(s16)stored;
	if (decoded->access_size == sizeof(u32) && !decoded->sign_extend_load)
		return (u32)stored;
	if (decoded->access_size == 1 && !decoded->sign_extend_load)
		return (u8)stored;
	if (decoded->access_size == 2 && !decoded->sign_extend_load)
		return (u16)stored;
	return stored;
}

static unsigned long bls_effective_address(
	const struct orlix_tcti_decoded_instruction *decoded,
	const struct pt_regs *before)
{
	if (decoded->decode_class == ORLIX_TCTI_DECODE_LOAD_LITERAL)
		return before->pc + decoded->memory_offset;
	if (decoded->decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET)
		return before->regs[1] + before->regs[3];
	if (decoded->memory_index_mode == ORLIX_TCTI_MEMORY_INDEX_POST)
		return before->regs[1];
	return before->regs[1] + decoded->memory_offset;
}

static void bls_visit_production(struct kunit *test, const struct bls_source_leaf *leaf)
{
	u32 instruction = bls_test_instruction(leaf);
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(instruction);
	struct orlix_tcti_native_capture_session *capture = NULL;
	struct orlix_tcti_native_wire_record wire = {};
	struct orlix_tcti_target_proof_ingestion_ledger *ledger;
	struct orlix_tcti_result result;
	struct pt_regs regs = {};
	struct pt_regs before;
	unsigned long code;
	unsigned long data = 0;
	unsigned long address;
	u64 stored[4] = {
		0x0102030480000001ULL, 0x1112131415161718ULL,
		0x212223247ffffffeULL, 0x3132333435363738ULL,
	};
	u8 sentinel[256];
	const void *token;
	size_t access;
	int ret;

	KUNIT_ASSERT_TRUE_MSG(test, bls_decode_is_memory(decoded.decode_class),
		"%s ordinal %u class %u", leaf->name, leaf->ordinal,
		decoded.decode_class);

	memset(sentinel, 0x5a, sizeof(sentinel));
	if (!bls_is_loadlit(leaf)) {
		data = orlix_tcti_memory_proof_map_bytes(test, sentinel,
							 sizeof(sentinel),
							 PROT_READ | PROT_WRITE);
		KUNIT_ASSERT_FALSE(test, IS_ERR_VALUE(data));
	}
	access = decoded.access_size ? decoded.access_size : 8;
	if (bls_is_loadlit(leaf) && !bls_is_prefetch(leaf))
		code = bls_map_program_bytes(test, instruction, stored, access);
	else
		code = bls_map_program(test, instruction);
	memset(&regs, 0, sizeof(regs));
	regs.pc = code;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[0] = stored[0];
	regs.regs[1] = data ? data + 16 : 0;
	regs.regs[2] = stored[2];
	regs.regs[3] = 0;
	current->thread.user_simd_valid = 1;
	current->thread.user_simd[0] = stored[0];
	current->thread.user_simd[1] = stored[1];
	current->thread.user_simd[4] = stored[2];
	current->thread.user_simd[5] = stored[3];
	if (bls_is_load(leaf) && !bls_is_prefetch(leaf) && data) {
		address = decoded.memory_index_mode == ORLIX_TCTI_MEMORY_INDEX_POST ?
			regs.regs[1] : regs.regs[1] + decoded.memory_offset;
		if (decoded.decode_class == ORLIX_TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET)
			address = regs.regs[1] + regs.regs[3];
		ret = orlix_tcti_write_user_data(current->mm, address, stored,
						 access);
		KUNIT_ASSERT_EQ(test, 0, ret);
		if (bls_is_pair(leaf))
			KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(
				current->mm, address + access, stored + 2, access));
		if (decoded.simd_fp && access > 8)
			KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(
				current->mm, address + 8, stored + 1, 8));
	}
	before = regs;
	token = orlix_tcti_base_load_store_production_capture_token(leaf->ordinal);
	KUNIT_EXPECT_TRUE_MSG(test, token != NULL, "%s capture token", leaf->name);
	if (token) {
		ret = orlix_tcti_native_capture_begin(token, leaf->ordinal,
			ORLIX_TCTI_TARGET_PROOF_OBLIGATION_REGISTERS, &capture);
		KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s begin", leaf->name);
	}
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	bls_expect_success(test, leaf, &result, &regs, code);
	address = bls_effective_address(&decoded, &before);

	if (bls_is_prefetch(leaf)) {
		KUNIT_EXPECT_EQ(test, before.regs[0], regs.regs[0]);
		KUNIT_EXPECT_EQ(test, before.regs[1], regs.regs[1]);
		if (data)
			orlix_tcti_memory_proof_expect_bytes(test, data, sentinel, 32,
							     leaf->name);
	} else if (bls_is_load(leaf)) {
		if (decoded.simd_fp) {
			orlix_tcti_memory_proof_expect_simd_dest(test, 0, access,
				stored[0], stored[1], leaf->name);
			if (bls_is_pair(leaf))
				orlix_tcti_memory_proof_expect_simd_dest(test, 2,
					access, stored[2], stored[3], leaf->name);
		} else {
			KUNIT_EXPECT_EQ_MSG(test,
				bls_expected_loaded(&decoded, stored[0]),
				regs.regs[0], "%s dest", leaf->name);
			if (bls_is_pair(leaf))
				KUNIT_EXPECT_EQ_MSG(test,
					bls_expected_loaded(&decoded, stored[2]),
					regs.regs[2], "%s dest2", leaf->name);
		}
	} else if (data) {
		u64 first = stored[0];
		u64 first_high = stored[1];

		if (decoded.simd_fp) {
			orlix_tcti_memory_proof_expect_bytes(test, address, &first,
				access > 8 ? 8 : access, leaf->name);
			if (access > 8)
				orlix_tcti_memory_proof_expect_bytes(test,
					address + 8, &first_high, 8, leaf->name);
		} else {
			if (access == sizeof(u32))
				first = (u32)stored[0];
			else if (access == 1)
				first = (u8)stored[0];
			else if (access == 2)
				first = (u16)stored[0];
			orlix_tcti_memory_proof_expect_bytes(test, address, &first,
				access > 8 ? 8 : access, leaf->name);
		}
		if (bls_is_pair(leaf)) {
			u64 second = stored[2];
			u64 second_high = stored[3];

			if (decoded.simd_fp) {
				orlix_tcti_memory_proof_expect_bytes(test,
					address + access, &second,
					access > 8 ? 8 : access, leaf->name);
				if (access > 8)
					orlix_tcti_memory_proof_expect_bytes(test,
						address + access + 8, &second_high,
						8, leaf->name);
			} else {
				if (access == sizeof(u32))
					second = (u32)stored[2];
				orlix_tcti_memory_proof_expect_bytes(test,
					address + access, &second,
					access > 8 ? 8 : access, leaf->name);
			}
		}
		if (bls_is_stgp(leaf)) {
			unsigned long tag_address = 0;
			u8 tag = 0xff;

			orlix_tcti_allocation_tag_last_store(&tag_address, &tag);
			KUNIT_EXPECT_EQ_MSG(test, address & ~0xfUL, tag_address,
					    "%s stgp tag addr", leaf->name);
			KUNIT_EXPECT_EQ_MSG(test, (address >> 56) & 0xfU, tag,
					    "%s stgp tag", leaf->name);
		}
	}
	if (!bls_is_prefetch(leaf) && !bls_is_loadlit(leaf) &&
	    decoded.decode_class != ORLIX_TCTI_DECODE_HINT)
		orlix_tcti_memory_proof_expect_writeback(test,
			decoded.memory_index_mode, before.regs[1],
			decoded.memory_offset, regs.regs[1], leaf->name);

	if (capture && result.reason == ORLIX_TCTI_EXIT_SYSCALL) {
		ret = orlix_tcti_native_capture_take_wire(capture, &wire);
		KUNIT_EXPECT_EQ_MSG(test, 0, ret, "%s wire", leaf->name);
		if (!ret) {
			ledger = orlix_tcti_target_proof_ingestion_ledger_create(1U);
			KUNIT_ASSERT_NOT_NULL(test, ledger);
			KUNIT_EXPECT_EQ_MSG(test, 0,
				orlix_tcti_target_proof_ingest_native(ledger, &wire, NULL),
				"%s ingest", leaf->name);
			KUNIT_EXPECT_EQ(test, 1U, ledger->native_passed);
			KUNIT_EXPECT_LT_MSG(test,
				orlix_tcti_target_proof_ingest_native(ledger, &wire, NULL),
				0, "%s replay", leaf->name);
			KUNIT_EXPECT_EQ(test, 1U, ledger->native_passed);
			orlix_tcti_target_proof_ingestion_ledger_destroy(ledger);
		}
	}
	orlix_tcti_native_wire_record_destroy(&wire);
	orlix_tcti_native_capture_destroy(capture);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	if (data)
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static void bls_production_resume(struct kunit *test)
{
	bls_for_each_member(test, bls_visit_production);
}

static u32 bls_reserved_instruction(const struct bls_source_leaf *leaf, u32 legal)
{
	u32 candidates[8];
	size_t count = 0;
	size_t index;

	if (bls_is_pair(leaf)) {
		candidates[count++] = legal & ~0x7c00U;
		candidates[count++] = (legal & ~0x1fU) | 1U;
		/* Integer opc=1 with mode=0 stays architecturally unallocated. */
		candidates[count++] = (legal & ~0xc1800000U) | (1U << 30);
	}
	if (bls_is_regoff(leaf))
		candidates[count++] = legal & ~(7U << 13);
	if (bls_is_loadlit(leaf))
		candidates[count++] = (legal & ~0xc0000000U) | (3U << 30) | BIT(26);
	if (bls_name_has(leaf->name, "ldst_pos") ||
	    bls_name_has(leaf->name, "ldst_immpost") ||
	    bls_name_has(leaf->name, "ldst_immpre") ||
	    bls_name_has(leaf->name, "ldst_unscaled") ||
	    bls_name_has(leaf->name, "ldst_unpriv")) {
		candidates[count++] = (legal & ~0x1fU) | 1U;
		candidates[count++] = (legal & ~0xc0c00000U) | 0xc0c00000U;
	}
	if (bls_is_gcs(leaf))
		candidates[count++] = legal ^ BIT(21);
	candidates[count++] = legal ^ BIT(21);
	candidates[count++] = (legal & ~0xc0c00000U) | 0xc0c00000U;
	candidates[count++] = 0x00010000U;
	for (index = 0; index < count; index++) {
		struct orlix_tcti_decoded_instruction decoded =
			orlix_tcti_decode_aarch64(candidates[index]);

		if (decoded.decode_class == ORLIX_TCTI_DECODE_UNSUPPORTED)
			return candidates[index];
	}
	return 0;
}

static void bls_visit_reserved(struct kunit *test, const struct bls_source_leaf *leaf)
{
	struct orlix_tcti_decoded_instruction decoded;
	struct orlix_tcti_result result;
	struct pt_regs regs = {};
	struct pt_regs before;
	unsigned long code;
	u32 legal = bls_test_instruction(leaf);
	u32 instruction = bls_reserved_instruction(leaf, legal);

	KUNIT_ASSERT_NE_MSG(test, 0U, instruction,
		"%s ordinal %u has no reserved encoding", leaf->name,
		leaf->ordinal);
	decoded = orlix_tcti_decode_aarch64(instruction);
	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_DECODE_UNSUPPORTED,
		decoded.decode_class, "%s reserved class insn %#x", leaf->name,
		instruction);
	code = bls_map_program(test, instruction);
	regs.pc = code;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[0] = 0x1111;
	regs.regs[1] = 0x2222;
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_UNSUPPORTED_INSTRUCTION,
		result.reason, "%s reserved reason", leaf->name);
	KUNIT_EXPECT_EQ(test, before.regs[0], regs.regs[0]);
	KUNIT_EXPECT_EQ(test, before.regs[1], regs.regs[1]);
	KUNIT_EXPECT_EQ(test, code, regs.pc);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
}

static void bls_reserved_is_rejected(struct kunit *test)
{
	bls_for_each_member(test, bls_visit_reserved);
}

static void bls_visit_unmapped_fault(struct kunit *test,
				     const struct bls_source_leaf *leaf)
{
	u32 instruction = bls_test_instruction(leaf);
	struct orlix_tcti_decoded_instruction decoded =
		orlix_tcti_decode_aarch64(instruction);
	struct orlix_tcti_result result;
	struct pt_regs regs = {};
	struct pt_regs before;
	unsigned long code;
	unsigned long data = 0;

	code = bls_map_program(test, instruction);
	regs.pc = code;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[0] = 0xabcdef;
	regs.regs[1] = 0x0000000200000000ULL;
	regs.regs[2] = 0x1111;
	regs.regs[3] = 0;
	if (bls_is_loadlit(leaf)) {
		/* imm19=4096 places the literal at pc+16KiB, past one page. */
		instruction = (leaf->pattern & leaf->mask) | (4096U << 5);
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
		code = bls_map_program(test, instruction);
		regs.pc = code;
	}
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	if (bls_is_prefetch(leaf)) {
		KUNIT_EXPECT_EQ_MSG(test, ORLIX_TCTI_EXIT_SYSCALL, result.reason,
				    "%s prefetch no-fault", leaf->name);
		KUNIT_EXPECT_EQ(test, before.regs[0], regs.regs[0]);
		KUNIT_EXPECT_EQ(test, before.regs[1], regs.regs[1]);
	} else {
		orlix_tcti_memory_proof_expect_user_fault(test, &result, code,
							  regs.pc, leaf->name);
		KUNIT_EXPECT_EQ(test, before.regs[0], regs.regs[0]);
		if (!bls_is_loadlit(leaf) &&
		    decoded.decode_class != ORLIX_TCTI_DECODE_HINT)
			KUNIT_EXPECT_EQ_MSG(test, before.regs[1], regs.regs[1],
					    "%s no writeback on fault", leaf->name);
	}
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	if (data)
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static void bls_unmapped_load_faults(struct kunit *test)
{
	bls_for_each_member(test, bls_visit_unmapped_fault);
}

static void bls_write_protect_store_faults(struct kunit *test)
{
	const struct bls_source_leaf *leaf = bls_source_leaf(3351U);
	u32 instruction;
	struct orlix_tcti_result result;
	struct pt_regs regs = {};
	struct pt_regs before;
	unsigned long code;
	unsigned long data;
	u8 sentinel[32];
	u64 stored = 0x0102030480000001ULL;

	KUNIT_ASSERT_NOT_NULL(test, leaf);
	instruction = bls_test_instruction(leaf);
	memset(sentinel, 0x5a, sizeof(sentinel));
	data = orlix_tcti_memory_proof_map_bytes(test, sentinel, sizeof(sentinel),
						 PROT_READ);
	code = bls_map_program(test, instruction);
	regs.pc = code;
	regs.pstate = PSR_MODE_EL0t;
	regs.syscallno = NO_SYSCALL;
	regs.regs[0] = stored;
	regs.regs[1] = data + 16;
	before = regs;
	result = orlix_tcti_resume_user(current, &regs, current->mm);
	orlix_tcti_memory_proof_expect_user_fault(test, &result, code, regs.pc,
						  leaf->name);
	KUNIT_EXPECT_EQ(test, before.regs[0], regs.regs[0]);
	KUNIT_EXPECT_EQ(test, before.regs[1], regs.regs[1]);
	orlix_tcti_memory_proof_expect_bytes(test, data, sentinel, sizeof(sentinel),
					     leaf->name);
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
	KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, PAGE_SIZE));
}

static void bls_pair_second_element_faults(struct kunit *test)
{
	static const u32 pair_ordinals[] = { 2894U, 2895U };
	size_t index;

	for (index = 0; index < ARRAY_SIZE(pair_ordinals); index++) {
		const struct bls_source_leaf *leaf =
			bls_source_leaf(pair_ordinals[index]);
		u32 instruction;
		struct orlix_tcti_decoded_instruction decoded;
		struct orlix_tcti_result result;
		struct pt_regs regs = {};
		struct pt_regs before;
		unsigned long code;
		unsigned long data;
		u64 first = 0x1111111111111111ULL;
		u64 observed = 0;

		KUNIT_ASSERT_NOT_NULL(test, leaf);
		instruction = bls_test_instruction(leaf);
		decoded = orlix_tcti_decode_aarch64(instruction);
		data = orlix_tcti_memory_proof_map_guarded_span(test,
							       PROT_READ | PROT_WRITE);
		KUNIT_ASSERT_EQ(test, 0, orlix_tcti_write_user_data(current->mm,
			data + PAGE_SIZE - 8, &first, sizeof(first)));
		code = bls_map_program(test, instruction);
		regs.pc = code;
		regs.pstate = PSR_MODE_EL0t;
		regs.syscallno = NO_SYSCALL;
		regs.regs[0] = first;
		regs.regs[1] = data + PAGE_SIZE - 16;
		regs.regs[2] = 0x2222222222222222ULL;
		before = regs;
		result = orlix_tcti_resume_user(current, &regs, current->mm);
		orlix_tcti_memory_proof_expect_user_fault(test, &result, code,
							  regs.pc, leaf->name);
		KUNIT_EXPECT_EQ_MSG(test, before.regs[0], regs.regs[0],
				    "%s dest unchanged", leaf->name);
		KUNIT_EXPECT_EQ_MSG(test, before.regs[2], regs.regs[2],
				    "%s dest2 unchanged", leaf->name);
		KUNIT_EXPECT_EQ_MSG(test, before.regs[1], regs.regs[1],
				    "%s no writeback on pair fault", leaf->name);
		if (!decoded.load) {
			KUNIT_ASSERT_EQ(test, 0,
				orlix_tcti_read_user_data(current->mm,
					data + PAGE_SIZE - 8, &observed, 8));
			KUNIT_EXPECT_EQ_MSG(test, first, observed,
					    "%s first element committed",
					    leaf->name);
		}
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(code, PAGE_SIZE));
		KUNIT_EXPECT_EQ(test, 0, vm_munmap(data, 2 * PAGE_SIZE));
	}
}

static struct kunit_case bls_cases[] = {
	KUNIT_CASE(bls_source_decode),
	KUNIT_CASE(bls_reserved_is_rejected),
	KUNIT_CASE(bls_production_resume),
	KUNIT_CASE(bls_unmapped_load_faults),
	KUNIT_CASE(bls_write_protect_store_faults),
	KUNIT_CASE(bls_pair_second_element_faults),
	{}
};

static struct kunit_suite orlix_tcti_base_load_store_source_bound_test_suite = {
	.name = "orlix-tcti-base-load-store-source-bound",
	.init = bls_test_init,
	.exit = bls_test_exit,
	.test_cases = bls_cases,
};
kunit_test_suite(orlix_tcti_base_load_store_source_bound_test_suite);
