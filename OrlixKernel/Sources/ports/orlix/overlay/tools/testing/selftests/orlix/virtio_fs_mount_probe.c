// SPDX-License-Identifier: GPL-2.0

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/xattr.h>
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

static bool build_virtiofs_tag_path(char *path, size_t path_size,
				    const char *device_name)
{
	const char prefix[] = "/sys/fs/virtiofs/";
	const char suffix[] = "/tag";
	size_t offset = 0;

	for (size_t i = 0; prefix[i] != '\0'; ++i) {
		if (offset + 1 >= path_size)
			return false;
		path[offset++] = prefix[i];
	}

	for (size_t i = 0; device_name[i] != '\0'; ++i) {
		if (offset + 1 >= path_size)
			return false;
		path[offset++] = device_name[i];
	}

	for (size_t i = 0; suffix[i] != '\0'; ++i) {
		if (offset + 1 >= path_size)
			return false;
		path[offset++] = suffix[i];
	}

	path[offset] = '\0';
	return true;
}

static bool virtiofs_tag_is_registered(void)
{
	DIR *devices = opendir("/sys/fs/virtiofs");
	struct dirent *entry;
	char path[128];

	if (!devices)
		return false;

	while ((entry = readdir(devices)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		if (!build_virtiofs_tag_path(path, sizeof(path), entry->d_name))
			continue;
		if (read_file_equals(path, ORLIX_VIRTIOFS_TAG)) {
			closedir(devices);
			return true;
		}
	}

	closedir(devices);
	return false;
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

static bool mounted_root_rejects_create_with_erofs(void)
{
	int fd;

	errno = 0;
	fd = open(ORLIX_VIRTIOFS_MOUNTPOINT "/orlix-create-probe",
		  O_WRONLY | O_CREAT | O_EXCL, 0644);
	if (fd >= 0) {
		close(fd);
		unlink(ORLIX_VIRTIOFS_MOUNTPOINT "/orlix-create-probe");
		return false;
	}

	return errno == EROFS;
}

static bool mounted_root_supports_statx(void)
{
	struct statx status;

	return syscall(SYS_statx, AT_FDCWD, ORLIX_VIRTIOFS_MOUNTPOINT,
		       AT_SYMLINK_NOFOLLOW, STATX_TYPE | STATX_MODE, &status) == 0 &&
	       (status.stx_mask & STATX_TYPE) != 0 &&
	       S_ISDIR(status.stx_mode);
}

static bool mounted_root_has_empty_xattr_list(void)
{
	errno = 0;
	return listxattr(ORLIX_VIRTIOFS_MOUNTPOINT, NULL, 0) == 0;
}

static bool mounted_root_reports_missing_xattr(void)
{
	errno = 0;
	return getxattr(ORLIX_VIRTIOFS_MOUNTPOINT, "user.orlix-missing",
			NULL, 0) < 0 &&
	       errno == ENODATA;
}

static bool build_mount_child_path(char *path, size_t path_size, const char *name)
{
	const char *prefix = ORLIX_VIRTIOFS_MOUNTPOINT "/";
	size_t offset = 0;

	while (prefix[offset] != '\0') {
		if (offset + 1 >= path_size)
			return false;
		path[offset] = prefix[offset];
		offset++;
	}

	while (*name != '\0') {
		if (offset + 1 >= path_size)
			return false;
		path[offset++] = *name++;
	}

	path[offset] = '\0';
	return true;
}

static bool build_descendant_path(
	char *path,
	size_t path_size,
	const char *parent,
	const char *name)
{
	size_t offset = 0;

	while (parent[offset] != '\0') {
		if (offset + 1 >= path_size)
			return false;
		path[offset] = parent[offset];
		offset++;
	}

	if (offset + 1 >= path_size)
		return false;
	path[offset++] = '/';

	while (*name != '\0') {
		if (offset + 1 >= path_size)
			return false;
		path[offset++] = *name++;
	}

	path[offset] = '\0';
	return true;
}

static bool mounted_regular_file_supports_lseek(void)
{
	DIR *directory;
	struct dirent *entry;
	char path[512];
	int fd = -1;
	bool saw_regular_file = false;
	bool result = false;

	directory = opendir(ORLIX_VIRTIOFS_MOUNTPOINT);
	if (!directory)
		return false;

	while ((entry = readdir(directory)) != NULL) {
		struct stat status;

		if (entry->d_name[0] == '.')
			continue;

		if (!build_mount_child_path(path, sizeof(path), entry->d_name))
			continue;

		if (stat(path, &status) != 0 || !S_ISREG(status.st_mode))
			continue;

		saw_regular_file = true;
		fd = open(path, O_RDONLY);
		if (fd < 0)
			continue;

		errno = 0;
		result = lseek(fd, 0, SEEK_END) == status.st_size;
		close(fd);
		break;
	}

	closedir(directory);
	return !saw_regular_file || result;
}

static bool mounted_nested_directory_supports_readdir_statx(void)
{
	DIR *root;
	DIR *nested;
	struct dirent *entry;
	char path[512];
	char nested_path[512];
	bool saw_directory = false;
	bool saw_nested_entry = false;
	bool result = false;

	root = opendir(ORLIX_VIRTIOFS_MOUNTPOINT);
	if (!root)
		return false;

	while ((entry = readdir(root)) != NULL) {
		struct stat status;
		struct statx nested_status;

		if (entry->d_name[0] == '.')
			continue;
		if (!build_mount_child_path(path, sizeof(path), entry->d_name))
			continue;
		if (stat(path, &status) != 0 || !S_ISDIR(status.st_mode))
			continue;

		saw_directory = true;
		nested = opendir(path);
		if (!nested)
			break;

		while ((entry = readdir(nested)) != NULL) {
			if (entry->d_name[0] == '.')
				continue;
			if (!build_descendant_path(nested_path, sizeof(nested_path),
						   path, entry->d_name))
				continue;

			saw_nested_entry = true;
			result = access(nested_path, R_OK) == 0 &&
				 syscall(SYS_statx, AT_FDCWD, nested_path,
					 AT_SYMLINK_NOFOLLOW,
					 STATX_TYPE | STATX_MODE,
					 &nested_status) == 0 &&
				 (nested_status.stx_mask & STATX_TYPE) != 0;
			break;
		}
		closedir(nested);
		break;
	}

	closedir(root);
	return !saw_directory || !saw_nested_entry || result;
}

int main(void)
{
	bool mounted = false;

	orlix_test_plan(12);

	orlix_test_result(
		virtiofs_tag_is_registered(),
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
		orlix_test_result(mounted_root_rejects_create_with_erofs(),
				  "mounted virtio-fs root rejects create with EROFS");
		orlix_test_result(mounted_root_supports_statx(),
				  "mounted virtio-fs root supports statx");
		orlix_test_result(mounted_root_has_empty_xattr_list(),
				  "mounted virtio-fs root reports an empty xattr list");
		orlix_test_result(mounted_root_reports_missing_xattr(),
				  "mounted virtio-fs root reports missing xattrs");
		orlix_test_result(mounted_regular_file_supports_lseek(),
				  "mounted virtio-fs regular files support lseek when present");
		orlix_test_result(
			mounted_nested_directory_supports_readdir_statx(),
			"mounted virtio-fs nested paths support readdir, access, and statx when present");
		umount(ORLIX_VIRTIOFS_MOUNTPOINT);
	} else {
		orlix_test_result(false,
				  "mounted virtio-fs root is a directory");
		orlix_test_result(false,
				  "mountinfo reports the mounted virtio-fs root");
		orlix_test_result(false,
				  "mounted virtio-fs root supports readdir");
		orlix_test_result(false,
				  "mounted virtio-fs root rejects create with EROFS");
		orlix_test_result(false,
				  "mounted virtio-fs root supports statx");
		orlix_test_result(false,
				  "mounted virtio-fs root reports an empty xattr list");
		orlix_test_result(false,
				  "mounted virtio-fs root reports missing xattrs");
		orlix_test_result(false,
				  "mounted virtio-fs regular files support lseek when present");
		orlix_test_result(
			false,
			"mounted virtio-fs nested paths support readdir, access, and statx when present");
	}

	orlix_test_exit();
}
