// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define ORLIX_OCI_MASKED_FILE "/etc/os-release"
#define ORLIX_OCI_READONLY_DIR "/root"
#define ORLIX_OCI_READONLY_WRITE_FILE "/root/orlix-oci-readonly-write-probe"
#define ORLIX_OCI_TMPFS_DIR "/mnt/oci-tmpfs"
#define ORLIX_OCI_TMPFS_FILE "/mnt/oci-tmpfs/orlix-oci-tmpfs-write-probe"

static bool masked_file_is_hidden(void)
{
	char byte;
	ssize_t nread;
	int fd;

	errno = 0;
	fd = open(ORLIX_OCI_MASKED_FILE, O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		orlix_test_comment_uint("open masked file errno ",
					(unsigned int)errno);
		return errno == EACCES;
	}

	errno = 0;
	nread = read(fd, &byte, sizeof(byte));
	if (nread < 0)
		orlix_test_comment_uint("read masked file errno ",
					(unsigned int)errno);
	close(fd);
	return nread == 0;
}

static bool readonly_directory_rejects_create(void)
{
	int fd;

	(void)unlink(ORLIX_OCI_READONLY_WRITE_FILE);
	errno = 0;
	fd = open(ORLIX_OCI_READONLY_WRITE_FILE,
		  O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (fd >= 0) {
		close(fd);
		(void)unlink(ORLIX_OCI_READONLY_WRITE_FILE);
		return false;
	}

	orlix_test_comment_uint("readonly create errno ", (unsigned int)errno);
	return errno == EROFS;
}

static bool tmpfs_target_is_directory(void)
{
	struct stat status;

	errno = 0;
	if (stat(ORLIX_OCI_TMPFS_DIR, &status) != 0) {
		orlix_test_comment_uint("stat tmpfs target errno ",
					(unsigned int)errno);
		return false;
	}
	return S_ISDIR(status.st_mode);
}

static bool mountinfo_reports_tmpfs_target(void)
{
	char buffer[16384];
	ssize_t nread;
	int fd;

	errno = 0;
	fd = open("/proc/self/mountinfo", O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		orlix_test_comment_uint("open mountinfo errno ",
					(unsigned int)errno);
		return false;
	}

	nread = read(fd, buffer, sizeof(buffer));
	close(fd);
	if (nread <= 0) {
		orlix_test_comment_uint("read mountinfo errno ",
					(unsigned int)errno);
		return false;
	}

	return orlix_contains(buffer, (size_t)nread,
			      " " ORLIX_OCI_TMPFS_DIR " ") &&
	       orlix_contains(buffer, (size_t)nread, " - tmpfs tmpfs ");
}

static bool tmpfs_supports_linux_write_readback(void)
{
	static const char payload[] = "orlix oci tmpfs writeback\n";
	char buffer[sizeof(payload)];
	ssize_t nread;
	int fd;

	(void)unlink(ORLIX_OCI_TMPFS_FILE);
	errno = 0;
	fd = open(ORLIX_OCI_TMPFS_FILE,
		  O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if (fd < 0) {
		orlix_test_comment_uint("create tmpfs file errno ",
					(unsigned int)errno);
		return false;
	}

	if (write(fd, payload, sizeof(payload) - 1) !=
	    (ssize_t)(sizeof(payload) - 1)) {
		orlix_test_comment_uint("write tmpfs file errno ",
					(unsigned int)errno);
		close(fd);
		return false;
	}

	if (lseek(fd, 0, SEEK_SET) != 0) {
		orlix_test_comment_uint("seek tmpfs file errno ",
					(unsigned int)errno);
		close(fd);
		return false;
	}

	nread = read(fd, buffer, sizeof(buffer));
	close(fd);
	return nread == (ssize_t)(sizeof(payload) - 1) &&
	       orlix_memcmp(buffer, payload, sizeof(payload) - 1) == 0;
}

int main(void)
{
	orlix_write_all("ORLIX-OCI-ROOTFS-CONTROLS-PROBE\n");
	orlix_write_all("1..5\n");
	orlix_test_result(masked_file_is_hidden(),
			  "OCI maskedPaths masks configured rootfs file");
	orlix_test_result(readonly_directory_rejects_create(),
			  "OCI readonlyPaths remount rejects writes");
	orlix_test_result(tmpfs_target_is_directory(),
			  "OCI tmpfs mount creates Linux directory target");
	orlix_test_result(mountinfo_reports_tmpfs_target(),
			  "OCI tmpfs mount appears in Linux mountinfo");
	orlix_test_result(tmpfs_supports_linux_write_readback(),
			  "OCI tmpfs mount supports Linux write readback");
	orlix_test_exit();
}
