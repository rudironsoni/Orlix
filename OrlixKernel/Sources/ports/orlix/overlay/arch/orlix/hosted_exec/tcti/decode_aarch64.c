// SPDX-License-Identifier: GPL-2.0-only
#include <linux/bits.h>
#include <linux/bitops.h>

#include "decode_aarch64.h"

#define AARCH64_SVC_MASK 0xffe0001fU
#define AARCH64_SVC_PATTERN 0xd4000001U
#define AARCH64_HINT_MASK 0xfffff01fU
#define AARCH64_HINT_PATTERN 0xd503201fU
#define AARCH64_PC_RELATIVE_ADDRESS_MASK 0x1f000000U
#define AARCH64_PC_RELATIVE_ADDRESS_PATTERN 0x10000000U
#define AARCH64_ADD_SUB_IMM_MASK 0x1f000000U
#define AARCH64_ADD_SUB_IMM_PATTERN 0x11000000U
#define AARCH64_ADD_SUB_SHIFTED_REG_MASK 0x1f200000U
#define AARCH64_ADD_SUB_SHIFTED_REG_PATTERN 0x0b000000U
#define AARCH64_ADD_SUB_EXTENDED_REG_MASK 0x1f200000U
#define AARCH64_ADD_SUB_EXTENDED_REG_PATTERN 0x0b200000U
#define AARCH64_UNCONDITIONAL_BRANCH_IMM_MASK 0x7c000000U
#define AARCH64_UNCONDITIONAL_BRANCH_IMM_PATTERN 0x14000000U
#define AARCH64_BRANCH_REGISTER_MASK 0xfffffc1fU
#define AARCH64_BR_PATTERN 0xd61f0000U
#define AARCH64_BLR_PATTERN 0xd63f0000U
#define AARCH64_RET_PATTERN 0xd65f0000U
#define AARCH64_COMPARE_BRANCH_IMM_MASK 0x7e000000U
#define AARCH64_COMPARE_BRANCH_IMM_PATTERN 0x34000000U
#define AARCH64_TEST_BRANCH_IMM_MASK 0x7e000000U
#define AARCH64_TEST_BRANCH_IMM_PATTERN 0x36000000U
#define AARCH64_CONDITIONAL_BRANCH_IMM_MASK 0xff000010U
#define AARCH64_CONDITIONAL_BRANCH_IMM_PATTERN 0x54000000U
#define AARCH64_CONDITIONAL_COMPARE_MASK 0x1fe00000U
#define AARCH64_CONDITIONAL_COMPARE_PATTERN 0x1a400000U
#define AARCH64_CONDITIONAL_SELECT_MASK 0x1fe00000U
#define AARCH64_CONDITIONAL_SELECT_PATTERN 0x1a800000U
#define AARCH64_LOAD_STORE_PAIR_MASK 0x3a000000U
#define AARCH64_LOAD_STORE_PAIR_PATTERN 0x28000000U
#define AARCH64_LOAD_STORE_UNSIGNED_IMM_MASK 0x3b000000U
#define AARCH64_LOAD_STORE_UNSIGNED_IMM_PATTERN 0x39000000U
#define AARCH64_LOAD_STORE_SIGNED_IMM_MASK 0x3b200000U
#define AARCH64_LOAD_STORE_SIGNED_IMM_PATTERN 0x38000000U
#define AARCH64_LOAD_STORE_REGISTER_OFFSET_MASK 0x3b200c00U
#define AARCH64_LOAD_STORE_REGISTER_OFFSET_PATTERN 0x38200800U
#define AARCH64_LOGICAL_SHIFTED_REGISTER_MASK 0x1f000000U
#define AARCH64_LOGICAL_SHIFTED_REGISTER_PATTERN 0x0a000000U
#define AARCH64_LOGICAL_IMMEDIATE_MASK 0x1f800000U
#define AARCH64_LOGICAL_IMMEDIATE_PATTERN 0x12000000U
#define AARCH64_BITFIELD_MASK 0x1f800000U
#define AARCH64_BITFIELD_PATTERN 0x13000000U
#define AARCH64_EXTRACT_MASK 0x7f800000U
#define AARCH64_EXTRACT_PATTERN 0x13800000U
#define AARCH64_DATA_PROCESSING_1SOURCE_MASK 0x5fe00000U
#define AARCH64_DATA_PROCESSING_1SOURCE_PATTERN 0x5ac00000U
#define AARCH64_DP1_CLZ_OPCODE 0x04U
#define AARCH64_DATA_PROCESSING_2SOURCE_MASK 0x7fe00000U
#define AARCH64_DATA_PROCESSING_2SOURCE_PATTERN 0x1ac00000U
#define AARCH64_MULTIPLY_ADD_SUB_MASK 0x7f000000U
#define AARCH64_MULTIPLY_ADD_SUB_PATTERN 0x1b000000U
#define AARCH64_MOVE_WIDE_IMM_MASK 0x1f800000U
#define AARCH64_MOVE_WIDE_IMM_PATTERN 0x12800000U
#define AARCH64_CLREX 0xd5033f5fU
#define AARCH64_LOAD_STORE_EXCLUSIVE_MASK 0x3f007c00U
#define AARCH64_LOAD_STORE_EXCLUSIVE_PATTERN 0x08007c00U
#define AARCH64_SYSTEM_REGISTER_MASK 0xfff00000U
#define AARCH64_MRS_PATTERN 0xd5300000U
#define AARCH64_MSR_PATTERN 0xd5100000U
#define AARCH64_SYSREG_TPIDR_EL0 0xde82U
#define AARCH64_SYSREG_NZCV 0xda10U

