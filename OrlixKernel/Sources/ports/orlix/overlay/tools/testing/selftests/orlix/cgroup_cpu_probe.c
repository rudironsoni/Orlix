// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define CGROUP_ROOT "/sys/fs/cgroup"
#define CGROUP_CHILD CGROUP_ROOT "/orlix-cgroup-cpu-probe"

static bool file_contains(const char *path, const char *needle)
{
	char buffer[1024];
	size_t size = 0;

	return orlix_read_file(path, buffer, sizeof(buffer), &size) == 0 &&
	       orlix_contains(buffer, size, needle);
}

static bool write_file(const char *path, const char *value)
{
	int fd;
	size_t length = orlix_strlen(value);
	ssize_t written;

	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return false;

	written = write(fd, value, length);
	close(fd);

	return written == (ssize_t)length;
}

static bool file_is_readable(const char *path)
{
	int fd = open(path, O_RDONLY | O_CLOEXEC);

	if (fd < 0)
		return false;

	close(fd);
	return true;
}

static bool ensure_child_cgroup(void)
{
	if (mkdir(CGROUP_CHILD, 0755) == 0)
		return true;

	return errno == EEXIST;
}

static void cleanup_child_cgroup(void)
{
	write_file(CGROUP_ROOT "/cgroup.procs", "0\n");
	rmdir(CGROUP_CHILD);
}

int main(void)
{
	bool child_created;

	orlix_test_plan(5);

	orlix_test_result(file_contains(CGROUP_ROOT "/cgroup.controllers", "cpu"),
			  "cgroup v2 reports cpu controller");
	orlix_test_result(write_file(CGROUP_ROOT "/cgroup.subtree_control", "+cpu\n"),
			  "cgroup v2 enables cpu controller for child cgroups");

	child_created = ensure_child_cgroup();
	orlix_test_result(child_created, "cgroup v2 cpu child cgroup can be created");
	orlix_test_result(child_created && file_is_readable(CGROUP_CHILD "/cpu.max"),
			  "cgroup v2 child exposes cpu.max");
	orlix_test_result(child_created &&
				  write_file(CGROUP_CHILD "/cpu.max", "50000 100000\n") &&
				  file_contains(CGROUP_CHILD "/cpu.max", "50000 100000"),
			  "cgroup v2 child accepts cpu.max quota period");

	cleanup_child_cgroup();
	orlix_test_exit();
}
