// SPDX-License-Identifier: GPL-2.0

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define ORLIX_VIRTIOFS_TAG "orlix-host0"
#define ORLIX_VIRTIOFS_MOUNTPOINT "/tmp/orlix-virtiofs-host"

static bool read_file_equals(const char *path, const char *expected)
{
	char buffer[64];
	ssize_t size;
	size_t expected_len;

	size = orlix_read_file(path, buffer, sizeof(buffer) - 1, false);
	if (size < 0)
		return false;

	buffer[size] = '\0';
	expected_len = orlix_strlen(expected);
	if ((size_t)size > 0 && buffer[size - 1] == '\n') {
		buffer[size - 1] = '\0';
		size--;
	}

	return (size_t)size == expected_len &&
	       orlix_memcmp(buffer, expected, expected_len) == 0;
}

static bool ensure_mountpoint(void)
{
	if (mkdir(ORLIX_VIRTIOFS_MOUNTPOINT, 0755) == 0 || errno == EEXIST)
		return true;

	return false;
}

static bool mount_host_virtiofs(void)
{
	return mount(ORLIX_VIRTIOFS_TAG, ORLIX_VIRTIOFS_MOUNTPOINT, "virtiofs",
		     0, NULL) == 0;
}

static bool mounted_root_is_directory(void)
{
	struct stat status;

	if (stat(ORLIX_VIRTIOFS_MOUNTPOINT, &status) != 0)
		return false;

	return S_ISDIR(status.st_mode);
}

static bool mounted_root_can_readdir(void)
{
	DIR *directory;
	struct dirent *entry;
	bool saw_dot = false;

	directory = opendir(ORLIX_VIRTIOFS_MOUNTPOINT);
	if (!directory)
		return false;

	while ((entry = readdir(directory)) != NULL) {
		if (entry->d_name[0] == '.' && entry->d_name[1] == '\0') {
			saw_dot = true;
			break;
		}
	}

	closedir(directory);
	return saw_dot;
}

static bool mountinfo_reports_virtiofs(void)
{
	char buffer[4096];
	ssize_t size;

	size = orlix_read_file("/proc/self/mountinfo", buffer,
			       sizeof(buffer), false);
	if (size <= 0)
		return false;

	return orlix_contains(buffer, (size_t)size, ORLIX_VIRTIOFS_MOUNTPOINT) &&
	       orlix_contains(buffer, (size_t)size, " - virtiofs ");
}

int main(void)
{
	bool mounted = false;

	orlix_test_plan(6);

	orlix_test_result(
		read_file_equals("/sys/fs/virtiofs/virtio0/tag", ORLIX_VIRTIOFS_TAG),
		"virtio-fs device exposes the standard Orlix host-folder tag");
	orlix_test_result(ensure_mountpoint(),
			  "virtio-fs mountpoint is available");

	mounted = mount_host_virtiofs();
	orlix_test_result(mounted,
			  "Linux mounts the Orlix host folder through virtio-fs");

	if (mounted) {
		orlix_test_result(mounted_root_is_directory(),
				  "mounted virtio-fs root is a directory");
		orlix_test_result(mountinfo_reports_virtiofs(),
				  "mountinfo reports the mounted virtio-fs root");
		orlix_test_result(mounted_root_can_readdir(),
				  "mounted virtio-fs root supports readdir");
		umount(ORLIX_VIRTIOFS_MOUNTPOINT);
	} else {
		orlix_test_result(false,
				  "mounted virtio-fs root is a directory");
		orlix_test_result(false,
				  "mountinfo reports the mounted virtio-fs root");
		orlix_test_result(false,
				  "mounted virtio-fs root supports readdir");
	}

	orlix_test_exit();
}
