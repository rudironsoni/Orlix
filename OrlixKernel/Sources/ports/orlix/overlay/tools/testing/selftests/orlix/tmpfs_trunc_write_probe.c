// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define PROBE_DIR "/tmp/orlix-tmpfs-trunc-write"
#define EXCL_PATH PROBE_DIR "/excl.tmp"
#define TRUNC_PATH PROBE_DIR "/trunc.tmp"
#define PAYLOAD "orlix-tmpfs-trunc-write"

static void stage(const char *name)
{
	orlix_write_all("# tmpfs_trunc_write ");
	orlix_write_all(name);
	orlix_write_all("\n");
}

static bool write_all(int fd, const char *data, size_t len)
{
	while (len > 0) {
		ssize_t n = write(fd, data, len);

		if (n <= 0)
			return false;
		data += n;
		len -= (size_t)n;
	}
	return true;
}

static bool setup_tmpfs(void)
{
	stage("mkdir");
	if (mkdir(PROBE_DIR, 0755) < 0 && errno != EEXIST)
		return false;
	stage("mount");
	if (mount("tmpfs", PROBE_DIR, "tmpfs", 0, "mode=0755") < 0 &&
	    errno != EBUSY)
		return false;
	return true;
}

static bool create_excl_file(void)
{
	int fd;

	stage("open excl");
	fd = open(EXCL_PATH, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
	if (fd < 0)
		return false;
	stage("close excl");
	return close(fd) == 0;
}

static bool trunc_write_read_back(void)
{
	char buffer[sizeof(PAYLOAD)];
	ssize_t nread;
	int fd;

	stage("open trunc");
	fd = open(TRUNC_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
	if (fd < 0)
		return false;
	stage("write");
	if (!write_all(fd, PAYLOAD, sizeof(PAYLOAD) - 1)) {
		close(fd);
		return false;
	}
	stage("close trunc");
	if (close(fd) != 0)
		return false;

	stage("open read");
	fd = open(TRUNC_PATH, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;
	nread = read(fd, buffer, sizeof(buffer));
	close(fd);
	stage("read done");
	return nread == (ssize_t)(sizeof(PAYLOAD) - 1) &&
	       orlix_memcmp(buffer, PAYLOAD, sizeof(PAYLOAD) - 1) == 0;
}

int main(void)
{
	orlix_test_plan(3);
	orlix_test_result(setup_tmpfs(), "tmpfs mounted for trunc/write probe");
	orlix_test_result(create_excl_file(),
			  "O_CREAT|O_EXCL create on tmpfs completes");
	orlix_test_result(trunc_write_read_back(),
			  "O_TRUNC write and read-back on tmpfs complete");
	orlix_test_exit();
}
