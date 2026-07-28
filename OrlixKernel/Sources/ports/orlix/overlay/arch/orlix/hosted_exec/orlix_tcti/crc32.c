// SPDX-License-Identifier: GPL-2.0-only
#include <asm/ptrace.h>
#include <linux/errno.h>
#include <linux/types.h>

#include "crc32.h"
#include "decode_aarch64.h"

#define ORLIX_TCTI_CRC32_POLYNOMIAL 0xedb88320U
#define ORLIX_TCTI_CRC32C_POLYNOMIAL 0x82f63b78U

static u32 orlix_tcti_crc32_update(u32 accumulator, u64 value, u8 byte_count,
				     u32 polynomial)
{
	u8 byte;

	for (byte = 0; byte < byte_count; byte++) {
		u8 bit;

		accumulator ^= (u8)(value >> (byte * 8));
		for (bit = 0; bit < 8; bit++)
			accumulator = (accumulator >> 1) ^
				(-(accumulator & 1U) & polynomial);
	}
	return accumulator;
}

int orlix_tcti_execute_crc32(struct pt_regs *regs,
			       const struct orlix_tcti_decoded_instruction *decoded)
{
	u32 polynomial;
	u32 accumulator;
	u64 value;
	u32 result;

	if (!regs || !decoded ||
	    decoded->decode_class != ORLIX_TCTI_DECODE_DATA_PROCESSING_2SOURCE ||
	    (decoded->dp2_op != ORLIX_TCTI_DP2_CRC32 &&
	     decoded->dp2_op != ORLIX_TCTI_DP2_CRC32C) ||
	    (decoded->access_size != sizeof(u8) &&
	     decoded->access_size != sizeof(u16) &&
	     decoded->access_size != sizeof(u32) &&
	     decoded->access_size != sizeof(u64)) ||
	    decoded->result_size != sizeof(u32) ||
	    decoded->is_64bit != (decoded->access_size == sizeof(u64)))
		return -EINVAL;

	accumulator = decoded->rn == 31 ? 0 : (u32)regs->regs[decoded->rn];
	value = decoded->rm == 31 ? 0 : regs->regs[decoded->rm];
	polynomial = decoded->dp2_op == ORLIX_TCTI_DP2_CRC32C ?
		ORLIX_TCTI_CRC32C_POLYNOMIAL : ORLIX_TCTI_CRC32_POLYNOMIAL;
	result = orlix_tcti_crc32_update(accumulator, value,
					 decoded->access_size, polynomial);
	if (decoded->rd != 31)
		regs->regs[decoded->rd] = result;
	regs->pc += sizeof(u32);
	return 0;
}
