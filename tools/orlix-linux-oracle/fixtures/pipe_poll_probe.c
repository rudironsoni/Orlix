// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void print_observation(const char *name, const char *value)
{
	printf("{\"observation\":\"%s\",\"value\":\"%s\"}\n", name, value);
	fflush(stdout);
}

static int set_nonblock(int fd)
{
	int flags = fcntl(fd, F_GETFL);

	return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

static int empty_pipe_read_returns_eagain(int read_fd)
{
	char byte;

	errno = 0;
	return read(read_fd, &byte, sizeof(byte)) == -1 && errno == EAGAIN;
}

static int empty_pipe_poll_read_times_out(int read_fd)
{
	struct pollfd pfd = {
		.fd = read_fd,
		.events = POLLIN,
		.revents = 0,
	};

	return poll(&pfd, 1, 0) == 0 && pfd.revents == 0;
}

static int pipe_poll_reports_write_ready(int write_fd)
{
	struct pollfd pfd = {
		.fd = write_fd,
		.events = POLLOUT,
		.revents = 0,
	};

	return poll(&pfd, 1, 0) == 1 && (pfd.revents & POLLOUT) != 0;
}

static int pipe_poll_reports_read_ready(int read_fd)
{
	struct pollfd pfd = {
		.fd = read_fd,
		.events = POLLIN,
		.revents = 0,
	};

	return poll(&pfd, 1, 0) == 1 && (pfd.revents & POLLIN) != 0;
}

static int pipe_read_returns_payload(int read_fd)
{
	static const char expected[] = "orlix-pipe-poll\n";
	char buffer[sizeof(expected)];
	ssize_t nread;

	memset(buffer, 0, sizeof(buffer));
	nread = read(read_fd, buffer, sizeof(expected) - 1);
	return nread == (ssize_t)(sizeof(expected) - 1) &&
	       memcmp(buffer, expected, sizeof(expected) - 1) == 0;
}

static int pipe_poll_reports_hangup(int read_fd)
{
	struct pollfd pfd = {
		.fd = read_fd,
		.events = POLLIN,
		.revents = 0,
	};

	return poll(&pfd, 1, 0) == 1 && (pfd.revents & POLLHUP) != 0;
}

static int observe(const char *name, int ok)
{
	print_observation(name, ok ? "ok" : "fail");
	return ok;
}

int main(void)
{
	static const char payload[] = "orlix-pipe-poll\n";
	int fds[2] = { -1, -1 };
	int passed = 1;
	int wrote_payload;

	if (pipe(fds) == 0 && set_nonblock(fds[0])) {
		observe("pipe-created", 1);
	} else {
		observe("pipe-created", 0);
		return 1;
	}

	passed &= observe("empty-read-eagain",
			  empty_pipe_read_returns_eagain(fds[0]));
	passed &= observe("empty-poll-timeout",
			  empty_pipe_poll_read_times_out(fds[0]));
	passed &= observe("write-end-writable",
			  pipe_poll_reports_write_ready(fds[1]));

	wrote_payload = write(fds[1], payload, sizeof(payload) - 1) ==
			(ssize_t)(sizeof(payload) - 1);
	passed &= observe("read-end-readable",
			  wrote_payload && pipe_poll_reports_read_ready(fds[0]));
	passed &= observe("read-payload",
			  wrote_payload && pipe_read_returns_payload(fds[0]));

	close(fds[1]);
	fds[1] = -1;
	passed &= observe("read-end-hangup", pipe_poll_reports_hangup(fds[0]));

	if (fds[0] >= 0)
		close(fds[0]);
	if (fds[1] >= 0)
		close(fds[1]);

	return passed ? 0 : 1;
}
