// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define DEVICE_DIR "/tmp/orlix-device-node-probe"
#define NULL_NODE DEVICE_DIR "/null"
#define FIFO_NODE DEVICE_DIR "/pipe"

static bool path_is_character_device(const char *path, unsigned int major_number,
				     unsigned int minor_number)
{
	struct stat st;

	if (stat(path, &st) != 0 || !S_ISCHR(st.st_mode))
		return false;
	return major(st.st_rdev) == major_number &&
	       minor(st.st_rdev) == minor_number;
}

static bool path_is_fifo(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 && S_ISFIFO(st.st_mode);
}

int main(void)
{
	struct stat st;

	orlix_test_plan(4);

	(void)unlink(NULL_NODE);
	(void)unlink(FIFO_NODE);
	(void)rmdir(DEVICE_DIR);

	orlix_test_result(mkdir(DEVICE_DIR, 0755) == 0,
			  "device node directory can be created");
	orlix_test_result(mknod(NULL_NODE, S_IFCHR | 0666, makedev(1, 3)) == 0 &&
				  path_is_character_device(NULL_NODE, 1, 3),
			  "Linux mknod creates OCI character device nodes");
	orlix_test_result(mkfifo(FIFO_NODE, 0644) == 0 && path_is_fifo(FIFO_NODE),
			  "Linux mkfifo creates OCI fifo device nodes");
	orlix_test_result(chown(NULL_NODE, 0, 0) == 0 &&
				  chmod(NULL_NODE, 0600) == 0 &&
				  stat(NULL_NODE, &st) == 0 &&
				  (st.st_mode & 07777) == 0600 &&
				  st.st_uid == 0 && st.st_gid == 0,
			  "Linux chown and chmod apply OCI device ownership and mode");

	(void)unlink(NULL_NODE);
	(void)unlink(FIFO_NODE);
	(void)rmdir(DEVICE_DIR);

	orlix_test_exit();
	return 0;
}
