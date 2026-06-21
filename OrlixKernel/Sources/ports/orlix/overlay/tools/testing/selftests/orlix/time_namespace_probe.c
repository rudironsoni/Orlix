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

static bool path_is_readable(const char *path)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC);

	if (fd < 0)
		return false;

	close(fd);
	return true;
}

static bool child_exited_successfully(pid_t child)
{
	int status;

	if (waitpid(child, &status, 0) != child)
		return false;

	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int main(void)
{
	struct stat parent_time;
	struct stat parent_time_after_unshare;
	struct stat children_time_before_unshare;
	struct stat children_time_after_unshare;
	bool unshare_prepares_child_namespace = false;
	bool forked_child_enters_child_namespace = false;
	bool time_namespace_is_readable = false;
	bool time_for_children_is_readable = false;
	bool proc_entries_are_readable;
	bool unshare_succeeds = false;
	pid_t child;

	orlix_test_plan(6);

	time_namespace_is_readable =
		stat("/proc/self/ns/time", &parent_time) == 0;
	time_for_children_is_readable =
		stat("/proc/self/ns/time_for_children",
		     &children_time_before_unshare) == 0;
	proc_entries_are_readable =
		time_namespace_is_readable && time_for_children_is_readable;

	orlix_test_result(time_namespace_is_readable,
			  "time namespace proc entry is readable");
	orlix_test_result(time_for_children_is_readable,
			  "time_for_children proc entry is readable");

	if (proc_entries_are_readable) {
		unshare_succeeds = unshare(CLONE_NEWTIME) == 0;
		orlix_test_result(unshare_succeeds,
				  "unshare CLONE_NEWTIME succeeds");

		if (unshare_succeeds &&
		    stat("/proc/self/ns/time", &parent_time_after_unshare) ==
			    0 &&
		    stat("/proc/self/ns/time_for_children",
			 &children_time_after_unshare) == 0) {
			unshare_prepares_child_namespace =
				same_inode(&parent_time,
					   &parent_time_after_unshare) &&
				!same_inode(&children_time_before_unshare,
					    &children_time_after_unshare);
		}
	} else {
		orlix_test_result(false, "unshare CLONE_NEWTIME succeeds");
	}

	orlix_test_result(unshare_prepares_child_namespace,
			  "unshare prepares time namespace for children");

	if (unshare_prepares_child_namespace) {
		child = fork();
		if (child == 0) {
			struct stat child_time;
			bool entered_child_namespace;

			entered_child_namespace =
				stat("/proc/self/ns/time", &child_time) == 0 &&
				!same_inode(&parent_time, &child_time) &&
				same_inode(&children_time_after_unshare,
					   &child_time);

			_exit(entered_child_namespace ? 0 : 1);
		}

		if (child > 0)
			forked_child_enters_child_namespace =
				child_exited_successfully(child);
	}

	orlix_test_result(forked_child_enters_child_namespace,
			  "forked child enters unshared time namespace");
	orlix_test_result(path_is_readable("/proc/self/timens_offsets"),
			  "time namespace exposes timens_offsets");

	orlix_test_exit();
}
