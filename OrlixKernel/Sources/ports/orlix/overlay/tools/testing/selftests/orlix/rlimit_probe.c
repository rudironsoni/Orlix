// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#define ORLIX_RLIMIT_NOFILE_SOFT 32
#define ORLIX_RLIMIT_NOFILE_HARD 32

static void write_literal(const char *message)
{
	size_t length = 0;

	while (message[length] != '\0')
		length++;
	(void)write(STDOUT_FILENO, message, length);
}

static int expect_nofile_limit(const char *label)
{
	struct rlimit limit;

	if (getrlimit(RLIMIT_NOFILE, &limit) != 0) {
		write_literal("not ok - getrlimit RLIMIT_NOFILE failed\n");
		return 1;
	}
	if (limit.rlim_cur != ORLIX_RLIMIT_NOFILE_SOFT ||
	    limit.rlim_max != ORLIX_RLIMIT_NOFILE_HARD) {
		write_literal("not ok - unexpected RLIMIT_NOFILE value\n");
		return 1;
	}
	write_literal(label);
	write_literal("\n");
	return 0;
}

static int prove_nofile_enforcement(void)
{
	int fds[ORLIX_RLIMIT_NOFILE_HARD + 8];
	int count = 0;
	int status = 1;

	for (size_t i = 0; i < sizeof(fds) / sizeof(fds[0]); i++)
		fds[i] = -1;

	for (;;) {
		int fd = open("/dev/null", O_RDONLY | O_CLOEXEC);

		if (fd >= 0) {
			if (count < (int)(sizeof(fds) / sizeof(fds[0])))
				fds[count] = fd;
			count++;
			continue;
		}
		if (errno == EMFILE) {
			write_literal("ok - RLIMIT_NOFILE enforces EMFILE\n");
			status = 0;
		} else {
			write_literal("not ok - RLIMIT_NOFILE did not return EMFILE\n");
		}
		break;
	}

	for (size_t i = 0; i < sizeof(fds) / sizeof(fds[0]); i++) {
		if (fds[i] >= 0)
			close(fds[i]);
	}
	return status;
}

static int prove_exec_inheritance(char *self)
{
	pid_t child = fork();
	int status;

	if (child < 0) {
		write_literal("not ok - fork failed\n");
		return 1;
	}
	if (child == 0) {
		execl(self, self, "--exec-child", NULL);
		_exit(127);
	}
	if (waitpid(child, &status, 0) != child) {
		write_literal("not ok - waitpid failed\n");
		return 1;
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		write_literal("not ok - exec child did not inherit RLIMIT_NOFILE\n");
		return 1;
	}
	write_literal("ok - RLIMIT_NOFILE survives exec\n");
	return 0;
}

int main(int argc, char **argv)
{
	struct rlimit limit = {
		.rlim_cur = ORLIX_RLIMIT_NOFILE_SOFT,
		.rlim_max = ORLIX_RLIMIT_NOFILE_HARD,
	};

	if (argc == 2 && strcmp(argv[1], "--exec-child") == 0)
		return expect_nofile_limit("ok - exec child inherited RLIMIT_NOFILE");

	if (setrlimit(RLIMIT_NOFILE, &limit) != 0) {
		write_literal("not ok - setrlimit RLIMIT_NOFILE failed\n");
		return 1;
	}
	if (expect_nofile_limit("ok - setrlimit/getrlimit RLIMIT_NOFILE") != 0)
		return 1;
	if (prove_exec_inheritance(argv[0]) != 0)
		return 1;
	if (prove_nofile_enforcement() != 0)
		return 1;
	return 0;
}
