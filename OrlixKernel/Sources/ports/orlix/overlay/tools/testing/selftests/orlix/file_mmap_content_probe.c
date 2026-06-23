// SPDX-License-Identifier: GPL-2.0

#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

static void write_literal(const char *text)
{
	size_t len = 0;

	while (text[len])
		len++;
	(void)write(STDOUT_FILENO, text, len);
}

static void write_hex_byte(unsigned char value)
{
	static const char digits[] = "0123456789abcdef";
	char out[2];

	out[0] = digits[value >> 4];
	out[1] = digits[value & 0xf];
	(void)write(STDOUT_FILENO, out, sizeof(out));
}

static void write_unsigned(unsigned long value)
{
	char buffer[32];
	size_t used = 0;

	if (value == 0) {
		write_literal("0");
		return;
	}
	while (value && used < sizeof(buffer)) {
		buffer[used++] = (char)('0' + (value % 10));
		value /= 10;
	}
	while (used)
		(void)write(STDOUT_FILENO, &buffer[--used], 1);
}

static void report_mismatch(off_t offset, unsigned char mmap_byte,
			    unsigned char pread_byte)
{
	write_literal("# FM BAD O");
	write_unsigned((unsigned long)offset);
	write_literal(" M");
	write_hex_byte(mmap_byte);
	write_literal(" P");
	write_hex_byte(pread_byte);
	write_literal("\n");
}

static bool check_offset(int fd, const unsigned char *mapped, off_t offset)
{
	unsigned char via_pread = 0;
	ssize_t nread;

	nread = pread(fd, &via_pread, 1, offset);
	if (nread != 1)
		return false;

	if (mapped[offset] != via_pread) {
		report_mismatch(offset, mapped[offset], via_pread);
		return false;
	}

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
