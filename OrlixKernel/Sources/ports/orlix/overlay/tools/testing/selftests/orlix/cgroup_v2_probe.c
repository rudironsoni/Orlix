// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define CGROUP_ROOT "/sys/fs/cgroup"
#define CGROUP_CHILD CGROUP_ROOT "/orlix-cgroup-v2-probe"

static bool file_is_readable(const char *path)
{
	char buffer[512];
	size_t size = 0;

	return orlix_read_file(path, buffer, sizeof(buffer), &size) == 0;
}

static bool proc_self_cgroup_has_v2_root(void)
{
	char buffer[256];
	size_t size = 0;

	return orlix_read_file("/proc/self/cgroup", buffer, sizeof(buffer),
			       &size) == 0 &&
	       orlix_contains(buffer, size, "0::/");
}

static bool mountinfo_has_cgroup2_root(void)
{
	char buffer[4096];
	size_t size = 0;

	return orlix_read_file("/proc/self/mountinfo", buffer, sizeof(buffer),
			       &size) == 0 &&
	       orlix_contains(buffer, size, " /sys/fs/cgroup ") &&
	       orlix_contains(buffer, size, " - cgroup2 cgroup2 ");
}

static bool write_string_to_file(const char *path, const char *data, size_t len)
{
	ssize_t written;
	int fd;

	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return false;

	written = write(fd, data, len);
	close(fd);

	return written == (ssize_t)len;
}

static bool cgroup_procs_accepts_self(const char *path)
{
	return write_string_to_file(path, "0\n", 2);
}

static bool child_cgroup_can_be_created(void)
{
	if (mkdir(CGROUP_CHILD, 0755) == 0)
		return true;
	if (errno == EEXIST)
		return true;

	return false;
}

static bool child_cgroup_procs_accepts_self(void)
{
	return cgroup_procs_accepts_self(CGROUP_CHILD "/cgroup.procs");
}

static bool self_can_return_to_root_cgroup(void)
{
	return cgroup_procs_accepts_self(CGROUP_ROOT "/cgroup.procs");
}

static bool child_cgroup_can_be_removed(void)
{
	if (rmdir(CGROUP_CHILD) == 0)
		return true;
	if (errno == ENOENT)
		return true;

	return false;
}

int main(void)
{
	bool child_created;
	bool moved_to_child;
	bool moved_to_root;

	orlix_test_plan(9);

	orlix_test_result(proc_self_cgroup_has_v2_root(),
			  "proc self cgroup reports v2 root");
	orlix_test_result(mountinfo_has_cgroup2_root(),
			  "mountinfo reports cgroup2 at /sys/fs/cgroup");
	orlix_test_result(file_is_readable(CGROUP_ROOT "/cgroup.controllers"),
			  "cgroup v2 controllers file is readable");
	orlix_test_result(file_is_readable(CGROUP_ROOT "/cgroup.subtree_control"),
			  "cgroup v2 subtree control file is readable");
	orlix_test_result(file_is_readable(CGROUP_ROOT "/cgroup.procs"),
			  "cgroup v2 procs file is readable");
	orlix_test_result(cgroup_procs_accepts_self(CGROUP_ROOT "/cgroup.procs"),
			  "cgroup v2 procs accepts current task at root");

	child_created = child_cgroup_can_be_created();
	orlix_test_result(child_created,
			  "cgroup v2 child cgroup directory can be created");

	moved_to_child = child_created && child_cgroup_procs_accepts_self();
	orlix_test_result(moved_to_child,
			  "cgroup v2 child cgroup accepts current task");

	moved_to_root = self_can_return_to_root_cgroup();
	orlix_test_result(moved_to_root && child_cgroup_can_be_removed(),
			  "cgroup v2 empty child cgroup can be removed");

	orlix_test_exit();
}
