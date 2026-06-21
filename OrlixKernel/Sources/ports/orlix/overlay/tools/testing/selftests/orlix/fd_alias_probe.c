// SPDX-License-Identifier: GPL-2.0
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define FD_ALIAS_PROBE_FILE "/tmp/orlix-fd-alias-probe.txt"
#define FD_ALIAS_PROBE_PAYLOAD "orlix-fd-alias\n"

static bool append_uint(char *buffer, size_t buffer_size, size_t *pos,
			unsigned int value)
{
	char digits[16];
	size_t count = 0;

	do {
		digits[count++] = (char)('0' + (value % 10));
		value /= 10;
	} while (value != 0 && count < sizeof(digits));

	while (count > 0) {
		if (*pos + 1 >= buffer_size)
			return false;
		buffer[(*pos)++] = digits[--count];
	}

	buffer[*pos] = '\0';
	return true;
}

static bool make_fd_path(char *buffer, size_t buffer_size, const char *prefix,
			 int fd)
{
	size_t pos = 0;

	pos = orlix_append_cstr(buffer, pos, buffer_size, prefix);
	return append_uint(buffer, buffer_size, &pos, (unsigned int)fd);
}

static bool path_is_directory(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool symlink_target_equals(const char *path, const char *expected)
{
	char buffer[128];
	ssize_t size;

	size = readlink(path, buffer, sizeof(buffer) - 1);
	if (size < 0)
		return false;
	buffer[size] = '\0';

	return orlix_memcmp(buffer, expected, orlix_strlen(expected)) == 0 &&
	       buffer[orlix_strlen(expected)] == '\0';
}

static bool stat_paths_match(const char *first, const char *second)
{
	struct stat first_st;
	struct stat second_st;

	return stat(first, &first_st) == 0 && stat(second, &second_st) == 0 &&
	       first_st.st_dev == second_st.st_dev &&
	       first_st.st_ino == second_st.st_ino;
}

static bool fd_alias_opens_referenced_file(int fd, const char *prefix)
{
	char path[64];
	char buffer[sizeof(FD_ALIAS_PROBE_PAYLOAD)] = { 0 };
	struct stat expected;
	struct stat actual;
	int alias_fd;
	ssize_t size;

	if (!make_fd_path(path, sizeof(path), prefix, fd))
		return false;
	if (fstat(fd, &expected) != 0)
		return false;

	alias_fd = open(path, O_RDONLY | O_CLOEXEC);
	if (alias_fd < 0)
		return false;

	size = read(alias_fd, buffer, sizeof(FD_ALIAS_PROBE_PAYLOAD) - 1);
	if (fstat(alias_fd, &actual) != 0) {
		close(alias_fd);
		return false;
	}
	close(alias_fd);

	return size == (ssize_t)(sizeof(FD_ALIAS_PROBE_PAYLOAD) - 1) &&
	       orlix_memcmp(buffer, FD_ALIAS_PROBE_PAYLOAD,
			    sizeof(FD_ALIAS_PROBE_PAYLOAD) - 1) == 0 &&
	       expected.st_dev == actual.st_dev && expected.st_ino == actual.st_ino;
}

static bool fd_path_exists_for_stdio(int fd, const char *prefix)
{
	char path[64];
	struct stat st;

	return make_fd_path(path, sizeof(path), prefix, fd) && stat(path, &st) == 0;
}

static int create_probe_file(void)
{
	int fd;

	fd = open(FD_ALIAS_PROBE_FILE, O_CREAT | O_TRUNC | O_RDWR | O_CLOEXEC,
		  0600);
	if (fd < 0)
		return -1;
	if (write(fd, FD_ALIAS_PROBE_PAYLOAD,
		  sizeof(FD_ALIAS_PROBE_PAYLOAD) - 1) !=
	    (ssize_t)(sizeof(FD_ALIAS_PROBE_PAYLOAD) - 1)) {
		close(fd);
		return -1;
	}
	if (lseek(fd, 0, SEEK_SET) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

int main(void)
{
	int fd;

	orlix_test_plan(11);

	fd = create_probe_file();
	orlix_test_result(fd >= 0, "fd alias probe file opens");
	orlix_test_result(path_is_directory("/dev/fd"), "/dev/fd is a directory");
	orlix_test_result(path_is_directory("/proc/self/fd"),
			  "/proc/self/fd is a directory");
	orlix_test_result(symlink_target_equals("/dev/fd", "/proc/self/fd"),
			  "/dev/fd aliases /proc/self/fd");
	orlix_test_result(symlink_target_equals("/dev/stdin", "/proc/self/fd/0"),
			  "/dev/stdin aliases fd 0");
	orlix_test_result(symlink_target_equals("/dev/stdout", "/proc/self/fd/1"),
			  "/dev/stdout aliases fd 1");
	orlix_test_result(symlink_target_equals("/dev/stderr", "/proc/self/fd/2"),
			  "/dev/stderr aliases fd 2");
	orlix_test_result(fd >= 0 && fd_alias_opens_referenced_file(fd, "/dev/fd/"),
			  "/dev/fd/N opens referenced file");
	orlix_test_result(fd >= 0 &&
				  fd_alias_opens_referenced_file(fd,
								 "/proc/self/fd/"),
			  "/proc/self/fd/N opens referenced file");
	orlix_test_result(stat_paths_match("/dev/stdout", "/proc/self/fd/1"),
			  "/dev/stdout resolves to proc fd 1");
	orlix_test_result(fd_path_exists_for_stdio(2, "/dev/fd/") &&
				  fd_path_exists_for_stdio(2, "/proc/self/fd/"),
			  "fd directories expose stderr");

	if (fd >= 0)
		close(fd);
	unlink(FD_ALIAS_PROBE_FILE);
	orlix_test_exit();
}
