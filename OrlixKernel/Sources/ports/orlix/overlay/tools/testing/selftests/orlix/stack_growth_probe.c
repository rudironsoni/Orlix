// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/resource.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define STACK_HEADROOM	(4UL * 1024UL * 1024UL)
#define STACK_GROWTH	(2UL * 1024UL * 1024UL)
#define STACK_FRAME	8192UL

static size_t page_size;
static volatile unsigned long stack_checksum;

static bool current_virtual_size(unsigned long *size)
{
	char buffer[64];
	unsigned long pages = 0;
	ssize_t length;
	size_t pos = 0;
	int fd;

	fd = open("/proc/self/statm", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;
	length = read(fd, buffer, sizeof(buffer));
	(void)close(fd);
	if (length <= 0)
		return false;

	while (pos < (size_t)length && buffer[pos] >= '0' &&
	       buffer[pos] <= '9') {
		unsigned long digit = (unsigned long)(buffer[pos] - '0');

		if (pages > (ULONG_MAX - digit) / 10)
			return false;
		pages = pages * 10 + digit;
		pos++;
	}
	if (!pos || pages > ULONG_MAX / page_size)
		return false;

	*size = pages * page_size;
	return true;
}

__attribute__((noinline)) static void grow_stack(size_t remaining)
{
	volatile unsigned char frame[STACK_FRAME];
	size_t offset;

	for (offset = 0; offset < sizeof(frame); offset += page_size)
		frame[offset] = 0x5a;
	frame[sizeof(frame) - 1] = 0xa5;

	if (remaining > sizeof(frame))
		grow_stack(remaining - sizeof(frame));

	stack_checksum += frame[0];
	stack_checksum += frame[sizeof(frame) - 1];
}

int main(void)
{
	struct rlimit limit;
	unsigned long virtual_size;
	long configured_page_size;

	orlix_test_plan(1);
	configured_page_size = sysconf(_SC_PAGESIZE);
	if (configured_page_size <= 0) {
		orlix_test_result(false, "sysconf reports the Linux page size");
		orlix_test_exit();
	}
	page_size = (size_t)configured_page_size;

	if (!current_virtual_size(&virtual_size) ||
	    virtual_size > ULONG_MAX - STACK_HEADROOM) {
		orlix_test_result(false, "current virtual size is measurable");
		orlix_test_exit();
	}

	limit.rlim_cur = virtual_size + STACK_HEADROOM;
	limit.rlim_max = limit.rlim_cur;
	if (setrlimit(RLIMIT_AS, &limit) != 0) {
		orlix_test_result(false, "RLIMIT_AS accepts stack headroom");
		orlix_test_exit();
	}

	grow_stack(STACK_GROWTH);
	orlix_test_result(stack_checksum != 0,
			  "grow-down stack faults within RLIMIT_AS headroom");
	orlix_test_exit();
}
