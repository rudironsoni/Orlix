// SPDX-License-Identifier: MIT

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define PROBE_MOUNTPOINT "/tmp/orlix-oracle-mount-ns"
#define PROBE_MARKER PROBE_MOUNTPOINT "/marker"

static void print_observation(const char *name, const char *value)
{
	printf("{\"observation\":\"%s\",\"value\":\"%s\"}\n", name, value);
	fflush(stdout);
}

static int read_file(const char *path, char *buffer, size_t capacity,
		     size_t *out_size)
{
	int fd;
	ssize_t nread;
	size_t total = 0;

	if (capacity == 0)
		return -1;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return -1;

	while (total + 1 < capacity) {
		nread = read(fd, buffer + total, capacity - total - 1);
		if (nread < 0) {
			close(fd);
			return -1;
		}
		if (nread == 0)
			break;
		total += (size_t)nread;
	}

	close(fd);
	buffer[total] = '\0';
	*out_size = total;
	return 0;
}

static int contains_bytes(const char *haystack, size_t size,
			  const char *needle)
{
	size_t needle_size = strlen(needle);
	size_t index;

	if (needle_size == 0 || needle_size > size)
		return 0;

	for (index = 0; index + needle_size <= size; index++)
		if (memcmp(haystack + index, needle, needle_size) == 0)
			return 1;
	return 0;
}

static int mountinfo_contains_tmpfs_mount(void)
{
	char buffer[8192];
	size_t size = 0;

	if (read_file("/proc/self/mountinfo", buffer, sizeof(buffer), &size) != 0)
		return 0;

	return contains_bytes(buffer, size, " " PROBE_MOUNTPOINT " ") &&
	       contains_bytes(buffer, size, " - tmpfs ");
}

static int ensure_mountpoint(void)
{
	return mkdir(PROBE_MOUNTPOINT, 0755) == 0 || errno == EEXIST;
}

static int child_probe(void)
{
	int fd;
	int ok = 1;

	if (unshare(CLONE_NEWNS) != 0)
		return 10;

	if (!ensure_mountpoint())
		return 11;

	if (mount("tmpfs", PROBE_MOUNTPOINT, "tmpfs", 0, "mode=755") != 0)
		return 12;

	ok = mountinfo_contains_tmpfs_mount();
	print_observation("child-mountinfo-tmpfs", ok ? "ok" : "fail");
	if (!ok)
		return 13;

	fd = open(PROBE_MARKER, O_CREAT | O_EXCL | O_WRONLY, 0644);
	if (fd < 0)
		return 14;
	if (write(fd, "child\n", 6) != 6) {
		close(fd);
		return 15;
	}
	if (close(fd) != 0)
		return 16;
	if (access(PROBE_MARKER, F_OK) != 0)
		return 17;

	return 0;
}

int main(void)
{
	pid_t child;
	int status = 0;
	int mountpoint_ok;
	int child_ok;
	int parent_hidden;

	(void)unlink(PROBE_MARKER);
	mountpoint_ok = ensure_mountpoint();
	print_observation("mountpoint-exists", mountpoint_ok ? "ok" : "fail");
	if (!mountpoint_ok)
		return 1;

	child = fork();
	if (child == 0)
		_exit(child_probe());

	child_ok = child > 0 && waitpid(child, &status, 0) == child &&
		   WIFEXITED(status) && WEXITSTATUS(status) == 0;
	print_observation("child-started", child > 0 ? "ok" : "fail");
	if (!child_ok)
		print_observation("child-mountinfo-tmpfs", "fail");

	parent_hidden = access(PROBE_MARKER, F_OK) != 0 && errno == ENOENT;
	if (!parent_hidden)
		(void)unlink(PROBE_MARKER);
	print_observation("parent-marker-hidden", parent_hidden ? "ok" : "fail");

	return child_ok && parent_hidden ? 0 : 1;
}
