// SPDX-License-Identifier: MIT

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CGROUP_ROOT "/sys/fs/cgroup"
#define CGROUP_CHILD CGROUP_ROOT "/orlix-oracle-cgroup-v2"

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

static int file_is_readable(const char *path)
{
	char buffer[512];
	size_t size = 0;

	return read_file(path, buffer, sizeof(buffer), &size) == 0;
}

static int proc_self_cgroup_has_v2_root(void)
{
	char buffer[256];
	size_t size = 0;

	return read_file("/proc/self/cgroup", buffer, sizeof(buffer), &size) == 0 &&
	       contains_bytes(buffer, size, "0::/");
}

static int mountinfo_has_cgroup2_root(void)
{
	char buffer[8192];
	size_t size = 0;

	return read_file("/proc/self/mountinfo", buffer, sizeof(buffer), &size) == 0 &&
	       contains_bytes(buffer, size, " /sys/fs/cgroup ") &&
	       contains_bytes(buffer, size, " - cgroup2 ");
}

static int write_string_to_file(const char *path, const char *data)
{
	int fd;
	size_t length = strlen(data);
	ssize_t written;

	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return 0;
	written = write(fd, data, length);
	close(fd);
	return written == (ssize_t)length;
}

static int cgroup_procs_accepts_self(const char *path)
{
	return write_string_to_file(path, "0\n");
}

static int child_cgroup_can_be_created(void)
{
	if (mkdir(CGROUP_CHILD, 0755) == 0)
		return 1;
	if (errno == EEXIST)
		return rmdir(CGROUP_CHILD) == 0 && mkdir(CGROUP_CHILD, 0755) == 0;
	return 0;
}

static int child_cgroup_accepts_self(void)
{
	return cgroup_procs_accepts_self(CGROUP_CHILD "/cgroup.procs");
}

static int return_to_root_and_remove_child(void)
{
	return cgroup_procs_accepts_self(CGROUP_ROOT "/cgroup.procs") &&
	       rmdir(CGROUP_CHILD) == 0;
}

static int observe(const char *name, int ok)
{
	print_observation(name, ok ? "ok" : "fail");
	return ok;
}

int main(void)
{
	int ok = 1;
	int child_created;
	int moved_to_child = 0;

	ok &= observe("proc-self-cgroup-v2-root",
		      proc_self_cgroup_has_v2_root());
	ok &= observe("mountinfo-cgroup2-root", mountinfo_has_cgroup2_root());
	ok &= observe("controllers-readable",
		      file_is_readable(CGROUP_ROOT "/cgroup.controllers"));
	ok &= observe("subtree-control-readable",
		      file_is_readable(CGROUP_ROOT "/cgroup.subtree_control"));
	ok &= observe("procs-readable",
		      file_is_readable(CGROUP_ROOT "/cgroup.procs"));
	ok &= observe("root-procs-accepts-self",
		      cgroup_procs_accepts_self(CGROUP_ROOT "/cgroup.procs"));

	child_created = child_cgroup_can_be_created();
	ok &= observe("child-cgroup-created", child_created);
	if (child_created)
		moved_to_child = child_cgroup_accepts_self();
	ok &= observe("child-procs-accepts-self", moved_to_child);
	ok &= observe("child-cgroup-removed",
		      child_created && return_to_root_and_remove_child());

	return ok ? 0 : 1;
}
