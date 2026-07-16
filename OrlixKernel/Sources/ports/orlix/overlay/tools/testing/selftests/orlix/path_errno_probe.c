// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

static bool open_fails_with_errno(const char *path, int expected_errno)
{
	int fd;
	int actual_errno;

	errno = 0;
	fd = open(path, O_RDONLY);
	if (fd >= 0) {
		close(fd);
		return false;
	}
	actual_errno = errno;
	return actual_errno == expected_errno;
}

static bool stat_fails_with_errno(const char *path, int expected_errno)
{
	struct stat st;
	int actual_errno;

	errno = 0;
	if (stat(path, &st) == 0)
		return false;
	actual_errno = errno;
	return actual_errno == expected_errno;
}

int main(void)
{
	int fd;
	bool fixture_created;
	bool cleaned;

	orlix_test_plan(6);

	unlink("path-errno-root/regular");
	unlink("path-errno-root/loop-a");
	unlink("path-errno-root/loop-b");
	rmdir("path-errno-root/dir");
	rmdir("path-errno-root");

	fixture_created = mkdir("path-errno-root", 0700) == 0 &&
			  mkdir("path-errno-root/dir", 0700) == 0;
	fd = open("path-errno-root/regular", O_CREAT | O_EXCL | O_WRONLY, 0600);
	fixture_created = fixture_created && fd >= 0;
	if (fd >= 0)
		close(fd);
	fixture_created = fixture_created &&
			  symlink("loop-b", "path-errno-root/loop-a") == 0 &&
			  symlink("loop-a", "path-errno-root/loop-b") == 0;

	orlix_test_result(fixture_created,
			  "path errno fixture created through Linux VFS");
	orlix_test_result(open_fails_with_errno("path-errno-root/missing", ENOENT),
			  "missing path returns ENOENT");
	orlix_test_result(
		open_fails_with_errno("path-errno-root/regular/child",
				      ENOTDIR),
		"non-directory child returns ENOTDIR");
	orlix_test_result(stat_fails_with_errno("path-errno-root/loop-a",
						ELOOP),
			  "symlink loop returns ELOOP");
	orlix_test_result(stat_fails_with_errno("path-errno-root/regular/",
						ENOTDIR),
			  "trailing slash on regular file returns ENOTDIR");

	cleaned = unlink("path-errno-root/loop-a") == 0 &&
		  unlink("path-errno-root/loop-b") == 0 &&
		  unlink("path-errno-root/regular") == 0 &&
		  rmdir("path-errno-root/dir") == 0 &&
		  rmdir("path-errno-root") == 0;
	orlix_test_result(cleaned, "path errno fixture cleaned");

	orlix_test_exit();
}
