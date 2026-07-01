/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef ORLIX_TCTI_DECODE_AARCH64_H
#define ORLIX_TCTI_DECODE_AARCH64_H

#include <linux/types.h>

enum tcti_decode_class {
	TCTI_DECODE_UNSUPPORTED = 0,
	TCTI_DECODE_SVC,
	TCTI_DECODE_HINT,
	TCTI_DECODE_ADD_SUB_IMMEDIATE,
	TCTI_DECODE_ADD_SUB_SHIFTED_REGISTER,
	TCTI_DECODE_ADD_SUB_EXTENDED_REGISTER,
	TCTI_DECODE_PC_RELATIVE_ADDRESS,
	TCTI_DECODE_UNCONDITIONAL_BRANCH_IMMEDIATE,
	TCTI_DECODE_UNCONDITIONAL_BRANCH_REGISTER,
	TCTI_DECODE_COMPARE_BRANCH_IMMEDIATE,
	TCTI_DECODE_TEST_BRANCH_IMMEDIATE,
	TCTI_DECODE_CONDITIONAL_BRANCH_IMMEDIATE,
	TCTI_DECODE_CONDITIONAL_COMPARE,
	TCTI_DECODE_CONDITIONAL_SELECT,
	TCTI_DECODE_LOAD_STORE_PAIR,
	TCTI_DECODE_LOAD_STORE_UNSIGNED_IMMEDIATE,
	TCTI_DECODE_LOAD_STORE_SIGNED_IMMEDIATE,
	TCTI_DECODE_LOAD_STORE_REGISTER_OFFSET,
	TCTI_DECODE_LOGICAL_SHIFTED_REGISTER,
	TCTI_DECODE_LOGICAL_IMMEDIATE,
	TCTI_DECODE_BITFIELD,
	TCTI_DECODE_EXTRACT,
	TCTI_DECODE_DATA_PROCESSING_1SOURCE,
	TCTI_DECODE_DATA_PROCESSING_2SOURCE,
	TCTI_DECODE_MULTIPLY_ADD_SUB,
	TCTI_DECODE_MOVE_WIDE_IMMEDIATE,
	TCTI_DECODE_SYSTEM_REGISTER,
	TCTI_DECODE_EXCLUSIVE_MONITOR_CLEAR,
	TCTI_DECODE_LOAD_STORE_EXCLUSIVE,
};

enum tcti_memory_index_mode {
	TCTI_MEMORY_INDEX_SIGNED_OFFSET = 0,
	TCTI_MEMORY_INDEX_PRE,
	TCTI_MEMORY_INDEX_POST,
};

enum tcti_logical_op {
	TCTI_LOGICAL_AND = 0,
	TCTI_LOGICAL_ORR,
	TCTI_LOGICAL_EOR,
};

enum tcti_move_wide_op {
	TCTI_MOVE_WIDE_MOVN = 0,
	TCTI_MOVE_WIDE_MOVZ,
	TCTI_MOVE_WIDE_MOVK,
};

enum tcti_branch_register_op {
	TCTI_BRANCH_REGISTER_BR = 0,
	TCTI_BRANCH_REGISTER_BLR,
	TCTI_BRANCH_REGISTER_RET,
};

enum tcti_system_register {
	TCTI_SYSTEM_REGISTER_TPIDR_EL0 = 0,
	TCTI_SYSTEM_REGISTER_NZCV,
};

enum tcti_conditional_select_op {
	TCTI_CONDITIONAL_SELECT_CSEL = 0,
	TCTI_CONDITIONAL_SELECT_CSINC,
	TCTI_CONDITIONAL_SELECT_CSINV,
	TCTI_CONDITIONAL_SELECT_CSNEG,
};

enum tcti_bitfield_op {
	TCTI_BITFIELD_SBFM = 0,
	TCTI_BITFIELD_BFM,
	TCTI_BITFIELD_UBFM,
};

enum tcti_data_processing_2source_op {
	TCTI_DP2_UDIV = 0,
	TCTI_DP2_SDIV,
	TCTI_DP2_LSLV,
	TCTI_DP2_LSRV,
	TCTI_DP2_ASRV,
	TCTI_DP2_RORV,
};

enum tcti_data_processing_1source_op {
	TCTI_DP1_CLZ = 0,
};

enum tcti_multiply_add_sub_op {
	TCTI_MUL_MADD = 0,
	TCTI_MUL_MSUB,
	TCTI_MUL_SMADDL,
	TCTI_MUL_SMSUBL,
	TCTI_MUL_SMULH,
	TCTI_MUL_UMADDL,
	TCTI_MUL_UMSUBL,
	TCTI_MUL_UMULH,
};

struct tcti_decoded_instruction {
	enum tcti_decode_class decode_class;
	u32 instruction;
	u8 rd;
	u8 rn;
	u8 rm;
	u8 ra;
	u8 rt;
	u8 rt2;
	u16 imm12;
	u16 imm16;
	u8 shift;
	u8 shift_amount;
	u8 halfword_shift;
	u8 access_size;
	u8 result_size;
	bool is_64bit;
	bool subtract;
	bool set_flags;
	bool immediate;
	s64 pc_relative_imm;
	bool page_relative;
	s64 branch_imm;
	bool link;
	u8 condition;
	u8 nzcv;
	bool nonzero;
	u8 test_bit;
	s64 memory_offset;
	bool load;
	bool sign_extend_load;
	u8 offset_extend;
	bool offset_shift;
	bool invert_second_operand;
	u64 logical_immediate;
	enum tcti_memory_index_mode memory_index_mode;
	enum tcti_logical_op logical_op;
	enum tcti_move_wide_op move_wide_op;
	enum tcti_branch_register_op branch_register_op;
	enum tcti_system_register system_register;
	enum tcti_conditional_select_op conditional_select_op;
	enum tcti_bitfield_op bitfield_op;
	enum tcti_data_processing_1source_op dp1_op;
	enum tcti_data_processing_2source_op dp2_op;
	enum tcti_multiply_add_sub_op mul_op;
	bool system_register_write;
	u8 bitfield_immr;
	u8 bitfield_imms;
	u8 rs;
	bool acquire;
	bool release;
	bool exclusive;
};

struct tcti_decoded_instruction tcti_decode_aarch64(u32 instruction);

#endif /* ORLIX_TCTI_DECODE_AARCH64_H */
