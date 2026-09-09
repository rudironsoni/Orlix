/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
	int status = 0;
	struct rusage usage = { 0 };
	pid_t child;
	pid_t reaped;

	errno = 0;
	if (wait3(&status, WNOHANG, &usage) != -1 || errno != ECHILD)
		return 1;

	child = fork();
	if (child < 0)
		return 2;
	if (child == 0)
		_exit(42);

	status = 0;
	reaped = wait3(&status, 0, &usage);
	if (reaped != child)
		return 3;
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 42)
		return 4;
	return 0;
}