static u32 tcti_bits(u32 value, u8 shift, u8 width)
{
	return (value >> shift) & ((1U << width) - 1U);
}

static bool tcti_decode_load_store_variant(u8 size, u8 opc, bool *load,
					   bool *sign_extend_load,
					   u8 *access_size, u8 *result_size)
{
	if (size > 3)
		return false;

	*access_size = 1U << size;
	*result_size = *access_size;
	*load = opc != 0;
	*sign_extend_load = false;

	switch (opc) {
	case 0:
	case 1:
		return true;
	case 2:
		if (size == 3)
			return false;
		*sign_extend_load = true;
		*result_size = sizeof(u32);
		return true;
	case 3:
		if (size >= 2)
			return false;
		*sign_extend_load = true;
		*result_size = sizeof(u64);
		return true;
	default:
		return false;
	}
}

static u64 tcti_ror_width(u64 value, u8 rotate, u8 width)
{
	u64 mask = width == 64 ? ~0ULL : (BIT_ULL(width) - 1);

	rotate %= width;
	value &= mask;
	if (!rotate)
		return value;

	return ((value >> rotate) | (value << (width - rotate))) & mask;
}

static bool tcti_decode_logical_immediate_mask(bool is_64bit, bool n, u8 immr,
					       u8 imms, u64 *out)
{
	u8 reg_width = is_64bit ? 64 : 32;
	u8 len_source = (n ? BIT(6) : 0) | ((~imms) & 0x3fU);
	u8 length;
	u8 levels;
	u8 size;
	u64 element;
	u64 result = 0;
	u8 offset;

	if (!len_source)
		return false;

	length = fls(len_source) - 1;
	if (!is_64bit && length == 6)
		return false;

	levels = BIT(length) - 1;
	if ((imms & levels) == levels)
		return false;

	size = BIT(length);
	element = BIT_ULL((imms & levels) + 1) - 1;
	element = tcti_ror_width(element, immr & levels, size);

	for (offset = 0; offset < reg_width; offset += size)
		result |= element << offset;

	*out = result;
	return true;
}

static void tcti_decode_memory_common(struct tcti_decoded_instruction *decoded,
				      u32 instruction, u8 size, u8 opc)
{
	decoded->rt = instruction & 0x1fU;
	decoded->rn = (instruction >> 5) & 0x1fU;
	tcti_decode_load_store_variant(size, opc, &decoded->load,
				       &decoded->sign_extend_load,
				       &decoded->access_size,
				       &decoded->result_size);
}

struct tcti_decoded_instruction tcti_decode_aarch64(u32 instruction)
{
	struct tcti_decoded_instruction decoded = {
		.decode_class = TCTI_DECODE_UNSUPPORTED,
		.instruction = instruction,
	};

	if ((instruction & AARCH64_SVC_MASK) == AARCH64_SVC_PATTERN) {
		decoded.decode_class = TCTI_DECODE_SVC;
		return decoded;
	}

