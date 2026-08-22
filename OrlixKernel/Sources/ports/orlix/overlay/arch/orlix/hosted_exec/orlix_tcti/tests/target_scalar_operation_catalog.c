/* SPDX-License-Identifier: GPL-2.0-only */
#include "target_scalar_operation_catalog.h"

#include <stdbool.h>
#include <string.h>

#define BASE_OBLIGATIONS \
	(ORLIX_TCTI_SCALAR_OBLIGATION_DECODE | \
	 ORLIX_TCTI_SCALAR_OBLIGATION_LEGAL_ENCODINGS | \
	 ORLIX_TCTI_SCALAR_OBLIGATION_REJECTED_ENCODINGS)
#define REG_PC (BASE_OBLIGATIONS | ORLIX_TCTI_SCALAR_OBLIGATION_REGISTERS | \
		ORLIX_TCTI_SCALAR_OBLIGATION_PC)
#define REG_PC_FLAGS (REG_PC | ORLIX_TCTI_SCALAR_OBLIGATION_FLAGS)
#define CONTROL_PC (REG_PC | ORLIX_TCTI_SCALAR_OBLIGATION_FLAGS)
#define EXCEPTION_EXIT \
	(BASE_OBLIGATIONS | ORLIX_TCTI_SCALAR_OBLIGATION_REGISTERS | \
	 ORLIX_TCTI_SCALAR_OBLIGATION_PC | ORLIX_TCTI_SCALAR_OBLIGATION_FAULTS | \
	 ORLIX_TCTI_SCALAR_OBLIGATION_STRUCTURED_EXIT)
#define BARRIER \
	(BASE_OBLIGATIONS | ORLIX_TCTI_SCALAR_OBLIGATION_PC | \
	 ORLIX_TCTI_SCALAR_OBLIGATION_ORDERING)
#define SYSTEM_STATE \
	(REG_PC | ORLIX_TCTI_SCALAR_OBLIGATION_SYSTEM_STATE)

#define DECODE_SOURCE \
	"OrlixKernel/Sources/ports/orlix/overlay/arch/orlix/hosted_exec/orlix_tcti/tests/orlix_tcti_decode_test.c"
#define DECODE_SOURCE_SHA256 \
	"52032e4b8b44f8116064fb96ce7fa3f6f512301e536017a7c9f06e56915fd9e6"
#define DECODE_OBJECT "orlix_tcti_decode_test.o"
#define DECODE_SUITE "orlix-tcti-decode"
#define DECODE_SUITE_SYMBOL "orlix_tcti_decode_test_suite"
#define DECODE_CASE_ARRAY "orlix_tcti_decode_test_cases"

struct source_row {
	const char *leaf_name;
	const char *mnemonic;
	const char *operation_id;
	uint32_t encoding_mask;
	uint32_t encoding_pattern;
	const char *condition_tcnd_hex;
};

#define ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE(...)
#define ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW(index, leaf, mnemonic, operation, mask, \
					  pattern, condition, ...) \
	{ leaf, mnemonic, operation, mask, pattern, condition },
static const struct source_row source_rows[] = {
#include "../isa/source_manifest.def"
};
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_ROW
#undef ORLIX_TCTI_A64_SOURCE_MANIFEST_SOURCE

struct classification_row {
	const char *leaf_name;
	bool required;
};

