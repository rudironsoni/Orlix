// SPDX-License-Identifier: MIT

#include <errno.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

static int ensure_dev_full(void)
{
	struct stat st;

	if (stat("/dev/full", &st) == 0 && S_ISCHR(st.st_mode))
		return 0;
	(void)mkdir("/dev", 0755);
	(void)mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
	if (stat("/dev/full", &st) == 0 && S_ISCHR(st.st_mode))
		return 0;
	if (mknod("/dev/full", S_IFCHR | 0666, makedev(1, 7)) != 0)
		return -1;
	if (stat("/dev/full", &st) != 0 || !S_ISCHR(st.st_mode))
		return -1;
	return 0;
}

int main(void)
{
	FILE *file;
	char buf[64];
	int rc;

	if (ensure_dev_full() != 0)
		return 2;

	file = fopen("/dev/full", "w");
	if (file == NULL)
		return 3;
	if (setvbuf(file, buf, _IOFBF, sizeof(buf)))
		return 4;
	if (fwrite("0123456789\n", 1, 11, file) != 11)
		return 5;
	errno = 0;
	rc = fclose(file);
	if (rc != EOF)
		return 6;
	if (errno != ENOSPC)
		return 7;
	return 0;
}