	if ((instruction & AARCH64_HINT_MASK) == AARCH64_HINT_PATTERN) {
		decoded.decode_class = TCTI_DECODE_HINT;
		return decoded;
	}

	if ((instruction & AARCH64_PC_RELATIVE_ADDRESS_MASK) ==
	    AARCH64_PC_RELATIVE_ADDRESS_PATTERN) {
		u64 imm = (tcti_bits(instruction, 5, 19) << 2) |
			  tcti_bits(instruction, 29, 2);

		decoded.decode_class = TCTI_DECODE_PC_RELATIVE_ADDRESS;
		decoded.rd = instruction & 0x1fU;
		decoded.page_relative = instruction & BIT(31);
		decoded.pc_relative_imm = sign_extend64(imm, 20);
		if (decoded.page_relative)
			decoded.pc_relative_imm <<= 12;
		return decoded;
	}

	if ((instruction & AARCH64_ADD_SUB_IMM_MASK) ==
	    AARCH64_ADD_SUB_IMM_PATTERN) {
		u8 shift = (instruction >> 22) & 0x3U;

		if (shift > 1)
			return decoded;

		decoded.decode_class = TCTI_DECODE_ADD_SUB_IMMEDIATE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.imm12 = (instruction >> 10) & 0xfffU;
		decoded.shift = shift;
		decoded.is_64bit = instruction & BIT(31);
		decoded.subtract = instruction & BIT(30);
		decoded.set_flags = instruction & BIT(29);
		return decoded;
	}

	if ((instruction & AARCH64_ADD_SUB_SHIFTED_REG_MASK) ==
	    AARCH64_ADD_SUB_SHIFTED_REG_PATTERN) {
		u8 shift = (instruction >> 22) & 0x3U;
		u8 amount = (instruction >> 10) & 0x3fU;
		bool is_64bit = instruction & BIT(31);

		if (shift == 3 || (!is_64bit && (amount & BIT(5))))
			return decoded;

		decoded.decode_class = TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.shift = shift;
		decoded.shift_amount = amount;
		decoded.is_64bit = is_64bit;
		decoded.subtract = instruction & BIT(30);
		decoded.set_flags = instruction & BIT(29);
		return decoded;
	}

	if ((instruction & AARCH64_ADD_SUB_EXTENDED_REG_MASK) ==
	    AARCH64_ADD_SUB_EXTENDED_REG_PATTERN) {
		u8 amount = (instruction >> 10) & 0x7U;

		if (amount > 4)
			return decoded;

		decoded.decode_class = TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.shift_amount = amount;
		decoded.offset_extend = (instruction >> 13) & 0x7U;
		decoded.is_64bit = instruction & BIT(31);
		decoded.subtract = instruction & BIT(30);
		decoded.set_flags = instruction & BIT(29);
		return decoded;
	}

	if ((instruction & AARCH64_UNCONDITIONAL_BRANCH_IMM_MASK) ==
	    AARCH64_UNCONDITIONAL_BRANCH_IMM_PATTERN) {
		decoded.decode_class = TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE;
		decoded.branch_imm =
			sign_extend64(instruction & 0x03ffffffU, 25) << 2;
		decoded.link = instruction & BIT(31);
		return decoded;
	}

	if ((instruction & AARCH64_BRANCH_REGISTER_MASK) == AARCH64_BR_PATTERN ||
	    (instruction & AARCH64_BRANCH_REGISTER_MASK) == AARCH64_BLR_PATTERN ||
	    (instruction & AARCH64_BRANCH_REGISTER_MASK) == AARCH64_RET_PATTERN) {
		u32 pattern = instruction & AARCH64_BRANCH_REGISTER_MASK;

		decoded.decode_class = TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.link = pattern == AARCH64_BLR_PATTERN;
		decoded.branch_register_op =
			pattern == AARCH64_BR_PATTERN ? TCTI_BRANCH_REGISTER_BR :
			pattern == AARCH64_BLR_PATTERN ? TCTI_BRANCH_REGISTER_BLR :
							  TCTI_BRANCH_REGISTER_RET;
		return decoded;
	}

