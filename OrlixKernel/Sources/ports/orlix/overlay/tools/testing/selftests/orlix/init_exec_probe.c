// SPDX-License-Identifier: GPL-2.0

#include <time.h>
#include <unistd.h>

int main(void)
{
	static const char message[] = "ORLIX-INIT-EXEC-PROBE\n";
	static const struct timespec one_second = {
		.tv_sec = 1,
		.tv_nsec = 0,
	};

	(void)write(STDOUT_FILENO, message, sizeof(message) - 1);
	for (;;)
		(void)nanosleep(&one_second, NULL);
}
