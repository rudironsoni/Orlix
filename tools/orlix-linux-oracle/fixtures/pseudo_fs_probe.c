// SPDX-License-Identifier: MIT

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

static int contains_bytes(const char *haystack, size_t size, const char *needle)
{
	size_t needle_size = strlen(needle);
	size_t index;

	if (needle_size == 0 || needle_size > size)
		return 0;
	for (index = 0; index + needle_size <= size; index++) {
		if (memcmp(haystack + index, needle, needle_size) == 0)
			return 1;
	}
	return 0;
}

static int append_cstr(char *buffer, size_t *position, size_t capacity,
		       const char *value)
{
	while (*value) {
		if (*position + 1 >= capacity)
			return -1;
		buffer[*position] = *value;
		(*position)++;
		value++;
	}
	return 0;
}

static int mountinfo_contains(const char *mountpoint, const char *filesystem)
{
	char buffer[8192];
	char mount_fragment[64];
	char fs_fragment[64];
	size_t mount_pos = 0;
	size_t fs_pos = 0;
	size_t size = 0;

	if (read_file("/proc/self/mountinfo", buffer, sizeof(buffer), &size) != 0)
		return 0;
	if (append_cstr(mount_fragment, &mount_pos, sizeof(mount_fragment), " ") != 0 ||
	    append_cstr(mount_fragment, &mount_pos, sizeof(mount_fragment),
			mountpoint) != 0 ||
	    append_cstr(mount_fragment, &mount_pos, sizeof(mount_fragment), " ") != 0)
		return 0;
	mount_fragment[mount_pos] = '\0';

	if (append_cstr(fs_fragment, &fs_pos, sizeof(fs_fragment), " - ") != 0 ||
	    append_cstr(fs_fragment, &fs_pos, sizeof(fs_fragment), filesystem) != 0 ||
	    append_cstr(fs_fragment, &fs_pos, sizeof(fs_fragment), " ") != 0)
		return 0;
	fs_fragment[fs_pos] = '\0';

	return contains_bytes(buffer, size, mount_fragment) &&
	       contains_bytes(buffer, size, fs_fragment);
}

static int path_is_directory(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int path_is_character_device(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 && S_ISCHR(st.st_mode);
}

static int proc_self_shape_is_readable(void)
{
	char buffer[1024];
	size_t size = 0;

	return read_file("/proc/self/status", buffer, sizeof(buffer), &size) == 0 &&
	       size > 0 && path_is_directory("/proc/self/fd") &&
	       path_is_character_device("/dev/null") &&
	       read_file("/proc/self/mounts", buffer, sizeof(buffer), &size) == 0 &&
	       size > 0;
}

static int dev_core_nodes_are_character_devices(void)
{
	return path_is_character_device("/dev/null") &&
	       path_is_character_device("/dev/zero") &&
	       path_is_character_device("/dev/random") &&
	       path_is_character_device("/dev/urandom");
}

static int devpts_allocates_ptmx(void)
{
	static const char *const ptmx_paths[] = {
		"/dev/ptmx",
		"/dev/pts/ptmx",
		NULL
	};
	const char *const *path;

	for (path = ptmx_paths; *path; path++) {
		struct stat st;
		int fd = open(*path, O_RDWR | O_NOCTTY | O_CLOEXEC);
		if (fd < 0)
			continue;
		if (fstat(fd, &st) == 0 && S_ISCHR(st.st_mode)) {
			close(fd);
			return 1;
		}
		close(fd);
	}
	return 0;
}

static int observe(const char *name, int ok)
{
	print_observation(name, ok ? "ok" : "fail");
	return ok;
}

int main(void)
{
	int passed = 1;

	passed &= observe("procfs-mounted", mountinfo_contains("/proc", "proc"));
	passed &= observe("sysfs-mounted", mountinfo_contains("/sys", "sysfs"));
	passed &= observe("devtmpfs-mounted", mountinfo_contains("/dev", "devtmpfs"));
	passed &= observe("devpts-mounted",
			  mountinfo_contains("/dev/pts", "devpts"));
	passed &= observe("tmpfs-mounted", mountinfo_contains("/tmp", "tmpfs"));
	passed &= observe("proc-self-readable", proc_self_shape_is_readable());
	passed &= observe("dev-core-char", dev_core_nodes_are_character_devices());
	passed &= observe("devpts-directory", path_is_directory("/dev/pts"));
	passed &= observe("ptmx-allocates", devpts_allocates_ptmx());
	passed &= observe("sysfs-virtio-directory",
			  path_is_directory("/sys/bus/virtio/devices"));

	return passed ? 0 : 1;
}
