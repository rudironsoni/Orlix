// SPDX-License-Identifier: GPL-2.0
#include <stdbool.h>
#include <stddef.h>

#include "orlix_kselftest_user.h"

#define CGROUP_PATH "/orlix/oci-cgroup-runtime-proof"
#define CGROUP_ROOT "/sys/fs/cgroup"
#define CGROUP_DIR CGROUP_ROOT CGROUP_PATH

static bool file_contains(const char *path, const char *needle)
{
	char buffer[1024];
	size_t size = 0;

	return orlix_read_file(path, buffer, sizeof(buffer), &size) == 0 &&
	       orlix_contains(buffer, size, needle);
}

int main(void)
{
	orlix_test_plan(6);

	orlix_test_result(
		file_contains("/proc/self/cgroup", "0::" CGROUP_PATH),
		"OCI cgroupsPath moves process into configured cgroup");
	orlix_test_result(
		file_contains(CGROUP_DIR "/pids.max", "64"),
		"OCI pids limit writes pids.max");
	orlix_test_result(
		file_contains(CGROUP_DIR "/cpu.max", "50000 100000"),
		"OCI CPU quota period writes cpu.max");
	orlix_test_result(
		file_contains(CGROUP_DIR "/cpu.weight", "100"),
		"OCI unified cgroup write applies cpu.weight");
	orlix_test_result(
		file_contains(CGROUP_DIR "/memory.max", "268435456"),
		"OCI memory limit writes memory.max");
	orlix_test_result(
		file_contains(CGROUP_DIR "/io.weight", "default 100"),
		"OCI blockIO weight writes io.weight");

	orlix_test_exit();
	return 0;
}