	if ((instruction & AARCH64_COMPARE_BRANCH_IMM_MASK) ==
	    AARCH64_COMPARE_BRANCH_IMM_PATTERN) {
		u64 imm = ((instruction >> 5) & 0x7ffffU);

		decoded.decode_class = TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE;
		decoded.rt = instruction & 0x1fU;
		decoded.is_64bit = instruction & BIT(31);
		decoded.nonzero = instruction & BIT(24);
		decoded.branch_imm = sign_extend64(imm, 18) << 2;
		return decoded;
	}

	if ((instruction & AARCH64_TEST_BRANCH_IMM_MASK) ==
	    AARCH64_TEST_BRANCH_IMM_PATTERN) {
		u8 bit_5 = instruction & BIT(31) ? BIT(5) : 0;
		u64 imm = (instruction >> 5) & 0x3fffU;

		decoded.decode_class = TCTI_DECODE_TEST_BRANCH_IMMEDIATE;
		decoded.rt = instruction & 0x1fU;
		decoded.nonzero = instruction & BIT(24);
		decoded.test_bit = bit_5 | ((instruction >> 19) & 0x1fU);
		decoded.branch_imm = sign_extend64(imm, 13) << 2;
		return decoded;
	}

	if ((instruction & AARCH64_CONDITIONAL_BRANCH_IMM_MASK) ==
	    AARCH64_CONDITIONAL_BRANCH_IMM_PATTERN) {
		u64 imm = (instruction >> 5) & 0x7ffffU;

		decoded.decode_class = TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE;
		decoded.condition = instruction & 0xfU;
		decoded.branch_imm = sign_extend64(imm, 18) << 2;
		return decoded;
	}

	if ((instruction & AARCH64_CONDITIONAL_COMPARE_MASK) ==
	    AARCH64_CONDITIONAL_COMPARE_PATTERN) {
		decoded.decode_class = TCTI_DECODE_CONDITIONAL_COMPARE;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.imm12 = (instruction >> 16) & 0x1fU;
		decoded.condition = (instruction >> 12) & 0xfU;
		decoded.nzcv = instruction & 0xfU;
		decoded.is_64bit = instruction & BIT(31);
		decoded.subtract = instruction & BIT(30);
		decoded.immediate = instruction & BIT(11);
		return decoded;
	}

	if ((instruction & AARCH64_CONDITIONAL_SELECT_MASK) ==
	    AARCH64_CONDITIONAL_SELECT_PATTERN) {
		bool op = instruction & BIT(30);
		bool op2 = instruction & BIT(10);

		decoded.decode_class = TCTI_DECODE_CONDITIONAL_SELECT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.condition = (instruction >> 12) & 0xfU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.is_64bit = instruction & BIT(31);
		decoded.conditional_select_op =
			!op && !op2 ? TCTI_CONDITIONAL_SELECT_CSEL :
			!op && op2 ? TCTI_CONDITIONAL_SELECT_CSINC :
			op && !op2 ? TCTI_CONDITIONAL_SELECT_CSINV :
				     TCTI_CONDITIONAL_SELECT_CSNEG;
		return decoded;
	}

	if ((instruction & AARCH64_LOAD_STORE_PAIR_MASK) ==
	    AARCH64_LOAD_STORE_PAIR_PATTERN) {
		u8 opc = (instruction >> 30) & 0x3U;
		u8 mode = (instruction >> 23) & 0x3U;
		u8 scale;

		if (mode == 0 || (opc != 0 && opc != 2))
			return decoded;

		scale = opc == 2 ? 3 : 2;
		decoded.decode_class = TCTI_DECODE_LOAD_STORE_PAIR;
		decoded.rt = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rt2 = (instruction >> 10) & 0x1fU;
		decoded.load = instruction & BIT(22);
		decoded.access_size = BIT(scale);
		decoded.result_size = decoded.access_size;
		decoded.memory_offset =
			sign_extend64((instruction >> 15) & 0x7fU, 6) << scale;
		decoded.memory_index_mode =
			mode == 1 ? TCTI_MEMORY_INDEX_POST :
			mode == 2 ? TCTI_MEMORY_INDEX_SIGNED_OFFSET :
				    TCTI_MEMORY_INDEX_PRE;
		return decoded;
	}

