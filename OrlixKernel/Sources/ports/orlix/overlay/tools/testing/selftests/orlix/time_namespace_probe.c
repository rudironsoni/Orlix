// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE

#include <fcntl.h>
#include <linux/sched.h>
#include <sched.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

static bool same_inode(const struct stat *left, const struct stat *right)
{
	return left->st_dev == right->st_dev && left->st_ino == right->st_ino;
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

static bool child_time_namespace_differs_from(const struct stat *before)
{
	struct stat child_time;

	if (stat("/proc/self/ns/time", &child_time) != 0)
		return false;

	return !same_inode(before, &child_time);
}

static bool forked_child_enters_new_time_namespace(void)
{
	struct stat before;
	struct stat after_for_children;
	pid_t child;
	int status;

	if (stat("/proc/self/ns/time", &before) != 0)
		return false;

	if (unshare(CLONE_NEWTIME) != 0)
		return false;

	if (stat("/proc/self/ns/time_for_children", &after_for_children) != 0)
		return false;

	if (same_inode(&before, &after_for_children))
		return false;

	child = fork();
	if (child < 0)
		return false;

	if (child == 0)
		_exit(child_time_namespace_differs_from(&before) ? 0 : 1);

	if (waitpid(child, &status, 0) != child)
		return false;

	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool time_namespace_exposes_offsets_file(void)
{
	if (unshare(CLONE_NEWTIME) != 0)
		return false;

	return proc_file_has_content("/proc/self/timens_offsets");
}

int main(void)
{
	orlix_test_plan(2);

	orlix_test_result(child_exits_successfully(
				  forked_child_enters_new_time_namespace),
			  "forked child enters unshared time namespace");
	orlix_test_result(
		child_exits_successfully(time_namespace_exposes_offsets_file),
			  "time namespace exposes timens_offsets");

	orlix_test_exit();
}
