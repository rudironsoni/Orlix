// SPDX-License-Identifier: GPL-2.0-only

#include <stdint.h>

#include "orlix_kselftest_user.h"

#ifndef ORLIX_FIXED_EXEC_BASE_ADDRESS
#error "ORLIX_FIXED_EXEC_BASE_ADDRESS must be provided by the Orlix kselftest target metadata"
#endif

static volatile uint64_t initialized_data = 0x0123456789abcdefULL;
static volatile uint64_t zero_data;

int main(void)
{
	uintptr_t pc = (uintptr_t)&main;

	orlix_test_plan(3);
	orlix_test_result(pc >= ORLIX_FIXED_EXEC_BASE_ADDRESS &&
			  pc < ORLIX_FIXED_EXEC_BASE_ADDRESS + 0x01000000UL,
			  "ET_EXEC retains its fixed Linux virtual address");
	orlix_test_result(initialized_data == 0x0123456789abcdefULL,
			  "ET_EXEC initialized data is readable through OrlixTCTI");
	zero_data = 0xfedcba9876543210ULL;
	orlix_test_result(zero_data == 0xfedcba9876543210ULL,
			  "ET_EXEC writable data is updated through OrlixTCTI");
	orlix_test_exit();
}
