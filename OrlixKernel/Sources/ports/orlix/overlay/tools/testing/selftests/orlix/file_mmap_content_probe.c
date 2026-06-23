// SPDX-License-Identifier: GPL-2.0

#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

static bool check_offset(int fd, const unsigned char *mapped, off_t offset)
{
	unsigned char via_pread = 0;
	ssize_t nread;

	nread = pread(fd, &via_pread, 1, offset);
	if (nread != 1)
		return false;

	if (mapped[offset] != via_pread)
		return false;

	return true;
}

int main(void)
{
	const char *test_path = "/kselftest-list.txt";
	const unsigned char *mapped;
	struct stat st;
	off_t offsets[5];
	size_t offset_count = 0;
	int fd;
	bool ok = true;

	orlix_test_plan(4);

	fd = open(test_path, O_RDONLY);
	orlix_test_result(fd >= 0, "can open regular initramfs test data");
	if (fd < 0)
		orlix_test_exit();

	orlix_test_result(fstat(fd, &st) == 0 && st.st_size >= 128,
			  "regular initramfs data has mmap-sized file contents");
	if (st.st_size < 128) {
		close(fd);
		orlix_test_exit();
	}

	mapped = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	orlix_test_result(mapped != MAP_FAILED,
			  "file-backed MAP_PRIVATE mapping succeeds");
	if (mapped == MAP_FAILED) {
		close(fd);
		orlix_test_exit();
	}

	offsets[offset_count++] = 0;
	offsets[offset_count++] = 1;
	offsets[offset_count++] = 4;
	offsets[offset_count++] = 16;
	offsets[offset_count++] = st.st_size / 2;

	for (size_t i = 0; i < offset_count; i++)
		ok = check_offset(fd, mapped, offsets[i]) && ok;

	orlix_test_result(ok, "file-backed mmap bytes match pread bytes");

	munmap((void *)mapped, (size_t)st.st_size);
	close(fd);
	orlix_test_exit();
}
