// SPDX-License-Identifier: GPL-2.0
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

static bool append_uint(char *buffer, size_t capacity, size_t *position,
			unsigned int value)
{
	char digits[16];
	size_t count = 0;

	do {
		digits[count++] = (char)('0' + value % 10);
		value /= 10;
	} while (value != 0 && count < sizeof(digits));

	while (count > 0) {
		if (*position + 1 >= capacity)
			return false;
		buffer[(*position)++] = digits[--count];
	}
	buffer[*position] = '\0';
	return true;
}

static int open_pty_slave(int master)
{
	char path[64];
	unsigned int number = 0;
	int unlock = 0;
	size_t position = 0;

	if (ioctl(master, TIOCSPTLCK, &unlock) != 0 ||
	    ioctl(master, TIOCGPTN, &number) != 0)
		return -1;

	position = orlix_append_cstr(path, position, sizeof(path), "/dev/pts/");
	if (!append_uint(path, sizeof(path), &position, number))
		return -1;
	return open(path, O_RDWR | O_NOCTTY | O_CLOEXEC);
}

static bool set_and_read_winsize(int setter, int reader,
				 unsigned short rows, unsigned short columns)
{
	struct winsize requested = { 0 };
	struct winsize observed = { 0 };
	requested.ws_row = rows;
	requested.ws_col = columns;

	return ioctl(setter, TIOCSWINSZ, &requested) == 0 &&
	       ioctl(reader, TIOCGWINSZ, &observed) == 0 &&
	       observed.ws_row == rows && observed.ws_col == columns;
}

static bool write_all(int fd, const unsigned char *bytes, size_t length)
{
	size_t written = 0;

	while (written < length) {
		ssize_t result = write(fd, bytes + written, length - written);

		if (result <= 0)
			return false;
		written += (size_t)result;
	}
	return true;
}

static bool read_exact(int fd, unsigned char *bytes, size_t length)
{
	size_t received = 0;

	while (received < length) {
		ssize_t result = read(fd, bytes + received, length - received);

		if (result <= 0)
			return false;
		received += (size_t)result;
	}
	return true;
}

static bool configure_raw_transport(int slave)
{
	struct termios state;

	if (tcgetattr(slave, &state) != 0)
		return false;
	cfmakeraw(&state);
	return tcsetattr(slave, TCSANOW, &state) == 0;
}

static bool bytes_flow(int writer, int reader,
		       const unsigned char *expected, size_t length)
{
	unsigned char observed[16];

	if (length > sizeof(observed) ||
	    !write_all(writer, expected, length) ||
	    !read_exact(reader, observed, length))
		return false;
	return orlix_memcmp(expected, observed, length) == 0;
}

static bool child_stdio_attaches(int master, int slave)
{
	static const unsigned char input = 0x71;
	static const unsigned char expected[] = { 0x4f, 0x45 };
	unsigned char observed[sizeof(expected)];
	int status = 0;
	pid_t child;

	child = fork();
	if (child == 0) {
		unsigned char byte;

		close(master);
		if (setsid() < 0 || ioctl(slave, TIOCSCTTY, 0) != 0 ||
		    dup2(slave, STDIN_FILENO) < 0 ||
		    dup2(slave, STDOUT_FILENO) < 0 ||
		    dup2(slave, STDERR_FILENO) < 0)
			_exit(1);
		if (slave > STDERR_FILENO)
			close(slave);
		if (!read_exact(STDIN_FILENO, &byte, 1) || byte != input ||
		    !write_all(STDOUT_FILENO, expected, 1) ||
		    !write_all(STDERR_FILENO, expected + 1, 1))
			_exit(2);
		_exit(0);
	}
	if (child < 0)
		return false;
	if (!write_all(master, &input, 1) ||
	    !read_exact(master, observed, sizeof(observed))) {
		kill(child, SIGKILL);
		(void)waitpid(child, &status, 0);
		return false;
	}
	if (waitpid(child, &status, 0) != child)
		return false;
	return WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
	       orlix_memcmp(expected, observed, sizeof(expected)) == 0;
}

int main(void)
{
	static const unsigned char master_to_slave[] = {
		0x00, 0xff, 0x0d, 0x0a, 0x1b, 0x7f,
	};
	static const unsigned char slave_to_master[] = {
		0x80, 0x00, 0xfe, 0x41, 0x0a, 0x1b,
	};
	int master;
	int slave = -1;

	orlix_test_plan(8);
	master = open("/dev/ptmx", O_RDWR | O_NOCTTY | O_CLOEXEC);
	orlix_test_result(master >= 0, "Linux PTY master allocates");
	if (master >= 0)
		slave = open_pty_slave(master);
	orlix_test_result(slave >= 0, "Linux PTY slave attaches");
	orlix_test_result(slave >= 0 && configure_raw_transport(slave),
			  "PTY slave raw line discipline preserves transport bytes");
	orlix_test_result(master >= 0 && slave >= 0 &&
			  bytes_flow(master, slave, master_to_slave,
				     sizeof(master_to_slave)),
			  "PTY master input reaches slave unchanged");
	orlix_test_result(master >= 0 && slave >= 0 &&
			  bytes_flow(slave, master, slave_to_master,
				     sizeof(slave_to_master)),
			  "PTY slave output reaches master unchanged");
	orlix_test_result(master >= 0 && slave >= 0 &&
			  child_stdio_attaches(master, slave),
			  "PTY child stdin stdout and stderr attach to slave");
	orlix_test_result(master >= 0 && slave >= 0 &&
			  set_and_read_winsize(master, slave, 37, 113),
			  "PTY master resize reaches slave");
	orlix_test_result(master >= 0 && slave >= 0 &&
			  set_and_read_winsize(slave, master, 42, 120),
			  "PTY slave resize reaches master");

	if (slave >= 0)
		close(slave);
	if (master >= 0)
		close(master);
	orlix_test_exit();
}