	if ((instruction & AARCH64_LOAD_STORE_UNSIGNED_IMM_MASK) ==
	    AARCH64_LOAD_STORE_UNSIGNED_IMM_PATTERN) {
		u8 size = (instruction >> 30) & 0x3U;
		u8 opc = (instruction >> 22) & 0x3U;

		if (!tcti_decode_load_store_variant(size, opc, &decoded.load,
						    &decoded.sign_extend_load,
						    &decoded.access_size,
						    &decoded.result_size))
			return decoded;

		decoded.decode_class = TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE;
		decoded.rt = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.memory_offset =
			((instruction >> 10) & 0xfffU) << size;
		decoded.memory_index_mode = TCTI_MEMORY_INDEX_SIGNED_OFFSET;
		return decoded;
	}

	if ((instruction & AARCH64_LOAD_STORE_SIGNED_IMM_MASK) ==
	    AARCH64_LOAD_STORE_SIGNED_IMM_PATTERN) {
		u8 size = (instruction >> 30) & 0x3U;
		u8 opc = (instruction >> 22) & 0x3U;
		u8 mode = (instruction >> 10) & 0x3U;

		if (mode == 2 ||
		    !tcti_decode_load_store_variant(size, opc, &decoded.load,
						   &decoded.sign_extend_load,
						   &decoded.access_size,
						   &decoded.result_size))
			return decoded;

		decoded.decode_class = TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE;
		decoded.rt = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.memory_offset =
			sign_extend64((instruction >> 12) & 0x1ffU, 8);
		decoded.memory_index_mode =
			mode == 0 ? TCTI_MEMORY_INDEX_SIGNED_OFFSET :
			mode == 1 ? TCTI_MEMORY_INDEX_POST :
				    TCTI_MEMORY_INDEX_PRE;
		return decoded;
	}

	if ((instruction & AARCH64_LOAD_STORE_REGISTER_OFFSET_MASK) ==
	    AARCH64_LOAD_STORE_REGISTER_OFFSET_PATTERN) {
		u8 size = (instruction >> 30) & 0x3U;
		u8 opc = (instruction >> 22) & 0x3U;
		u8 option = (instruction >> 13) & 0x7U;

		if ((option != 2 && option != 3 && option != 6 && option != 7) ||
		    !tcti_decode_load_store_variant(size, opc, &decoded.load,
						   &decoded.sign_extend_load,
						   &decoded.access_size,
						   &decoded.result_size))
			return decoded;

		decoded.decode_class = TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET;
		decoded.rt = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.offset_extend = option;
		decoded.offset_shift = instruction & BIT(12);
		return decoded;
	}

	if ((instruction & AARCH64_LOGICAL_SHIFTED_REGISTER_MASK) ==
	    AARCH64_LOGICAL_SHIFTED_REGISTER_PATTERN) {
		u8 opc = (instruction >> 29) & 0x3U;
		u8 amount = (instruction >> 10) & 0x3fU;
		bool is_64bit = instruction & BIT(31);

		if (!is_64bit && (amount & BIT(5)))
			return decoded;

		decoded.decode_class = TCTI_DECODE_LOGICAL_SHIFTED_REGISTER;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.shift = (instruction >> 22) & 0x3U;
		decoded.shift_amount = amount;
		decoded.is_64bit = is_64bit;
		decoded.invert_second_operand = instruction & BIT(21);
		decoded.set_flags = opc == 3;
		decoded.logical_op =
			opc == 0 ? TCTI_LOGICAL_AND :
			opc == 1 ? TCTI_LOGICAL_ORR :
			opc == 2 ? TCTI_LOGICAL_EOR : TCTI_LOGICAL_AND;
		return decoded;
	}

	if ((instruction & AARCH64_LOGICAL_IMMEDIATE_MASK) ==
	    AARCH64_LOGICAL_IMMEDIATE_PATTERN) {
		u8 opc = (instruction >> 29) & 0x3U;
		bool n = instruction & BIT(22);
		u8 immr = (instruction >> 16) & 0x3fU;
		u8 imms = (instruction >> 10) & 0x3fU;

		if (!tcti_decode_logical_immediate_mask(instruction & BIT(31),
						       n, immr, imms,
						       &decoded.logical_immediate))
			return decoded;

		decoded.decode_class = TCTI_DECODE_LOGICAL_IMMEDIATE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.is_64bit = instruction & BIT(31);
		decoded.set_flags = opc == 3;
		decoded.logical_op =
			opc == 0 ? TCTI_LOGICAL_AND :
			opc == 1 ? TCTI_LOGICAL_ORR :
			opc == 2 ? TCTI_LOGICAL_EOR : TCTI_LOGICAL_AND;
		return decoded;
	}

