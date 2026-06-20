// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void print_observation(const char *name, const char *value)
{
	printf("{\"observation\":\"%s\",\"value\":\"%s\"}\n", name, value);
	fflush(stdout);
}

static int parse_uint(const char *s)
{
	int value = 0;

	if (!s || !s[0])
		return -1;
	while (*s) {
		if (*s < '0' || *s > '9')
			return -1;
		value = value * 10 + (*s - '0');
		s++;
	}
	return value;
}

static int exec_child_main(const char *read_fd_text, const char *cloexec_fd_text)
{
	static const char expected[] = "orlix-fd-inherit\n";
	char buffer[sizeof(expected)];
	int read_fd = parse_uint(read_fd_text);
	int cloexec_fd = parse_uint(cloexec_fd_text);
	ssize_t nread;
	int passed = 1;

	print_observation("child-started", "ok");
	if (read_fd < 0 || cloexec_fd < 0)
		return 80;

	memset(buffer, 0, sizeof(buffer));
	nread = read(read_fd, buffer, sizeof(expected) - 1);
	if (nread == (ssize_t)(sizeof(expected) - 1) &&
	    memcmp(buffer, expected, sizeof(expected) - 1) == 0) {
		print_observation("inherited-read", "ok");
	} else {
		print_observation("inherited-read", "fail");
		passed = 0;
	}

	errno = 0;
	if (fcntl(cloexec_fd, F_GETFD) == -1 && errno == EBADF) {
		print_observation("cloexec-ebadf", "ok");
	} else {
		print_observation("cloexec-ebadf", "fail");
		passed = 0;
	}

	return passed ? 0 : 1;
}

static void format_uint(char *buffer, size_t capacity, unsigned int value)
{
	snprintf(buffer, capacity, "%u", value);
}

static int run_exec_child(const char *self_path, int read_fd, int cloexec_fd)
{
	char read_fd_arg[16];
	char cloexec_fd_arg[16];
	pid_t child;
	int status;

	format_uint(read_fd_arg, sizeof(read_fd_arg), (unsigned int)read_fd);
	format_uint(cloexec_fd_arg, sizeof(cloexec_fd_arg), (unsigned int)cloexec_fd);

	child = fork();
	if (child == 0) {
		char *const exec_argv[] = {
			(char *)self_path,
			(char *)"--exec-child",
			read_fd_arg,
			cloexec_fd_arg,
			NULL
		};

		execv(self_path, exec_argv);
		_exit(127);
	}
	if (child < 0)
		return -1;
	if (waitpid(child, &status, 0) != child)
		return -1;
	if (!WIFEXITED(status))
		return -1;
	return WEXITSTATUS(status);
}

int main(int argc, char **argv)
{
	static const char payload[] = "orlix-fd-inherit\n";
	int inherited_pipe[2] = { -1, -1 };
	int cloexec_pipe[2] = { -1, -1 };
	int flags;
	int child_status = -1;

	if (argc == 4 && strcmp(argv[1], "--exec-child") == 0)
		return exec_child_main(argv[2], argv[3]);

	if (pipe(inherited_pipe) == 0 && pipe(cloexec_pipe) == 0) {
		print_observation("pipe-created", "ok");
	} else {
		print_observation("pipe-created", "fail");
		return 1;
	}

	flags = fcntl(inherited_pipe[0], F_GETFD);
	if (flags >= 0 && (flags & FD_CLOEXEC) == 0) {
		print_observation("fd-without-cloexec", "ok");
	} else {
		print_observation("fd-without-cloexec", "fail");
		return 1;
	}

	flags = fcntl(cloexec_pipe[0], F_GETFD);
	if (flags >= 0 && fcntl(cloexec_pipe[0], F_SETFD, flags | FD_CLOEXEC) == 0) {
		flags = fcntl(cloexec_pipe[0], F_GETFD);
	}
	if (flags >= 0 && (flags & FD_CLOEXEC) != 0) {
		print_observation("fd-marked-cloexec", "ok");
	} else {
		print_observation("fd-marked-cloexec", "fail");
		return 1;
	}

	if (write(inherited_pipe[1], payload, sizeof(payload) - 1) !=
	    (ssize_t)(sizeof(payload) - 1)) {
		return 1;
	}
	close(inherited_pipe[1]);
	close(cloexec_pipe[1]);

	child_status = run_exec_child(argv[0], inherited_pipe[0], cloexec_pipe[0]);
	if (child_status == 0) {
		print_observation("exec-child-exit", "ok");
	} else {
		print_observation("exec-child-exit", "fail");
		return 1;
	}

	close(inherited_pipe[0]);
	close(cloexec_pipe[0]);
	return 0;
}
