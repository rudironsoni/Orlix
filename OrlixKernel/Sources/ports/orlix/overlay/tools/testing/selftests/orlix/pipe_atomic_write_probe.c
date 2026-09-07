// SPDX-License-Identifier: GPL-2.0

#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

#define LOAD_CHILDREN 40
#define WRITERS 2
#define LINES 400
#define LINE_LEN 64

static void stage(const char *name)
{
	orlix_write_all("# pipe_atomic_write ");
	orlix_write_all(name);
	orlix_write_all("\n");
}

static bool write_record(int fd, unsigned int writer, unsigned int line)
{
	char record[LINE_LEN];
	ssize_t n;
	size_t off = 0;

	memset(record, (int)('A' + (writer % 26)), LINE_LEN);
	record[0] = (char)('0' + writer);
	record[1] = ' ';
	record[2] = (char)('0' + (line % 10));
	record[LINE_LEN - 1] = '\n';

	while (off < LINE_LEN) {
		n = write(fd, record + off, LINE_LEN - off);
		if (n <= 0)
			return false;
		off += (size_t)n;
	}
	return true;
}

static bool spawn_load(pid_t *pids, int read_fd, int write_fd)
{
	unsigned int i;

	for (i = 0; i < LOAD_CHILDREN; i++) {
		pid_t pid = fork();

		if (pid < 0)
			return false;
		if (pid == 0) {
			close(read_fd);
			close(write_fd);
			for (;;)
				pause();
		}
		pids[i] = pid;
	}
	return true;
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

static bool concurrent_line_writes_stay_whole(void)
{
	int fds[2];
	pid_t load[LOAD_CHILDREN];
	pid_t writers[WRITERS];
	unsigned int i;
	char buf[LINE_LEN];
	size_t have = 0;
	unsigned int seen = 0;
	unsigned int counts[WRITERS] = { 0 };
	bool ok = true;

	memset(load, 0, sizeof(load));
	stage("pipe");
	if (pipe(fds) < 0)
		return false;
	stage("load");
	if (!spawn_load(load, fds[0], fds[1])) {
		close(fds[0]);
		close(fds[1]);
		return false;
	}
	stage("writers");
	for (i = 0; i < WRITERS; i++) {
		pid_t pid = fork();

		if (pid < 0) {
			ok = false;
			break;
		}
		if (pid == 0) {
			unsigned int line;

			close(fds[0]);
			for (line = 0; line < LINES; line++) {
				if (!write_record(fds[1], i, line))
					_exit(1);
			}
			close(fds[1]);
			_exit(0);
		}
		writers[i] = pid;
	}
	close(fds[1]);
	if (ok) {
		for (;;) {
			ssize_t n = read(fds[0], buf + have, LINE_LEN - have);

			if (n == 0)
				break;
			if (n < 0) {
				ok = false;
				break;
			}
			have += (size_t)n;
			while (have >= LINE_LEN) {
				unsigned int writer;

				if (buf[LINE_LEN - 1] != '\n') {
					ok = false;
					goto done;
				}
				if (buf[0] < '0' || buf[0] >= (char)('0' + WRITERS) ||
				    buf[1] != ' ') {
					ok = false;
					goto done;
				}
				writer = (unsigned int)(buf[0] - '0');
				counts[writer]++;
				seen++;
				have -= LINE_LEN;
				if (have)
					memmove(buf, buf + LINE_LEN, have);
			}
		}
		if (have != 0)
			ok = false;
		if (seen != WRITERS * LINES)
			ok = false;
		for (i = 0; i < WRITERS; i++) {
			int status = 0;

			if (counts[i] != LINES)
				ok = false;
			if (waitpid(writers[i], &status, 0) != writers[i] ||
			    !WIFEXITED(status) || WEXITSTATUS(status) != 0)
				ok = false;
		}
	}
done:
	close(fds[0]);
	reap_load(load);
	return ok;
}

int main(void)
{
	orlix_test_plan(1);
	orlix_test_result(concurrent_line_writes_stay_whole(),
			  "PIPE_BUF concurrent line writes stay whole under load");
	orlix_test_exit();
}
