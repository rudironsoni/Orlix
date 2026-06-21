// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdbool.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define OVERLAY_BASE "/mnt/orlix-overlayfs-probe"
#define OVERLAY_LOWER OVERLAY_BASE "/lower"
#define OVERLAY_UPPER OVERLAY_BASE "/upper"
#define OVERLAY_WORK OVERLAY_BASE "/work"
#define OVERLAY_MERGED OVERLAY_BASE "/merged"
#define OVERLAY_OPTIONS                                                        \
	"lowerdir=" OVERLAY_LOWER ",upperdir=" OVERLAY_UPPER                  \
	",workdir=" OVERLAY_WORK

static bool mkdir_if_needed(const char *path)
{
	if (mkdir(path, 0755) == 0)
		return true;

	return errno == EEXIST;
}

static bool write_file(const char *path, const char *data)
{
	size_t len = orlix_strlen(data);
	int fd;
	ssize_t nwritten;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		return false;

	nwritten = write(fd, data, len);
	close(fd);

	return nwritten == (ssize_t)len;
}

static bool file_equals(const char *path, const char *expected)
{
	char buffer[64];
	size_t expected_len = orlix_strlen(expected);
	ssize_t nread;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return false;

	nread = read(fd, buffer, sizeof(buffer));
	close(fd);

	return nread == (ssize_t)expected_len &&
	       orlix_memcmp(buffer, expected, expected_len) == 0;
}

static bool setup_overlay_workspace(void)
{
	if (!mkdir_if_needed(OVERLAY_BASE))
		return false;

	if (unshare(CLONE_NEWNS) != 0)
		return false;

	if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) != 0)
		return false;

	if (mount("tmpfs", OVERLAY_BASE, "tmpfs", 0, "mode=0755") != 0)
		return false;

	return mkdir_if_needed(OVERLAY_LOWER) && mkdir_if_needed(OVERLAY_UPPER) &&
	       mkdir_if_needed(OVERLAY_WORK) && mkdir_if_needed(OVERLAY_MERGED);
}

static bool overlayfs_mounts_and_reads_lower(void)
{
	if (!setup_overlay_workspace())
		return false;

	if (!write_file(OVERLAY_LOWER "/lower.txt", "lower-data\n"))
		return false;

	if (mount("overlay", OVERLAY_MERGED, "overlay", 0, OVERLAY_OPTIONS) != 0)
		return false;

	return file_equals(OVERLAY_MERGED "/lower.txt", "lower-data\n");
}

static bool overlayfs_copy_up_preserves_lower(void)
{
	if (!setup_overlay_workspace())
		return false;

	if (!write_file(OVERLAY_LOWER "/copy-up.txt", "lower-data\n"))
		return false;

	if (mount("overlay", OVERLAY_MERGED, "overlay", 0, OVERLAY_OPTIONS) != 0)
		return false;

	if (access(OVERLAY_UPPER "/copy-up.txt", F_OK) == 0)
		return false;

	if (!write_file(OVERLAY_MERGED "/copy-up.txt", "upper-data\n"))
		return false;

	return file_equals(OVERLAY_LOWER "/copy-up.txt", "lower-data\n") &&
	       file_equals(OVERLAY_UPPER "/copy-up.txt", "upper-data\n") &&
	       file_equals(OVERLAY_MERGED "/copy-up.txt", "upper-data\n");
}

static bool overlayfs_unlink_hides_lower_file(void)
{
	if (!setup_overlay_workspace())
		return false;

	if (!write_file(OVERLAY_LOWER "/deleted.txt", "lower-data\n"))
		return false;

	if (mount("overlay", OVERLAY_MERGED, "overlay", 0, OVERLAY_OPTIONS) != 0)
		return false;

	if (unlink(OVERLAY_MERGED "/deleted.txt") != 0)
		return false;

	return access(OVERLAY_MERGED "/deleted.txt", F_OK) != 0 &&
	       errno == ENOENT &&
	       file_equals(OVERLAY_LOWER "/deleted.txt", "lower-data\n");
}

static bool child_exits_successfully(bool (*probe)(void))
{
	pid_t child;
	int status;

	child = fork();
	if (child < 0)
		return false;

	if (child == 0)
		_exit(probe() ? 0 : 1);

	if (waitpid(child, &status, 0) != child)
		return false;

	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(void)
{
	orlix_test_plan(3);

	orlix_test_result(
		child_exits_successfully(overlayfs_mounts_and_reads_lower),
		"overlayfs mounts and reads lower files");
	orlix_test_result(
		child_exits_successfully(overlayfs_copy_up_preserves_lower),
		"overlayfs copy-up preserves lower files");
	orlix_test_result(
		child_exits_successfully(overlayfs_unlink_hides_lower_file),
		"overlayfs unlink hides lower files");

	orlix_test_exit();
}
