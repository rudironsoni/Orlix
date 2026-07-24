// SPDX-License-Identifier: GPL-2.0
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

static bool range_is_zero(const volatile unsigned char *memory, size_t size)
{
	size_t i;

	for (i = 0; i < size; i++) {
		if (memory[i] != 0)
			return false;
	}
	return true;
}

int main(void)
{
	volatile unsigned char *memory;
	long page_size;
	pid_t child;
	int status = 0;
	bool child_cow_ok;

	page_size = sysconf(_SC_PAGESIZE);
	if (page_size <= 0)
		page_size = getpagesize();

	orlix_test_plan(3);
	memory = mmap(NULL, 2 * (size_t)page_size, PROT_READ | PROT_WRITE,
		      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (memory == MAP_FAILED) {
		orlix_test_result(false,
				  "untouched anonymous memory reads as zero");
		orlix_test_result(false,
				  "writing anonymous memory preserves neighboring zero bytes");
		orlix_test_result(false,
				  "forked anonymous zero-page write is private");
		orlix_test_exit();
		return 1;
	}

	orlix_test_result(range_is_zero(memory, 2 * (size_t)page_size),
			  "untouched anonymous memory reads as zero");

	memory[0] = 0x5a;
	orlix_test_result(memory[0] == 0x5a &&
			  range_is_zero(memory + 1, (size_t)page_size - 1),
			  "writing anonymous memory preserves neighboring zero bytes");

	child = fork();
	if (child == 0) {
		memory[page_size] = 0xa5;
		_exit(memory[page_size] == 0xa5 ? 0 : 1);
	}
	child_cow_ok = child > 0 && waitpid(child, &status, 0) == child &&
		       WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
		       memory[page_size] == 0;
	orlix_test_result(child_cow_ok,
			  "forked anonymous zero-page write is private");

	munmap((void *)memory, 2 * (size_t)page_size);
	orlix_test_exit();
	return 0;
}