	if ((instruction & AARCH64_BITFIELD_MASK) == AARCH64_BITFIELD_PATTERN) {
		u8 opc = (instruction >> 29) & 0x3U;
		bool sf = instruction & BIT(31);
		bool n = instruction & BIT(22);
		u8 immr = (instruction >> 16) & 0x3fU;
		u8 imms = (instruction >> 10) & 0x3fU;

		if (opc == 3)
			return decoded;
		if (!sf && (n || (immr & BIT(5)) || (imms & BIT(5))))
			return decoded;
		if (sf && !n)
			return decoded;

		decoded.decode_class = TCTI_DECODE_BITFIELD;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.is_64bit = sf;
		decoded.bitfield_immr = immr;
		decoded.bitfield_imms = imms;
		decoded.bitfield_op =
			opc == 0 ? TCTI_BITFIELD_SBFM :
			opc == 1 ? TCTI_BITFIELD_BFM : TCTI_BITFIELD_UBFM;
		return decoded;
	}

	if ((instruction & AARCH64_EXTRACT_MASK) == AARCH64_EXTRACT_PATTERN) {
		bool sf = instruction & BIT(31);
		bool n = instruction & BIT(22);
		u8 lsb = (instruction >> 10) & 0x3fU;

		if (n != sf)
			return decoded;
		if (!sf && (lsb & BIT(5)))
			return decoded;

		decoded.decode_class = TCTI_DECODE_EXTRACT;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.shift_amount = lsb;
		decoded.is_64bit = sf;
		return decoded;
	}

	if ((instruction & AARCH64_DATA_PROCESSING_1SOURCE_MASK) ==
	    AARCH64_DATA_PROCESSING_1SOURCE_PATTERN) {
		u8 opcode = (instruction >> 10) & 0x3fU;

		if (opcode != AARCH64_DP1_CLZ_OPCODE)
			return decoded;

		decoded.decode_class = TCTI_DECODE_DATA_PROCESSING_1SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.is_64bit = instruction & BIT(31);
		decoded.dp1_op = TCTI_DP1_CLZ;
		return decoded;
	}

	if ((instruction & AARCH64_DATA_PROCESSING_2SOURCE_MASK) ==
	    AARCH64_DATA_PROCESSING_2SOURCE_PATTERN) {
		u8 opcode = (instruction >> 10) & 0x3fU;

		switch (opcode) {
		case 0x02:
			decoded.dp2_op = TCTI_DP2_UDIV;
			break;
		case 0x03:
			decoded.dp2_op = TCTI_DP2_SDIV;
			break;
		case 0x08:
			decoded.dp2_op = TCTI_DP2_LSLV;
			break;
		case 0x09:
			decoded.dp2_op = TCTI_DP2_LSRV;
			break;
		case 0x0a:
			decoded.dp2_op = TCTI_DP2_ASRV;
			break;
		case 0x0b:
			decoded.dp2_op = TCTI_DP2_RORV;
			break;
		default:
			return decoded;
		}

		decoded.decode_class = TCTI_DECODE_DATA_PROCESSING_2SOURCE;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.is_64bit = instruction & BIT(31);
		return decoded;
	}

