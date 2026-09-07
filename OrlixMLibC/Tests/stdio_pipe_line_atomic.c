// SPDX-License-Identifier: MIT

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define LOAD_CHILDREN 40
#define WRITERS 2
#define LINES 400
#define LINE_LEN 37

static int spawn_load(pid_t *pids, int read_fd, int write_fd)
{
	unsigned int i;

	for (i = 0; i < LOAD_CHILDREN; i++) {
		pid_t pid = fork();

		if (pid < 0)
			return -1;
		if (pid == 0) {
			close(read_fd);
			close(write_fd);
			for (;;)
				pause();
		}
		pids[i] = pid;
	}
	return 0;
}

static void reap_load(pid_t *pids)
{
	unsigned int i;

	for (i = 0; i < LOAD_CHILDREN; i++) {
		if (pids[i] > 0) {
			kill(pids[i], SIGKILL);
			waitpid(pids[i], NULL, 0);
		}
	}
}

static int writer_stdio(int write_fd, unsigned int id)
{
	unsigned int line;

	if (dup2(write_fd, STDOUT_FILENO) < 0)
		return 1;
	close(write_fd);
	if (setvbuf(stdout, NULL, _IOLBF, 0))
		return 1;
	for (line = 0; line < LINES; line++) {
		unsigned int nibble;
		unsigned int i;

		for (i = 0; i < 32; i++) {
			nibble = (id + line + i) & 0xf;
			if (printf("%x", nibble) != 1)
				return 1;
		}
		if (putchar(' ') == EOF || putchar(' ') == EOF)
			return 1;
		if (printf("%02u\n", id) != 3)
			return 1;
	}
	if (fflush(stdout))
		return 1;
	return 0;
}

static int line_is_checksum(const char *line)
{
	unsigned int i;

	for (i = 0; i < 32; i++) {
		char c = line[i];

		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
			return 0;
	}
	if (line[32] != ' ' || line[33] != ' ')
		return 0;
	if (line[34] < '0' || line[34] > '9' || line[35] < '0' ||
	    line[35] > '9' || line[36] != '\n')
		return 0;
	return 1;
}

int main(void)
{
	int fds[2];
	pid_t load[LOAD_CHILDREN];
	pid_t writers[WRITERS];
	unsigned int i;
	char buf[LINE_LEN];
	size_t have = 0;
	unsigned int seen = 0;
	int failed = 0;

	memset(load, 0, sizeof(load));
	if (pipe(fds) < 0)
		return 1;
	if (spawn_load(load, fds[0], fds[1]) < 0) {
		close(fds[0]);
		close(fds[1]);
		return 1;
	}
	for (i = 0; i < WRITERS; i++) {
		pid_t pid = fork();

		if (pid < 0) {
			failed = 1;
			break;
		}
		if (pid == 0) {
			close(fds[0]);
			_exit(writer_stdio(fds[1], i));
		}
		writers[i] = pid;
	}
	close(fds[1]);
	if (!failed) {
		for (;;) {
			ssize_t n = read(fds[0], buf + have, LINE_LEN - have);

			if (n == 0)
				break;
			if (n < 0) {
				failed = 1;
				break;
			}
			have += (size_t)n;
			while (have >= LINE_LEN) {
				if (!line_is_checksum(buf)) {
					failed = 1;
					goto done;
				}
				seen++;
				have -= LINE_LEN;
				if (have)
					memmove(buf, buf + LINE_LEN, have);
			}
		}
		if (have != 0 || seen != WRITERS * LINES)
			failed = 1;
		for (i = 0; i < WRITERS; i++) {
			int status = 0;

			if (waitpid(writers[i], &status, 0) != writers[i] ||
			    !WIFEXITED(status) || WEXITSTATUS(status) != 0)
				failed = 1;
		}
	}
done:
	close(fds[0]);
	reap_load(load);
	return failed;
}
