// SPDX-License-Identifier: GPL-2.0
#include <asm/hwcap.h>
#include <elf.h>
#include <sys/auxv.h>

#include "orlix_kselftest_user.h"

int main(void)
{
	const unsigned long previous_unproved_hwcap =
		HWCAP_FP | HWCAP_ASIMD | HWCAP_AES | HWCAP_PMULL |
		HWCAP_SHA1 | HWCAP_SHA2 | HWCAP_CRC32 | HWCAP_SHA3 |
		HWCAP_SM3 | HWCAP_SM4 | HWCAP_SHA512;
	unsigned long hwcap = getauxval(AT_HWCAP);
	unsigned long hwcap2 = getauxval(AT_HWCAP2);

	orlix_test_plan(3);
	orlix_test_result(hwcap == 0,
			  "AT_HWCAP exposes only source-bound proved cohorts");
	orlix_test_result(hwcap != previous_unproved_hwcap,
			  "AT_HWCAP rejects the previous unproved feature mask");
	orlix_test_result(hwcap2 == 0,
			  "AT_HWCAP2 exposes no unimplemented extensions");
	orlix_test_exit();
}
