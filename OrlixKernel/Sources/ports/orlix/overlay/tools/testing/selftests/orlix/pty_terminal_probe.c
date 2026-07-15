// SPDX-License-Identifier: GPL-2.0
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
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

int main(void)
{
	int master;
	int slave = -1;

	orlix_test_plan(4);
	master = open("/dev/ptmx", O_RDWR | O_NOCTTY | O_CLOEXEC);
	orlix_test_result(master >= 0, "Linux PTY master allocates");
	if (master >= 0)
		slave = open_pty_slave(master);
	orlix_test_result(slave >= 0, "Linux PTY slave attaches");
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
