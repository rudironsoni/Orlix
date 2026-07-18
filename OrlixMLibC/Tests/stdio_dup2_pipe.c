// SPDX-License-Identifier: MIT

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
	static const char expected[] = "pipe output";
	char path[] = "/tmp/mlibc-stdio-dup2-XXXXXX";
	char observed[sizeof(expected)] = {};
	int stdout_copy;
	int pipe_fds[2];
	int file_fd;
	ssize_t size;
	int failed = 0;

	stdout_copy = dup(STDOUT_FILENO);
	file_fd = mkstemp(path);
	if (stdout_copy < 0 || file_fd < 0)
		return 1;
	unlink(path);

	if (setvbuf(stdout, NULL, _IOFBF, BUFSIZ))
		return 1;
	if (dup2(file_fd, STDOUT_FILENO) < 0)
		return 1;
	close(file_fd);
	if (fputs("file output", stdout) < 0 || fflush(stdout))
		return 1;

	if (pipe(pipe_fds) || dup2(pipe_fds[1], STDOUT_FILENO) < 0)
		return 1;
	close(pipe_fds[1]);
	if (fputs(expected, stdout) < 0 || fflush(stdout))
		failed = 1;

	if (dup2(stdout_copy, STDOUT_FILENO) < 0)
		return 1;
	close(stdout_copy);
	size = read(pipe_fds[0], observed, sizeof(observed));
	close(pipe_fds[0]);

	if (size != (ssize_t)(sizeof(expected) - 1))
		failed = 1;
	if (memcmp(observed, expected, sizeof(expected) - 1))
		failed = 1;
	return failed;
}
