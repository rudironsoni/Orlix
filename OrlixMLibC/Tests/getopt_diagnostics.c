// SPDX-License-Identifier: MIT

#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
	static const struct option options[] = {
		{ "base64", no_argument, NULL, '6' },
		{ NULL, 0, NULL, 0 },
	};
	static const char expected[] =
		"basenc: unrecognized option '--foobar'\n";
	char *argv[] = { "basenc", "--foobar", NULL };
	char observed[sizeof(expected)] = {};
	int stderr_copy;
	int pipe_fds[2];
	ssize_t size;
	int result;

	if (pipe(pipe_fds) || (stderr_copy = dup(STDERR_FILENO)) < 0)
		return 1;
	if (dup2(pipe_fds[1], STDERR_FILENO) < 0)
		return 1;
	close(pipe_fds[1]);

	optind = 0;
	opterr = 1;
	result = getopt_long(2, argv, "", options, NULL);
	fflush(stderr);
	if (dup2(stderr_copy, STDERR_FILENO) < 0)
		return 1;
	close(stderr_copy);

	size = read(pipe_fds[0], observed, sizeof(observed));
	close(pipe_fds[0]);
	if (result != '?' || optind != 2)
		return 1;
	if (size != (ssize_t)(sizeof(expected) - 1))
		return 1;
	return memcmp(observed, expected, sizeof(expected) - 1) != 0;
}
