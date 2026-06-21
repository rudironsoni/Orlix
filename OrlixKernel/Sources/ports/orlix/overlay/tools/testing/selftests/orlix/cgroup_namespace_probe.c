// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE

#include <fcntl.h>
#include <linux/sched.h>
#include <sched.h>
#include <stdbool.h>
#include <stddef.h>
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

static bool proc_file_contains(const char *path, const char *needle)
{
	char buffer[512];
	ssize_t nread;
	int fd;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return false;

	nread = read(fd, buffer, sizeof(buffer) - 1);
	close(fd);

	if (nread <= 0)
		return false;

	buffer[nread] = '\0';
	return orlix_contains(buffer, (size_t)nread, needle);
}

static bool cgroup_namespace_inode_changes_after_unshare(void)
{
	struct stat before;
	struct stat after;

	if (stat("/proc/self/ns/cgroup", &before) != 0)
		return false;

	if (unshare(CLONE_NEWCGROUP) != 0)
		return false;

	if (stat("/proc/self/ns/cgroup", &after) != 0)
		return false;

	return before.st_dev != after.st_dev || before.st_ino != after.st_ino;
}

static bool cgroup_namespace_keeps_proc_cgroup_readable(void)
{
	if (unshare(CLONE_NEWCGROUP) != 0)
		return false;

	return proc_file_contains("/proc/self/cgroup", "0::/");
}

int main(void)
{
	orlix_test_plan(2);

	orlix_test_result(
		child_exits_successfully(cgroup_namespace_inode_changes_after_unshare),
		"cgroup namespace unshare changes /proc/self/ns/cgroup");
	orlix_test_result(
		child_exits_successfully(cgroup_namespace_keeps_proc_cgroup_readable),
		"cgroup namespace keeps /proc/self/cgroup readable");

	orlix_test_exit();
}
