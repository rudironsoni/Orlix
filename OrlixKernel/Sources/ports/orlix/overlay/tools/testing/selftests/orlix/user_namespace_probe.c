// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE

#include <fcntl.h>
#include <sched.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

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

static bool proc_file_has_content(const char *path)
{
	char byte;
	int fd;
	ssize_t nread;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return false;

	nread = read(fd, &byte, 1);
	close(fd);

	return nread == 1;
}

static bool user_namespace_inode_changes_after_unshare(void)
{
	struct stat before;
	struct stat after;

	if (stat("/proc/self/ns/user", &before) != 0)
		return false;

	if (unshare(CLONE_NEWUSER) != 0)
		return false;

	if (stat("/proc/self/ns/user", &after) != 0)
		return false;

	return before.st_dev != after.st_dev || before.st_ino != after.st_ino;
}

static bool user_namespace_exposes_id_maps(void)
{
	if (unshare(CLONE_NEWUSER) != 0)
		return false;

	return proc_file_has_content("/proc/self/uid_map") &&
	       proc_file_has_content("/proc/self/gid_map");
}

static bool user_namespace_exposes_setgroups_control(void)
{
	if (unshare(CLONE_NEWUSER) != 0)
		return false;

	return access("/proc/self/setgroups", F_OK) == 0;
}

int main(void)
{
	orlix_test_plan(3);

	orlix_test_result(
		child_exits_successfully(user_namespace_inode_changes_after_unshare),
		"user namespace unshare changes /proc/self/ns/user");
	orlix_test_result(child_exits_successfully(user_namespace_exposes_id_maps),
			  "user namespace exposes uid_map and gid_map");
	orlix_test_result(
		child_exits_successfully(user_namespace_exposes_setgroups_control),
		"user namespace exposes setgroups control");

	orlix_test_exit();
}
