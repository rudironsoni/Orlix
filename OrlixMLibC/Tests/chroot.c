/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
	static const char root[] = "/tmp/mlibc-chroot-test";
	static const char payload[] = "inside\n";
	char buffer[sizeof(payload)] = { 0 };
	int fd;

	errno = 0;
	if (chroot("/tmp/mlibc-chroot-missing") != -1 || errno != ENOENT)
		return 1;

	if (mkdir(root, 0755) < 0 && errno != EEXIST)
		return 2;

	fd = open("/tmp/mlibc-chroot-test/sentinel", O_WRONLY | O_CREAT | O_TRUNC,
		  0644);
	if (fd < 0)
		return 3;
	if (write(fd, payload, sizeof(payload)) != sizeof(payload)) {
		close(fd);
		return 4;
	}
	if (close(fd) < 0)
		return 5;

	if (chroot(root) < 0)
		return 6;
	if (chdir("/") < 0)
		return 7;

	fd = open("/sentinel", O_RDONLY);
	if (fd < 0)
		return 8;
	if (read(fd, buffer, sizeof(buffer)) != sizeof(payload)) {
		close(fd);
		return 9;
	}
	if (close(fd) < 0)
		return 10;
	if (memcmp(buffer, payload, sizeof(payload)) != 0)
		return 11;

	return 0;
}
