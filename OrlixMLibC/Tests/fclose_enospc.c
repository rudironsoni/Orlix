// SPDX-License-Identifier: MIT

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
	FILE *file;
	char buf[64];
	int rc;

	if (access("/dev/full", W_OK) != 0)
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
