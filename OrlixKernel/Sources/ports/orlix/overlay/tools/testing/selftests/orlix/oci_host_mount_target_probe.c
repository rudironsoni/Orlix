// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define ORLIX_OCI_HOST_MOUNT_TARGET "/mnt/oci-host"
#define ORLIX_OCI_HOST_ROOT_FILE "/mnt/oci-host/root-file.txt"
#define ORLIX_OCI_HOST_NESTED_FILE "/mnt/oci-host/nested/deeper/nested-file.txt"
#define ORLIX_OCI_HOST_WRITE_FILE "/mnt/oci-host/oci-target-write-probe"

static bool read_file_equals(const char *path, const char *expected)
{
	char buffer[128];
	ssize_t nread;
	size_t expected_len = orlix_strlen(expected);
	int fd;

	errno = 0;
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		orlix_test_comment_uint("open errno ", (unsigned int)errno);
		return false;
	}
	errno = 0;
	nread = read(fd, buffer, sizeof(buffer) - 1);
	if (nread < 0) {
		orlix_test_comment_uint("read errno ", (unsigned int)errno);
		close(fd);
		return false;
	}
	close(fd);
	buffer[nread] = '\0';

	return (size_t)nread == expected_len &&
	       orlix_memcmp(buffer, expected, expected_len) == 0;
}

static bool target_is_directory(void)
{
	struct stat status;

	errno = 0;
	if (stat(ORLIX_OCI_HOST_MOUNT_TARGET, &status) != 0) {
		orlix_test_comment_uint("stat errno ", (unsigned int)errno);
		return false;
	}

	return S_ISDIR(status.st_mode);
}

static bool target_is_mountpoint(void)
{
	char buffer[4096];
	size_t size;

	if (orlix_read_file("/proc/self/mountinfo", buffer, sizeof(buffer), &size) != 0)
		return false;

	return orlix_contains(buffer, size, " /mnt/oci-host ");
}

static bool write_probe_file(void)
{
	int fd;
	const char payload[] = "orlix configured host mount writable\n";

	errno = 0;
	fd = open(ORLIX_OCI_HOST_WRITE_FILE,
		  O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0644);
	if (fd < 0) {
		orlix_test_comment_uint("write open errno ", (unsigned int)errno);
		return false;
	}

	if (write(fd, payload, sizeof(payload) - 1) != (ssize_t)(sizeof(payload) - 1)) {
		close(fd);
		return false;
	}

	if (fsync(fd) != 0) {
		close(fd);
		return false;
	}

	close(fd);
	return read_file_equals(ORLIX_OCI_HOST_WRITE_FILE, payload);
}

static bool write_rejected_readonly(void)
{
	int fd;

	errno = 0;
	fd = open(ORLIX_OCI_HOST_WRITE_FILE,
		  O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0644);
	if (fd >= 0) {
		close(fd);
		unlink(ORLIX_OCI_HOST_WRITE_FILE);
		return false;
	}

	orlix_test_comment_uint("readonly write errno ", (unsigned int)errno);
	return errno == EROFS;
}

int main(int argc, char **argv)
{
	bool readonly = argc > 1 && strcmp(argv[1], "--readonly") == 0;

	orlix_write_all("1..6\n");
	orlix_test_result(target_is_directory(),
			  "OCI configured host mount target is a directory");
	orlix_test_result(target_is_mountpoint(),
			  "OCI configured host mount target is a Linux mountpoint");
	orlix_test_result(read_file_equals(ORLIX_OCI_HOST_ROOT_FILE,
					   "orlix virtio-fs fixture\n"),
			  "OCI configured host mount target exposes root fixture file");
	orlix_test_result(read_file_equals(ORLIX_OCI_HOST_NESTED_FILE,
					   "nested fixture\n"),
			  "OCI configured host mount target exposes nested fixture file");
	errno = 0;
	if (access(ORLIX_OCI_HOST_ROOT_FILE, R_OK) != 0) {
		orlix_test_comment_uint("access errno ", (unsigned int)errno);
		orlix_test_result(false,
				  "OCI configured host mount target supports Linux access checks");
	} else {
		orlix_test_result(true,
				  "OCI configured host mount target supports Linux access checks");
	}
	orlix_test_result(readonly ? write_rejected_readonly() : write_probe_file(),
			  readonly ?
				  "OCI configured read-only host mount rejects writes" :
				  "OCI configured writable host mount supports Linux writeback");
	orlix_test_exit();
}
