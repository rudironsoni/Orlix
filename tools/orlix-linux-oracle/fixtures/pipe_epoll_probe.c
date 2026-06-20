// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#if defined(__linux__)
#include <sys/epoll.h>
#else
#define EPOLLIN 0x001
#define EPOLLOUT 0x004
#define EPOLLHUP 0x010
#define EPOLL_CLOEXEC 02000000
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3

typedef union epoll_data {
	void *ptr;
	int fd;
	uint32_t u32;
	uint64_t u64;
} epoll_data_t;

struct epoll_event {
	uint32_t events;
	epoll_data_t data;
};

static int epoll_create1(int flags)
{
	(void)flags;
	return -1;
}

static int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event)
{
	(void)epfd;
	(void)op;
	(void)fd;
	(void)event;
	return -1;
}

static int epoll_wait(int epfd, struct epoll_event *events, int maxevents,
		      int timeout)
{
	(void)epfd;
	(void)events;
	(void)maxevents;
	(void)timeout;
	return -1;
}
#endif
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

static int epoll_add(int epoll_fd, int fd, uint32_t events)
{
	struct epoll_event event;

	memset(&event, 0, sizeof(event));
	event.events = events;
	event.data.fd = fd;
	return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == 0;
}

static int epoll_mod(int epoll_fd, int fd, uint32_t events)
{
	struct epoll_event event;

	memset(&event, 0, sizeof(event));
	event.events = events;
	event.data.fd = fd;
	return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) == 0;
}

static int empty_pipe_epoll_times_out(int epoll_fd)
{
	struct epoll_event event;

	memset(&event, 0, sizeof(event));
	return epoll_wait(epoll_fd, &event, 1, 0) == 0;
}

static int pipe_epoll_reports_write_ready(int epoll_fd, int write_fd)
{
	struct epoll_event event;
	int ready;

	memset(&event, 0, sizeof(event));
	if (!epoll_add(epoll_fd, write_fd, EPOLLOUT))
		return 0;
	ready = epoll_wait(epoll_fd, &event, 1, 0) == 1 &&
		event.data.fd == write_fd && (event.events & EPOLLOUT) != 0;
	(void)epoll_ctl(epoll_fd, EPOLL_CTL_DEL, write_fd, NULL);
	return ready;
}

static int pipe_epoll_reports_read_ready(int epoll_fd, int read_fd)
{
	struct epoll_event event;

	memset(&event, 0, sizeof(event));
	return epoll_wait(epoll_fd, &event, 1, 0) == 1 &&
	       event.data.fd == read_fd && (event.events & EPOLLIN) != 0;
}

static int pipe_read_returns_payload(int read_fd)
{
	static const char expected[] = "orlix-pipe-epoll\n";
	char buffer[sizeof(expected)];
	ssize_t nread;

	memset(buffer, 0, sizeof(buffer));
	nread = read(read_fd, buffer, sizeof(expected) - 1);
	return nread == (ssize_t)(sizeof(expected) - 1) &&
	       memcmp(buffer, expected, sizeof(expected) - 1) == 0;
}

static int pipe_epoll_reports_hangup(int epoll_fd, int read_fd)
{
	struct epoll_event event;

	memset(&event, 0, sizeof(event));
	return epoll_wait(epoll_fd, &event, 1, 0) == 1 &&
	       event.data.fd == read_fd && (event.events & EPOLLHUP) != 0;
}

static int observe(const char *name, int ok)
{
	print_observation(name, ok ? "ok" : "fail");
	return ok;
}

int main(void)
{
	static const char payload[] = "orlix-pipe-epoll\n";
	int fds[2] = { -1, -1 };
	int epoll_fd = -1;
	int passed = 1;
	int pipe_created;
	int epoll_created;
	int read_registered = 0;
	int wrote_payload = 0;

	pipe_created = pipe(fds) == 0 && set_nonblock(fds[0]);
	epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	epoll_created = epoll_fd >= 0;

	passed &= observe("pipe-created", pipe_created);
	passed &= observe("epoll-created", epoll_created);
	passed &= observe("empty-read-eagain",
			  pipe_created && empty_pipe_read_returns_eagain(fds[0]));

	if (pipe_created && epoll_created)
		read_registered = epoll_add(epoll_fd, fds[0], EPOLLIN);
	passed &= observe("read-end-registered", read_registered);
	passed &= observe("empty-epoll-timeout",
			  read_registered && empty_pipe_epoll_times_out(epoll_fd));
	passed &= observe("write-end-writable",
			  pipe_created && epoll_created &&
				  pipe_epoll_reports_write_ready(epoll_fd,
								 fds[1]));

	if (pipe_created)
		wrote_payload = write(fds[1], payload, sizeof(payload) - 1) ==
				(ssize_t)(sizeof(payload) - 1);
	passed &= observe("read-end-readable",
			  wrote_payload && read_registered &&
				  pipe_epoll_reports_read_ready(epoll_fd,
								fds[0]));
	passed &= observe("read-payload",
			  wrote_payload && pipe_read_returns_payload(fds[0]));

	if (read_registered)
		read_registered = epoll_mod(epoll_fd, fds[0], EPOLLIN | EPOLLHUP);
	if (fds[1] >= 0) {
		close(fds[1]);
		fds[1] = -1;
	}
	passed &= observe("read-end-hangup",
			  read_registered &&
				  pipe_epoll_reports_hangup(epoll_fd, fds[0]));

	if (fds[0] >= 0)
		close(fds[0]);
	if (fds[1] >= 0)
		close(fds[1]);
	if (epoll_fd >= 0)
		close(epoll_fd);

	return passed ? 0 : 1;
}