	if ((instruction & AARCH64_MULTIPLY_ADD_SUB_MASK) ==
	    AARCH64_MULTIPLY_ADD_SUB_PATTERN) {
		u8 family = (instruction >> 21) & 0x7U;
		bool subtract = instruction & BIT(15);
		bool is_64bit = instruction & BIT(31);
		u8 ra = (instruction >> 10) & 0x1fU;

		switch (family) {
		case 0:
			decoded.mul_op = subtract ? TCTI_MUL_MSUB :
						   TCTI_MUL_MADD;
			break;
		case 1:
			if (!is_64bit)
				return decoded;
			decoded.mul_op = subtract ? TCTI_MUL_SMSUBL :
						   TCTI_MUL_SMADDL;
			break;
		case 2:
			if (!is_64bit || subtract || ra != 31)
				return decoded;
			decoded.mul_op = TCTI_MUL_SMULH;
			break;
		case 5:
			if (!is_64bit)
				return decoded;
			decoded.mul_op = subtract ? TCTI_MUL_UMSUBL :
						   TCTI_MUL_UMADDL;
			break;
		case 6:
			if (!is_64bit || subtract || ra != 31)
				return decoded;
			decoded.mul_op = TCTI_MUL_UMULH;
			break;
		default:
			return decoded;
		}

		decoded.decode_class = TCTI_DECODE_MULTIPLY_ADD_SUB;
		decoded.rd = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.ra = ra;
		decoded.rm = (instruction >> 16) & 0x1fU;
		decoded.is_64bit = is_64bit;
		return decoded;
	}

	if ((instruction & AARCH64_MOVE_WIDE_IMM_MASK) ==
	    AARCH64_MOVE_WIDE_IMM_PATTERN) {
		u8 opc = (instruction >> 29) & 0x3U;
		u8 hw = (instruction >> 21) & 0x3U;
		bool is_64bit = instruction & BIT(31);

		if (opc == 1)
			return decoded;
		if (!is_64bit && (hw & BIT(1)))
			return decoded;

		decoded.decode_class = TCTI_DECODE_MOVE_WIDE_IMMEDIATE;
		decoded.rd = instruction & 0x1fU;
		decoded.imm16 = (instruction >> 5) & 0xffffU;
		decoded.halfword_shift = hw * 16;
		decoded.is_64bit = is_64bit;
		decoded.move_wide_op =
			opc == 0 ? TCTI_MOVE_WIDE_MOVN :
			opc == 2 ? TCTI_MOVE_WIDE_MOVZ : TCTI_MOVE_WIDE_MOVK;
		return decoded;
	}

	if (instruction == AARCH64_CLREX) {
		decoded.decode_class = TCTI_DECODE_EXCLUSIVE_MONITOR_CLEAR;
		return decoded;
	}

	if ((instruction & AARCH64_LOAD_STORE_EXCLUSIVE_MASK) ==
	    AARCH64_LOAD_STORE_EXCLUSIVE_PATTERN) {
		u8 size = (instruction >> 30) & 0x3U;
		bool load = instruction & BIT(22);
		bool ordered_nonexclusive = instruction & BIT(23);
		u8 rs = (instruction >> 16) & 0x1fU;

		if (!load && ordered_nonexclusive && rs != 31)
			return decoded;
		if (load && rs != 31)
			return decoded;

		decoded.decode_class = TCTI_DECODE_LOAD_STORE_EXCLUSIVE;
		decoded.rt = instruction & 0x1fU;
		decoded.rn = (instruction >> 5) & 0x1fU;
		decoded.rs = rs;
		decoded.access_size = BIT(size);
		decoded.result_size = decoded.access_size;
		decoded.load = load;
		decoded.exclusive = !ordered_nonexclusive;
		decoded.acquire = load && (instruction & BIT(15));
		decoded.release = !load && (instruction & BIT(15));
		return decoded;
	}

	if ((instruction & AARCH64_SYSTEM_REGISTER_MASK) == AARCH64_MRS_PATTERN ||
	    (instruction & AARCH64_SYSTEM_REGISTER_MASK) == AARCH64_MSR_PATTERN) {
		u16 sysreg = (instruction >> 5) & 0xffffU;

		if (sysreg != AARCH64_SYSREG_TPIDR_EL0 &&
		    sysreg != AARCH64_SYSREG_NZCV)
			return decoded;

		decoded.decode_class = TCTI_DECODE_SYSTEM_REGISTER;
		decoded.rt = instruction & 0x1fU;
		decoded.system_register =
			sysreg == AARCH64_SYSREG_TPIDR_EL0 ?
				TCTI_SYSTEM_REGISTER_TPIDR_EL0 :
				TCTI_SYSTEM_REGISTER_NZCV;
		decoded.system_register_write =
			(instruction & AARCH64_SYSTEM_REGISTER_MASK) ==
			AARCH64_MSR_PATTERN;
		return decoded;
	}

	return decoded;
}
