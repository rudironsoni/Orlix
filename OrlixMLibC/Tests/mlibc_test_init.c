/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <net/if.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static char test_list[32768];
static char selected_test[128];
static unsigned int test_index;
static unsigned int test_failures;
static char *const test_envp[] = {
	"LANG=en_US.utf8",
	"HOME=/root",
	"LOGNAME=root",
	"SHELL=/bin/sh",
	"USER=root",
	NULL,
};

static void park_init(void)
{
	for (;;)
		(void)pause();
}

static void test_result(bool passed, const char *name)
{
	test_index++;
	if (!passed)
		test_failures++;
	printf("%s %u - %s\n", passed ? "ok" : "not ok", test_index, name);
	fflush(stdout);
}

static bool read_file(const char *path, char *buffer, size_t capacity,
		      size_t *size)
{
	FILE *file;

	file = fopen(path, "r");
	if (!file)
		return false;
	*size = fread(buffer, 1, capacity - 1, file);
	buffer[*size] = '\0';
	fclose(file);
	return true;
}

static bool parse_test_line(char *line, char **label, char **path)
{
	char *separator;

	if (line[0] == '\0' || line[0] == '#')
		return false;
	separator = strchr(line, ':');
	if (!separator)
		return false;
	*separator = '\0';
	*label = line;
	*path = separator + 1;
	return **label != '\0' && **path != '\0';
}

static void read_selected_test(void)
{
	static const char prefix[] = "orlix.mlibc=";
	char cmdline[1024];
	size_t size = 0;
	size_t pos;

	selected_test[0] = '\0';
	if (!read_file("/proc/cmdline", cmdline, sizeof(cmdline), &size))
		return;

	for (pos = 0; pos < size; pos++) {
		size_t i;
		size_t out = 0;

		if (pos > 0 && cmdline[pos - 1] != ' ')
			continue;
		for (i = 0; prefix[i]; i++) {
			if (pos + i >= size || cmdline[pos + i] != prefix[i])
				break;
		}
		if (prefix[i])
			continue;
		pos += i;
		while (pos < size && cmdline[pos] != ' ' &&
		       cmdline[pos] != '\n' &&
		       out + 1 < sizeof(selected_test))
			selected_test[out++] = cmdline[pos++];
		selected_test[out] = '\0';
		return;
	}
}

static bool test_is_selected(const char *label)
{
	if (selected_test[0] == '\0')
		return true;
	return strcmp(selected_test, label) == 0;
}

static bool mount_tmpfs_at(const char *target, const char *data)
{
	if (mkdir(target, 01777) < 0 && errno != EEXIST) {
		perror("# mkdir tmpfs target");
		return false;
	}
	if (mount("tmpfs", target, "tmpfs", 0, data) < 0 && errno != EBUSY) {
		perror("# mount tmpfs");
		return false;
	}
	return true;
}

static void configure_loopback(void)
{
	struct ifreq ifr;
	int fd;

	fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		perror("# loopback socket");
		return;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, "lo", IFNAMSIZ - 1);
	if (ioctl(fd, SIOCGIFFLAGS, &ifr) < 0) {
		perror("# loopback SIOCGIFFLAGS");
		close(fd);
		return;
	}

	if (!(ifr.ifr_flags & IFF_UP)) {
		ifr.ifr_flags |= IFF_UP;
		if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0)
			perror("# loopback SIOCSIFFLAGS");
	}
	close(fd);
}

static unsigned int count_tests(char *data, size_t size)
{
	unsigned int count = 0;
	char *cursor = data;

	while (cursor < data + size) {
		char *line = cursor;
		char *newline = strchr(cursor, '\n');
		char *label;
		char *path;

		if (!newline)
			newline = data + size;
		*newline = '\0';
		if (parse_test_line(line, &label, &path) &&
		    test_is_selected(label))
			count++;
		cursor = newline + 1;
	}
	return count;
}

static int run_test(const char *label, const char *path)
{
	pid_t child;
	int status;

	printf("# exec %s (%s)\n", path, label);
	fflush(stdout);

	child = fork();
	if (child == 0) {
		char *const argv[] = { (char *)path, NULL };

		execve(path, argv, test_envp);
		_exit(127);
	}
	if (child < 0)
		return -1;
	{
		int waited;

		for (waited = 0; waited < 20; waited++) {
			pid_t reaped = waitpid(child, &status, WNOHANG);

			if (reaped == child)
				break;
			if (reaped < 0)
				return -1;
			(void)sleep(1);
		}
		if (waited == 20) {
			printf("# %s exceeded 20s wait, killing pid %d\n",
			       label, (int)child);
			fflush(stdout);
			(void)kill(child, SIGKILL);
			if (waitpid(child, &status, 0) != child)
				return -1;
			return -1;
		}
	}
	if (WIFEXITED(status)) {
		int code = WEXITSTATUS(status);

		if (code)
			printf("# %s exited with status %d\n", label, code);
		return code;
	}
	if (WIFSIGNALED(status))
		printf("# %s killed by signal %d\n", label, WTERMSIG(status));
	else
		printf("# %s ended with wait status 0x%x\n", label, status);
	fflush(stdout);
	if (!WIFEXITED(status))
		return -1;
	return -1;
}

static void run_tests(char *data, size_t size)
{
	char *cursor = data;

	while (cursor < data + size) {
		char *line = cursor;
		char *newline = strchr(cursor, '\n');
		char *label;
		char *path;

		if (!newline)
			newline = data + size;
		*newline = '\0';
		if (parse_test_line(line, &label, &path) &&
		    test_is_selected(label))
			test_result(run_test(label, path) == 0, label);
		cursor = newline + 1;
	}
}

int main(void)
{
	size_t list_size = 0;
	unsigned int test_count = 0;
	bool have_list;

	puts("ORLIX-MLIBC-TEST-INIT");
	(void)mkdir("/proc", 0555);
	(void)mount("proc", "/proc", "proc", 0, NULL);
	read_selected_test();
	configure_loopback();

	have_list = read_file("/mlibc-test-list.txt", test_list,
			      sizeof(test_list), &list_size);
	if (have_list)
		test_count = count_tests(test_list, list_size);

	printf("TAP version 13\n1..%u\n", test_count + 2);
	test_result(have_list && test_count > 0,
		    "installed upstream mlibc test list is readable");
	test_result(mount_tmpfs_at("/tmp", "mode=1777") && chdir("/tmp") == 0,
		    "tmpfs mounted at /tmp for mlibc tests");
	if (have_list) {
		have_list = read_file("/mlibc-test-list.txt", test_list,
				      sizeof(test_list), &list_size);
		if (have_list)
			run_tests(test_list, list_size);
	}
	puts("ORLIX-MLIBC-TEST-END");
	fflush(stdout);

	if (getpid() == 1)
		park_init();
	return test_failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