#define ORLIX_TCTI_SCALAR_CLASS_REQUIRED true
#define ORLIX_TCTI_SCALAR_CLASS_UNCLASSIFIED false
#define ORLIX_TCTI_SCALAR_CLASS_NON_EL0 false
#define ORLIX_TCTI_SCALAR_CLASS_ARCHITECTURALLY_UNDEFINED false
#define ORLIX_TCTI_SCALAR_STRINGIFY_INNER(value) #value
#define ORLIX_TCTI_SCALAR_STRINGIFY(value) ORLIX_TCTI_SCALAR_STRINGIFY_INNER(value)
#define ORLIX_TCTI_A64_TARGET_CLASSIFICATION(leaf, classification, relation, \
				       related_leaf, evidence, proof) \
	{ ORLIX_TCTI_SCALAR_STRINGIFY(leaf), \
	  ORLIX_TCTI_SCALAR_CLASS_##classification },
static const struct classification_row classification_rows[] = {
#include "../isa/target_classification.def"
};
#undef ORLIX_TCTI_A64_TARGET_CLASSIFICATION
#undef ORLIX_TCTI_SCALAR_STRINGIFY
#undef ORLIX_TCTI_SCALAR_STRINGIFY_INNER
#undef ORLIX_TCTI_SCALAR_CLASS_ARCHITECTURALLY_UNDEFINED
#undef ORLIX_TCTI_SCALAR_CLASS_NON_EL0
#undef ORLIX_TCTI_SCALAR_CLASS_UNCLASSIFIED
#undef ORLIX_TCTI_SCALAR_CLASS_REQUIRED

_Static_assert(sizeof(source_rows) / sizeof(source_rows[0]) ==
		       sizeof(classification_rows) /
			       sizeof(classification_rows[0]),
	       "source and classification row counts must match");

struct operation_spec {
	const char *operation_id;
	size_t expected_leaves;
	uint32_t obligations;
	const char *kunit_case;
};

#define SPEC(operation, leaves, duties, test_case) \
	{ operation, leaves, duties, test_case }

/*
 * Exact Arm operation IDs are deliberate. Prefixes, mnemonic families and
 * wildcard bindings are not accepted because they can silently absorb a new
 * source leaf with different semantics.
 */
static const struct operation_spec operation_specs[] = {
	SPEC("ADR", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_pc_relative_address_family"),
	SPEC("ADRP", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_pc_relative_address_family"),
	SPEC("ADD_addsub_imm", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_add_sub_immediate_family"),
	SPEC("ADDS_addsub_imm", 2, REG_PC_FLAGS,
	     "orlix_tcti_gadget_executes_complete_add_sub_immediate_family"),
	SPEC("SUB_addsub_imm", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_add_sub_immediate_family"),
	SPEC("SUBS_addsub_imm", 2, REG_PC_FLAGS,
	     "orlix_tcti_gadget_executes_complete_add_sub_immediate_family"),
	SPEC("ADD_addsub_shift", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_add_sub_shifted_register_family"),
	SPEC("ADDS_addsub_shift", 2, REG_PC_FLAGS,
	     "orlix_tcti_gadget_executes_complete_add_sub_shifted_register_family"),
	SPEC("SUB_addsub_shift", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_add_sub_shifted_register_family"),
	SPEC("SUBS_addsub_shift", 2, REG_PC_FLAGS,
	     "orlix_tcti_gadget_executes_complete_add_sub_shifted_register_family"),
	SPEC("ADD_addsub_ext", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_add_sub_extended_register_family"),
	SPEC("ADDS_addsub_ext", 2, REG_PC_FLAGS,
	     "orlix_tcti_gadget_executes_complete_add_sub_extended_register_family"),
	SPEC("SUB_addsub_ext", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_add_sub_extended_register_family"),
	SPEC("SUBS_addsub_ext", 2, REG_PC_FLAGS,
	     "orlix_tcti_gadget_executes_complete_add_sub_extended_register_family"),
	SPEC("ADC", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_add_sub_with_carry_family"),
	SPEC("ADCS", 2, REG_PC_FLAGS,
	     "orlix_tcti_gadget_executes_complete_add_sub_with_carry_family"),
	SPEC("SBC", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_add_sub_with_carry_family"),
	SPEC("SBCS", 2, REG_PC_FLAGS,
	     "orlix_tcti_gadget_executes_complete_add_sub_with_carry_family"),
	SPEC("AND_log_imm", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_logical_immediate_family"),
	SPEC("ORR_log_imm", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_logical_immediate_family"),
	SPEC("EOR_log_imm", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_logical_immediate_family"),
	SPEC("ANDS_log_imm", 2, REG_PC_FLAGS,
	     "orlix_tcti_gadget_executes_complete_logical_immediate_family"),
	SPEC("AND_log_shift", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_logical_shifted_register_family"),
	SPEC("BIC_log_shift", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_logical_shifted_register_family"),
	SPEC("ORR_log_shift", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_logical_shifted_register_family"),
	SPEC("ORN_log_shift", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_logical_shifted_register_family"),
	SPEC("EOR_log_shift", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_logical_shifted_register_family"),
	SPEC("EON", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_logical_shifted_register_family"),
	SPEC("ANDS_log_shift", 2, REG_PC_FLAGS,
	     "orlix_tcti_gadget_executes_complete_logical_shifted_register_family"),
	SPEC("BICS", 2, REG_PC_FLAGS,
	     "orlix_tcti_gadget_executes_complete_logical_shifted_register_family"),
	SPEC("MOVN", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_move_wide_immediate_family"),
	SPEC("MOVZ", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_move_wide_immediate_family"),
	SPEC("MOVK", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_move_wide_immediate_family"),
	SPEC("SBFM", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_bitfield_family"),
	SPEC("BFM", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_bitfield_family"),
	SPEC("UBFM", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_bitfield_family"),
	SPEC("EXTR", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_extract_family"),
	SPEC("RBIT_int", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_1source_family"),
	SPEC("REV16_int", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_1source_family"),
	SPEC("REV", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_1source_family"),
	SPEC("REV32_int", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_1source_family"),
	SPEC("CLZ_int", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_1source_family"),
	SPEC("CLS_int", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_1source_family"),
	SPEC("UDIV", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_2source_family"),
	SPEC("SDIV", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_2source_family"),
	SPEC("LSLV", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_2source_family"),
	SPEC("LSRV", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_2source_family"),
	SPEC("ASRV", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_2source_family"),
	SPEC("RORV", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_2source_family"),
	SPEC("CRC32", 4, REG_PC,
	     "orlix_tcti_gadget_executes_complete_crc32_family"),
	SPEC("CRC32C", 4, REG_PC,
	     "orlix_tcti_gadget_executes_complete_crc32_family"),
	SPEC("MADD", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_3source_family"),
	SPEC("MSUB", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_3source_family"),
	SPEC("SMADDL", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_3source_family"),
	SPEC("SMSUBL", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_3source_family"),
	SPEC("SMULH", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_3source_family"),
	SPEC("UMADDL", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_3source_family"),
	SPEC("UMSUBL", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_3source_family"),
	SPEC("UMULH", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_data_processing_3source_family"),
	SPEC("CCMN_reg", 2, REG_PC_FLAGS,
	     "orlix_tcti_gadget_executes_complete_conditional_compare_family"),
	SPEC("CCMP_reg", 2, REG_PC_FLAGS,
	     "orlix_tcti_gadget_executes_complete_conditional_compare_family"),
	SPEC("CCMN_imm", 2, REG_PC_FLAGS,
	     "orlix_tcti_gadget_executes_complete_conditional_compare_family"),
	SPEC("CCMP_imm", 2, REG_PC_FLAGS,
	     "orlix_tcti_gadget_executes_complete_conditional_compare_family"),
	SPEC("CSEL", 2, CONTROL_PC,
	     "orlix_tcti_gadget_executes_complete_conditional_select_family"),
	SPEC("CSINC", 2, CONTROL_PC,
	     "orlix_tcti_gadget_executes_complete_conditional_select_family"),
	SPEC("CSINV", 2, CONTROL_PC,
	     "orlix_tcti_gadget_executes_complete_conditional_select_family"),
	SPEC("CSNEG", 2, CONTROL_PC,
	     "orlix_tcti_gadget_executes_complete_conditional_select_family"),
	SPEC("B_cond", 1, CONTROL_PC,
	     "orlix_tcti_gadget_executes_complete_conditional_branch_immediate_family"),
	SPEC("B_uncond", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_unconditional_branch_immediate_family"),
	SPEC("BL", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_unconditional_branch_immediate_family"),
	SPEC("BR", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_unconditional_branch_register_family"),
	SPEC("BLR", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_unconditional_branch_register_family"),
	SPEC("RET", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_unconditional_branch_register_family"),
	SPEC("CBZ", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_compare_branch_immediate_family"),
	SPEC("CBNZ", 2, REG_PC,
	     "orlix_tcti_gadget_executes_complete_compare_branch_immediate_family"),
	SPEC("TBZ", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_test_branch_immediate_family"),
	SPEC("TBNZ", 1, REG_PC,
	     "orlix_tcti_gadget_executes_complete_test_branch_immediate_family"),
	SPEC("SVC", 1, EXCEPTION_EXIT,
	     "orlix_tcti_resume_user_reports_syscall_and_register_state"),
	SPEC("BRK", 1, EXCEPTION_EXIT,
	     "orlix_tcti_resume_user_reports_breakpoint_and_register_state"),
	SPEC("HLT", 1, EXCEPTION_EXIT,
	     "orlix_tcti_resume_user_reports_hlt_as_undefined"),
	SPEC("NOP", 1, REG_PC,
	     "orlix_tcti_switch_executes_complete_hint_barrier_cache_family"),
	SPEC("YIELD", 1, REG_PC,
	     "orlix_tcti_switch_executes_complete_hint_barrier_cache_family"),
	SPEC("WFE", 1, REG_PC,
	     "orlix_tcti_switch_executes_complete_hint_barrier_cache_family"),
	SPEC("WFI", 1, REG_PC,
	     "orlix_tcti_switch_executes_complete_hint_barrier_cache_family"),
	SPEC("SEV", 1, REG_PC,
	     "orlix_tcti_switch_executes_complete_hint_barrier_cache_family"),
	SPEC("SEVL", 1, REG_PC,
	     "orlix_tcti_switch_executes_complete_hint_barrier_cache_family"),
	SPEC("CSDB", 1, BARRIER,
	     "orlix_tcti_switch_executes_complete_hint_barrier_cache_family"),
	SPEC("HINT", 1, REG_PC,
	     "orlix_tcti_switch_executes_complete_hint_barrier_cache_family"),
	SPEC("CLREX", 1, SYSTEM_STATE,
	     "orlix_tcti_switch_executes_exclusive_monitor_clear"),
	SPEC("DSB", 1, BARRIER,
	     "orlix_tcti_switch_executes_complete_hint_barrier_cache_family"),
	SPEC("DMB", 1, BARRIER,
	     "orlix_tcti_switch_executes_complete_hint_barrier_cache_family"),
	SPEC("ISB", 1, BARRIER,
	     "orlix_tcti_switch_executes_complete_hint_barrier_cache_family"),
	SPEC("SYS", 1, SYSTEM_STATE,
	     "orlix_tcti_switch_executes_complete_el0_system_registers"),
	SPEC("MSR_reg", 1, SYSTEM_STATE,
	     "orlix_tcti_switch_executes_complete_el0_system_registers"),
	SPEC("MRS", 1, SYSTEM_STATE,
	     "orlix_tcti_switch_executes_complete_el0_system_registers"),
};
#undef SPEC

static struct orlix_tcti_scalar_operation_catalog_entry
	catalog[ORLIX_TCTI_SCALAR_OPERATION_CATALOG_EXPECTED_COUNT];
static size_t catalog_count;
static bool catalog_initialized;

static bool empty(const char *text)
{
	return !text || !text[0];
}

struct sha256_state {
	uint32_t hash[8];
	uint64_t bytes;
	uint8_t block[64];
	size_t used;
};

static uint32_t rotate_right(uint32_t value, unsigned int shift)
{
	return (value >> shift) | (value << (32 - shift));
}

static void sha256_transform(struct sha256_state *state, const uint8_t *block)
{
	static const uint32_t constants[64] = {
		0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
		0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
		0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
		0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
		0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
		0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
		0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
		0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
		0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
		0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
		0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
		0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
		0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
		0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
		0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
		0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
	};
	uint32_t words[64];
	uint32_t a, b, c, d, e, f, g, h;
	size_t index;

	for (index = 0; index < 16; index++)
		words[index] = ((uint32_t)block[index * 4] << 24) |
			((uint32_t)block[index * 4 + 1] << 16) |
			((uint32_t)block[index * 4 + 2] << 8) |
			block[index * 4 + 3];
	for (index = 16; index < 64; index++) {
		uint32_t s0 = rotate_right(words[index - 15], 7) ^
			rotate_right(words[index - 15], 18) ^
			(words[index - 15] >> 3);
		uint32_t s1 = rotate_right(words[index - 2], 17) ^
			rotate_right(words[index - 2], 19) ^
			(words[index - 2] >> 10);

		words[index] = words[index - 16] + s0 + words[index - 7] + s1;
	}
	a = state->hash[0];
	b = state->hash[1];
	c = state->hash[2];
	d = state->hash[3];
	e = state->hash[4];
	f = state->hash[5];
	g = state->hash[6];
	h = state->hash[7];
	for (index = 0; index < 64; index++) {
		uint32_t sum1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^
			rotate_right(e, 25);
		uint32_t choice = (e & f) ^ (~e & g);
		uint32_t temporary1 = h + sum1 + choice + constants[index] +
			words[index];
		uint32_t sum0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^
			rotate_right(a, 22);
		uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
		uint32_t temporary2 = sum0 + majority;

		h = g;
		g = f;
		f = e;
		e = d + temporary1;
		d = c;
		c = b;
		b = a;
		a = temporary1 + temporary2;
	}
	state->hash[0] += a;
	state->hash[1] += b;
	state->hash[2] += c;
	state->hash[3] += d;
	state->hash[4] += e;
	state->hash[5] += f;
	state->hash[6] += g;
	state->hash[7] += h;
}

static void sha256_update(struct sha256_state *state, const void *data,
			  size_t length)
{
	const uint8_t *bytes = data;

	state->bytes += length;
	while (length) {
		size_t available = sizeof(state->block) - state->used;
		size_t copied = length < available ? length : available;

		memcpy(state->block + state->used, bytes, copied);
		state->used += copied;
		bytes += copied;
		length -= copied;
		if (state->used == sizeof(state->block)) {
			sha256_transform(state, state->block);
			state->used = 0;
		}
	}
}

static void sha256_finish(struct sha256_state *state, uint8_t output[32])
{
	uint64_t bits = state->bytes * 8;
	size_t index;

	state->block[state->used++] = 0x80;
	if (state->used > 56) {
		memset(state->block + state->used, 0,
		       sizeof(state->block) - state->used);
		sha256_transform(state, state->block);
		state->used = 0;
	}
	memset(state->block + state->used, 0, 56 - state->used);
	for (index = 0; index < 8; index++)
		state->block[63 - index] = (uint8_t)(bits >> (index * 8));
	sha256_transform(state, state->block);
	for (index = 0; index < 8; index++) {
		output[index * 4] = (uint8_t)(state->hash[index] >> 24);
		output[index * 4 + 1] =
			(uint8_t)(state->hash[index] >> 16);
		output[index * 4 + 2] = (uint8_t)(state->hash[index] >> 8);
		output[index * 4 + 3] = (uint8_t)state->hash[index];
	}
}

static void sha256_init(struct sha256_state *state)
{
	static const uint32_t initial[8] = {
		0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
		0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
	};

	memset(state, 0, sizeof(*state));
	memcpy(state->hash, initial, sizeof(initial));
}

static void sha256_hex(const uint8_t digest[32],
		       char output[ORLIX_TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE])
{
	static const char digits[] = "0123456789abcdef";
	size_t index;

	for (index = 0; index < 32; index++) {
		output[index * 2] = digits[digest[index] >> 4];
		output[index * 2 + 1] = digits[digest[index] & 0xf];
	}
	output[64] = '\0';
}

static bool valid_sha256_hex(
	const char digest[ORLIX_TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE])
{
	size_t index;

	if (!digest)
		return false;
	for (index = 0; index < 64; index++)
		if (!((digest[index] >= '0' && digest[index] <= '9') ||
		      (digest[index] >= 'a' && digest[index] <= 'f')))
			return false;
	return digest[64] == '\0';
}

static bool sha256_self_test(void)
{
	static const uint8_t abc[] = { 'a', 'b', 'c' };
	static const char expected[] =
		"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
	struct sha256_state state;
	char actual[ORLIX_TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE];
	uint8_t digest[32];

	sha256_init(&state);
	sha256_update(&state, abc, sizeof(abc));
	sha256_finish(&state, digest);
	sha256_hex(digest, actual);
	return !strcmp(actual, expected);
}

static void sha256_update_u32(struct sha256_state *state, uint32_t value)
{
	uint8_t bytes[4] = {
		(uint8_t)(value >> 24), (uint8_t)(value >> 16),
		(uint8_t)(value >> 8), (uint8_t)value,
	};

	sha256_update(state, bytes, sizeof(bytes));
}

static void sha256_update_u64(struct sha256_state *state, uint64_t value)
{
	uint8_t bytes[8];
	size_t index;

	for (index = 0; index < sizeof(bytes); index++)
		bytes[index] = (uint8_t)(value >> ((7 - index) * 8));
	sha256_update(state, bytes, sizeof(bytes));
}

static void sha256_update_field(struct sha256_state *state, const char *text)
{
	size_t length = strlen(text);

	sha256_update_u64(state, length);
	sha256_update(state, text, length);
}

static int source_digest(
	const struct orlix_tcti_scalar_operation_catalog_entry *entry,
	uint8_t output[32])
{
	struct sha256_state state;

	if (!entry || !output || empty(entry->leaf_name) ||
	    empty(entry->mnemonic) || empty(entry->operation_id) ||
	    empty(entry->condition_tcnd_hex))
		return -1;
	sha256_init(&state);
	sha256_update_field(&state, entry->leaf_name);
	sha256_update_field(&state, entry->mnemonic);
	sha256_update_field(&state, entry->operation_id);
	sha256_update_u32(&state, entry->encoding_mask);
	sha256_update_u32(&state, entry->encoding_pattern);
	sha256_update_field(&state, entry->condition_tcnd_hex);
	sha256_finish(&state, output);
	return 0;
}

int orlix_tcti_scalar_operation_source_sha256(
	const struct orlix_tcti_scalar_operation_catalog_entry *entry,
	char output[ORLIX_TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE])
{
	uint8_t digest[32];

	if (!output || source_digest(entry, digest))
		return -1;
	sha256_hex(digest, output);
	return 0;
}

int orlix_tcti_scalar_operation_catalog_sha256(
	const struct orlix_tcti_scalar_operation_catalog_entry *entries, size_t count,
	char output[ORLIX_TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE])
{
	struct sha256_state state;
	uint8_t digest[32];
	size_t index;

	if ((!entries && count) || !output)
		return -1;
	sha256_init(&state);
	sha256_update_u64(&state, count);
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_scalar_operation_catalog_entry *entry =
			&entries[index];

		if (!valid_sha256_hex(entry->source_sha256) ||
		    empty(entry->kunit_source) ||
		    !valid_sha256_hex(entry->kunit_source_sha256) ||
		    empty(entry->kunit_object) ||
		    empty(entry->kunit_suite) ||
		    empty(entry->kunit_suite_symbol) ||
		    empty(entry->kunit_case_array) ||
		    empty(entry->kunit_case))
			return -1;
		sha256_update_u64(&state, 64);
		sha256_update(&state, entry->source_sha256, 64);
		sha256_update_u32(&state, entry->obligations);
		sha256_update_field(&state, entry->kunit_source);
		sha256_update_field(&state, entry->kunit_source_sha256);
		sha256_update_field(&state, entry->kunit_object);
		sha256_update_field(&state, entry->kunit_suite);
		sha256_update_field(&state, entry->kunit_suite_symbol);
		sha256_update_field(&state, entry->kunit_case_array);
		sha256_update_field(&state, entry->kunit_case);
	}
	sha256_finish(&state, digest);
	sha256_hex(digest, output);
	return 0;
}

static const struct operation_spec *find_spec(const char *operation_id)
{
	size_t index;

	for (index = 0; index < sizeof(operation_specs) /
				      sizeof(operation_specs[0]); index++)
		if (!strcmp(operation_specs[index].operation_id, operation_id))
			return &operation_specs[index];
	return NULL;
}

static void initialize_catalog(void)
{
	size_t row;

	if (catalog_initialized)
		return;
	for (row = 0; row < sizeof(source_rows) / sizeof(source_rows[0]); row++) {
		const struct source_row *source = &source_rows[row];
		const struct classification_row *classification =
			&classification_rows[row];
		const struct operation_spec *spec =
			find_spec(source->operation_id);
		struct orlix_tcti_scalar_operation_catalog_entry *entry;

		if (strcmp(source->leaf_name, classification->leaf_name) ||
		    !classification->required || !spec ||
		    catalog_count >=
				     ORLIX_TCTI_SCALAR_OPERATION_CATALOG_EXPECTED_COUNT)
			continue;
		entry = &catalog[catalog_count++];
		entry->leaf_name = source->leaf_name;
		entry->mnemonic = source->mnemonic;
		entry->operation_id = source->operation_id;
		entry->encoding_mask = source->encoding_mask;
		entry->encoding_pattern = source->encoding_pattern;
		entry->condition_tcnd_hex = source->condition_tcnd_hex;
		entry->obligations = spec->obligations;
		entry->kunit_source = DECODE_SOURCE;
		entry->kunit_source_sha256 = DECODE_SOURCE_SHA256;
		entry->kunit_object = DECODE_OBJECT;
		entry->kunit_suite = DECODE_SUITE;
		entry->kunit_suite_symbol = DECODE_SUITE_SYMBOL;
		entry->kunit_case_array = DECODE_CASE_ARRAY;
		entry->kunit_case = spec->kunit_case;
		entry->proof_state = ORLIX_TCTI_SCALAR_OPERATION_UNPROVED;
		if (orlix_tcti_scalar_operation_source_sha256(
			    entry, entry->source_sha256))
			entry->source_sha256[0] = '\0';
	}
	catalog_initialized = true;
}

const struct orlix_tcti_scalar_operation_catalog_entry *
orlix_tcti_scalar_operation_catalog_entries(size_t *count)
{
	initialize_catalog();
	if (count)
		*count = catalog_count;
	return catalog;
}

static bool looks_coarse(const char *operation_id)
{
	size_t length;

	if (empty(operation_id))
		return false;
	length = strlen(operation_id);
	return strchr(operation_id, '*') || strchr(operation_id, '?') ||
	       operation_id[length - 1] == '_' ||
	       !strcmp(operation_id, "scalar") ||
	       !strcmp(operation_id, "integer") ||
	       !strcmp(operation_id, "control");
}

int orlix_tcti_scalar_operation_catalog_validate(
	const struct orlix_tcti_scalar_operation_catalog_entry *entries, size_t count,
	enum orlix_tcti_scalar_operation_catalog_error *error)
{
	size_t operation_counts[sizeof(operation_specs) /
				sizeof(operation_specs[0])] = { 0 };
	size_t index;
	size_t previous;
	uint32_t known_obligations =
		ORLIX_TCTI_SCALAR_OBLIGATION_DECODE |
		ORLIX_TCTI_SCALAR_OBLIGATION_LEGAL_ENCODINGS |
		ORLIX_TCTI_SCALAR_OBLIGATION_REJECTED_ENCODINGS |
		ORLIX_TCTI_SCALAR_OBLIGATION_REGISTERS |
		ORLIX_TCTI_SCALAR_OBLIGATION_PC |
		ORLIX_TCTI_SCALAR_OBLIGATION_FLAGS |
		ORLIX_TCTI_SCALAR_OBLIGATION_FAULTS |
		ORLIX_TCTI_SCALAR_OBLIGATION_ORDERING |
		ORLIX_TCTI_SCALAR_OBLIGATION_SYSTEM_STATE |
		ORLIX_TCTI_SCALAR_OBLIGATION_STRUCTURED_EXIT;

	if (error)
		*error = ORLIX_TCTI_SCALAR_OPERATION_CATALOG_OK;
	if (!entries && count)
		goto invalid_argument;
	if (!sha256_self_test())
		goto invalid_argument;
	if (count != ORLIX_TCTI_SCALAR_OPERATION_CATALOG_EXPECTED_COUNT) {
		if (error)
			*error = ORLIX_TCTI_SCALAR_OPERATION_CATALOG_WRONG_COUNT;
		return -1;
	}
	for (index = 0; index < count; index++) {
		const struct orlix_tcti_scalar_operation_catalog_entry *entry =
			&entries[index];
		const struct operation_spec *spec;
		char expected_sha256[ORLIX_TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE];
		size_t spec_index;

		if (empty(entry->leaf_name) || empty(entry->mnemonic) ||
		    empty(entry->operation_id) ||
		    empty(entry->condition_tcnd_hex)) {
			if (error)
				*error =
					ORLIX_TCTI_SCALAR_OPERATION_CATALOG_MISSING_BINDING;
			return -1;
		}
		if (looks_coarse(entry->operation_id)) {
			if (error)
				*error =
					ORLIX_TCTI_SCALAR_OPERATION_CATALOG_COARSE_OPERATION;
			return -1;
		}
		spec = find_spec(entry->operation_id);
		if (!spec) {
			if (error)
				*error =
					ORLIX_TCTI_SCALAR_OPERATION_CATALOG_UNKNOWN_OPERATION;
			return -1;
		}
		spec_index = (size_t)(spec - operation_specs);
		operation_counts[spec_index]++;
		if (!entry->obligations ||
		    entry->obligations & ~known_obligations ||
		    entry->obligations != spec->obligations) {
			if (error)
				*error =
					ORLIX_TCTI_SCALAR_OPERATION_CATALOG_INVALID_OBLIGATIONS;
			return -1;
		}
		if (strcmp(entry->kunit_source, DECODE_SOURCE) ||
		    strcmp(entry->kunit_source_sha256,
			   DECODE_SOURCE_SHA256) ||
		    strcmp(entry->kunit_object, DECODE_OBJECT) ||
		    strcmp(entry->kunit_suite, DECODE_SUITE) ||
		    strcmp(entry->kunit_suite_symbol,
			   DECODE_SUITE_SYMBOL) ||
		    strcmp(entry->kunit_case_array, DECODE_CASE_ARRAY) ||
		    strcmp(entry->kunit_case, spec->kunit_case)) {
			if (error)
				*error =
					ORLIX_TCTI_SCALAR_OPERATION_CATALOG_INVALID_PROVENANCE;
			return -1;
		}
		if (entry->proof_state != ORLIX_TCTI_SCALAR_OPERATION_UNPROVED) {
			if (error)
				*error =
					ORLIX_TCTI_SCALAR_OPERATION_CATALOG_ALREADY_PROVED;
			return -1;
		}
		if (!valid_sha256_hex(entry->source_sha256) ||
		    orlix_tcti_scalar_operation_source_sha256(entry, expected_sha256) ||
		    strcmp(entry->source_sha256, expected_sha256)) {
			if (error)
				*error =
					ORLIX_TCTI_SCALAR_OPERATION_CATALOG_STALE_SOURCE;
			return -1;
		}
		for (previous = 0; previous < index; previous++) {
			if (!strcmp(entry->leaf_name,
				    entries[previous].leaf_name)) {
				if (error)
					*error =
						ORLIX_TCTI_SCALAR_OPERATION_CATALOG_DUPLICATE_LEAF;
				return -1;
			}
			if (entry->encoding_mask ==
				    entries[previous].encoding_mask &&
			    entry->encoding_pattern ==
				    entries[previous].encoding_pattern &&
			    !strcmp(entry->condition_tcnd_hex,
				    entries[previous].condition_tcnd_hex)) {
				if (error)
					*error =
						ORLIX_TCTI_SCALAR_OPERATION_CATALOG_DUPLICATE_SOURCE_ENCODING;
				return -1;
			}
		}
	}
	for (index = 0; index < sizeof(operation_specs) /
				      sizeof(operation_specs[0]); index++) {
		if (operation_counts[index] !=
		    operation_specs[index].expected_leaves) {
			if (error)
				*error =
					ORLIX_TCTI_SCALAR_OPERATION_CATALOG_SOURCE_MANIFEST_DRIFT;
			return -1;
		}
	}
	{
		char catalog_sha256[ORLIX_TCTI_SCALAR_OPERATION_SHA256_HEX_SIZE];

		if (orlix_tcti_scalar_operation_catalog_sha256(
			    entries, count, catalog_sha256) ||
		    strcmp(catalog_sha256,
			   ORLIX_TCTI_SCALAR_OPERATION_CATALOG_REVIEWED_SHA256)) {
			if (error)
				*error =
					ORLIX_TCTI_SCALAR_OPERATION_CATALOG_SOURCE_MANIFEST_DRIFT;
			return -1;
		}
	}
	return 0;

invalid_argument:
	if (error)
		*error = ORLIX_TCTI_SCALAR_OPERATION_CATALOG_INVALID_ARGUMENT;
	return -1;
}
