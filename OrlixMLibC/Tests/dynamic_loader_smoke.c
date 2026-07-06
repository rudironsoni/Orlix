/* SPDX-License-Identifier: MIT */

#include <elf.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/auxv.h>

int main(void)
{
	unsigned long at_base = getauxval(AT_BASE);

	if (!at_base) {
		puts("ORLIX-MLIBC-DYNAMIC-LOADER-AT-BASE-MISSING");
		return EXIT_FAILURE;
	}

	printf("ORLIX-MLIBC-DYNAMIC-LOADER-OK AT_BASE=%#lx\n", at_base);
	return EXIT_SUCCESS;
}
