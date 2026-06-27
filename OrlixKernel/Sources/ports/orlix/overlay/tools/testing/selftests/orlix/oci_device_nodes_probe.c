// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define NULL_NODE "/dev/orlix-oci-null"
#define FIFO_NODE "/dev/orlix-oci-pipe"
#define BLOCK_NODE "/dev/orlix-oci-block"

static bool path_mode_has_permissions(const struct stat *st, mode_t mode)
{
	return (st->st_mode & 07777) == mode;
}

static bool path_is_character_device(const char *path, unsigned int major_number,
				     unsigned int minor_number, mode_t mode)
{
	struct stat st;

	return stat(path, &st) == 0 && S_ISCHR(st.st_mode) &&
	       major(st.st_rdev) == major_number &&
	       minor(st.st_rdev) == minor_number &&
	       path_mode_has_permissions(&st, mode) && st.st_uid == 0 &&
	       st.st_gid == 0;
}

static bool path_is_block_device(const char *path, unsigned int major_number,
				 unsigned int minor_number, mode_t mode)
{
	struct stat st;

	return stat(path, &st) == 0 && S_ISBLK(st.st_mode) &&
	       major(st.st_rdev) == major_number &&
	       minor(st.st_rdev) == minor_number &&
	       path_mode_has_permissions(&st, mode) && st.st_uid == 0 &&
	       st.st_gid == 0;
}

static bool path_is_fifo(const char *path, mode_t mode)
{
	struct stat st;

	return stat(path, &st) == 0 && S_ISFIFO(st.st_mode) &&
	       path_mode_has_permissions(&st, mode) && st.st_uid == 0 &&
	       st.st_gid == 0;
}

static bool null_device_discards_io(void)
{
	char byte = 0x7f;
	int fd = open(NULL_NODE, O_RDWR | O_CLOEXEC);
	bool result;

	if (fd < 0)
		return false;

	errno = 0;
	result = write(fd, &byte, 1) == 1 && read(fd, &byte, 1) == 0;
	close(fd);
	return result;
}

int main(void)
{
	orlix_test_plan(4);

	orlix_test_result(path_is_character_device(NULL_NODE, 1, 3, 0666),
			  "OCI linux.devices creates configured character device node");
	orlix_test_result(null_device_discards_io(),
			  "OCI character device node uses Linux device behavior");
	orlix_test_result(path_is_fifo(FIFO_NODE, 0644),
			  "OCI linux.devices creates configured fifo device node");
	orlix_test_result(path_is_block_device(BLOCK_NODE, 7, 0, 0600),
			  "OCI linux.devices creates configured block device node");

	orlix_test_exit();
	return 0;
}
