// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define CGROUP_ROOT "/sys/fs/cgroup"
#define CGROUP_CHILD CGROUP_ROOT "/orlix-cgroup-io-probe"

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

static bool read_vdb_device(char *buffer, size_t buffer_size)
{
	size_t size = 0;

	if (orlix_read_file("/sys/block/vdb/dev", buffer, buffer_size, &size) != 0)
		return false;
	if (size == 0 || size >= buffer_size)
		return false;
	if (buffer[size - 1] == '\n')
		size--;
	buffer[size] = '\0';
	return size > 0;
}

static bool build_control_value(char *buffer, size_t buffer_size,
				const char *device, const char *suffix)
{
	size_t device_size = orlix_strlen(device);
	size_t suffix_size = orlix_strlen(suffix);

	if (device_size + 1 + suffix_size + 1 > buffer_size)
		return false;
	for (size_t i = 0; i < device_size; i++)
		buffer[i] = device[i];
	buffer[device_size] = ' ';
	for (size_t i = 0; i < suffix_size; i++)
		buffer[device_size + 1 + i] = suffix[i];
	buffer[device_size + 1 + suffix_size] = '\n';
	buffer[device_size + 1 + suffix_size + 1] = '\0';
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
	bool device_read;
	bool device_weight_written = false;
	bool device_weight_visible = false;
	char device[32];
	char weight[64];
	char max[64];
	char qos[64];

	orlix_test_plan(9);
	orlix_test_result(file_contains(CGROUP_ROOT "/cgroup.controllers", "io"),
			  "cgroup v2 reports io controller");
	orlix_test_result(write_file(CGROUP_ROOT "/cgroup.subtree_control",
				     "+io\n"),
			  "io controller can be enabled for child cgroups");

	child_created = ensure_child_cgroup();
	orlix_test_result(child_created, "child cgroup can be created");
	orlix_test_result(child_created &&
				  file_is_readable(CGROUP_CHILD "/io.weight") &&
				  file_is_readable(CGROUP_CHILD "/io.max"),
			  "child cgroup exposes io.weight and io.max");
	orlix_test_result(child_created &&
				  write_file(CGROUP_CHILD "/io.weight",
					     "default 100\n") &&
				  file_contains(CGROUP_CHILD "/io.weight",
						"default 100"),
			  "io.weight accepts OCI blockIO default weight");

	device_read = read_vdb_device(device, sizeof(device));
	orlix_test_result(device_read, "writable state block device major:minor is readable");
	if (device_read)
		device_read = build_control_value(weight, sizeof(weight), device,
					  "200") &&
			build_control_value(max, sizeof(max), device,
					    "rbps=1048576") &&
			build_control_value(qos, sizeof(qos), device,
					    "enable=1");
	orlix_test_result(device_read &&
			  write_file(CGROUP_ROOT "/io.cost.qos", qos),
			  "io.cost.qos enables per-device IO cost control");
	if (child_created && device_read) {
		errno = 0;
		device_weight_written = write_file(CGROUP_CHILD "/io.weight",
						  weight);
		if (!device_weight_written)
			orlix_test_comment_uint("io.weight write errno ",
						(unsigned int)errno);
		if (device_weight_written) {
			device_weight_visible = file_contains(
				CGROUP_CHILD "/io.weight", device);
			if (!device_weight_visible)
				orlix_test_comment("io.weight missing device ",
						   device, orlix_strlen(device));
		}
	}
	orlix_test_result(child_created && device_read &&
			  device_weight_written && device_weight_visible,
			  "io.weight accepts OCI blockIO device weight");
	orlix_test_result(child_created && device_read &&
				  write_file(CGROUP_CHILD "/io.max", max) &&
				  file_contains(CGROUP_CHILD "/io.max",
						"rbps=1048576"),
			  "io.max accepts OCI blockIO device throttle");

	if (child_created)
		cleanup_child_cgroup();
	orlix_test_exit();
}
