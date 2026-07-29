// SPDX-License-Identifier: GPL-2.0
#include <sys/mman.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#ifndef PROT_MTE
#define PROT_MTE 0x20
#endif

int main(void)
{
	void *mapping;

	orlix_test_plan(2);
	mapping = mmap(NULL, getpagesize(), PROT_READ | PROT_WRITE | PROT_MTE,
		       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	orlix_test_result(mapping != MAP_FAILED,
			"anonymous PROT_MTE mapping is accepted");
	if (mapping != MAP_FAILED) {
		orlix_test_result(mprotect(mapping, getpagesize(),
					PROT_READ | PROT_MTE) == 0,
			"PROT_MTE eligibility survives mprotect");
		munmap(mapping, getpagesize());
	} else {
		orlix_test_result(0, "PROT_MTE eligibility survives mprotect");
	}
	orlix_test_exit();
}
