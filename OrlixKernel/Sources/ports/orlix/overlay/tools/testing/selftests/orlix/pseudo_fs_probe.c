// SPDX-License-Identifier: GPL-2.0
#include <fcntl.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

static bool file_contains(const char *path, const char *needle)
{
	char buffer[8192];
	size_t size = 0;

	return orlix_read_file(path, buffer, sizeof(buffer), &size) == 0 &&
	       orlix_contains(buffer, size, needle);
}

static bool mountinfo_has_mount(const char *target, const char *fs_type)
{
	char buffer[65536];
	char mount_fragment[80];
	char fs_fragment[80];
	size_t pos = 0;
	size_t fs_pos = 0;
	size_t size = 0;

	pos = orlix_append_cstr(mount_fragment, pos, sizeof(mount_fragment),
			       " ");
	pos = orlix_append_cstr(mount_fragment, pos, sizeof(mount_fragment),
			       target);
	pos = orlix_append_cstr(mount_fragment, pos, sizeof(mount_fragment),
			       " ");

	fs_pos = orlix_append_cstr(fs_fragment, fs_pos, sizeof(fs_fragment),
				  " - ");
	fs_pos = orlix_append_cstr(fs_fragment, fs_pos, sizeof(fs_fragment),
				  fs_type);
	fs_pos = orlix_append_cstr(fs_fragment, fs_pos, sizeof(fs_fragment),
				 " ");
	mount_fragment[pos] = '\0';
	fs_fragment[fs_pos] = '\0';

	if (orlix_read_file("/proc/self/mountinfo", buffer, sizeof(buffer),
			 &size) != 0)
		return false;

	if (!orlix_contains(buffer, size, mount_fragment))
		orlix_test_comment("missing mountinfo target ", target,
				   orlix_strlen(target));
	if (!orlix_contains(buffer, size, fs_fragment))
		orlix_test_comment("missing mountinfo fs ", fs_type,
				   orlix_strlen(fs_type));
	return orlix_contains(buffer, size, mount_fragment) &&
	       orlix_contains(buffer, size, fs_fragment);
}

static bool path_is_directory(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool path_is_character_device(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 && S_ISCHR(st.st_mode);
}

static bool dev_null_accepts_write(void)
{
	int fd;
	bool ok;

	fd = open("/dev/null", O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return false;

	ok = write(fd, "x", 1) == 1;
	close(fd);
	return ok;
}

static bool dev_zero_returns_zero(void)
{
	unsigned char byte = 0xff;
	int fd;
	bool ok;

	fd = open("/dev/zero", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;

	ok = read(fd, &byte, 1) == 1 && byte == 0;
	close(fd);
	return ok;
}

static bool dev_urandom_returns_byte(void)
{
	unsigned char byte;
	int fd;
	bool ok;

	fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;

	ok = read(fd, &byte, 1) == 1;
	close(fd);
	return ok;
}

static bool devpts_allocates_ptmx(void)
{
	static const char *const ptmx_paths[] = {
		"/dev/ptmx",
		"/dev/pts/ptmx",
		NULL,
	};
	const char *const *path;
	struct stat st;
	int fd;

	for (path = ptmx_paths; *path; path++) {
		fd = open(*path, O_RDWR | O_NOCTTY | O_CLOEXEC);
		if (fd < 0)
			continue;

		if (fstat(fd, &st) == 0 && S_ISCHR(st.st_mode)) {
			close(fd);
			return true;
		}
		close(fd);
	}

	return false;
}

int main(void)
{
	orlix_test_plan(20);

	orlix_test_result(path_is_directory("/proc"), "/proc is a directory");
	orlix_test_result(path_is_directory("/sys"), "/sys is a directory");
	orlix_test_result(path_is_directory("/dev"), "/dev is a directory");
	orlix_test_result(path_is_directory("/dev/pts"),
			  "/dev/pts is a directory");
	orlix_test_result(path_is_directory("/tmp"), "/tmp is a directory");
	orlix_test_result(mountinfo_has_mount("/proc", "proc"),
			  "mountinfo reports procfs at /proc");
	orlix_test_result(mountinfo_has_mount("/sys", "sysfs"),
			  "mountinfo reports sysfs at /sys");
	orlix_test_result(mountinfo_has_mount("/dev", "devtmpfs"),
			  "mountinfo reports devtmpfs at /dev");
	orlix_test_result(mountinfo_has_mount("/dev/pts", "devpts"),
			  "mountinfo reports devpts at /dev/pts");
	orlix_test_result(mountinfo_has_mount("/tmp", "tmpfs"),
			  "mountinfo reports tmpfs at /tmp");
	orlix_test_result(file_contains("/proc/self/status", "Pid:"),
			  "proc self status exposes pid");
	orlix_test_result(file_contains("/proc/self/status", "Uid:"),
			  "proc self status exposes uid");
	orlix_test_result(file_contains("/proc/self/mounts", " /proc "),
			  "proc self mounts exposes proc mount");
	orlix_test_result(path_is_character_device("/dev/null"),
			  "/dev/null is a character device");
	orlix_test_result(path_is_character_device("/dev/zero"),
			  "/dev/zero is a character device");
	orlix_test_result(path_is_character_device("/dev/random"),
			  "/dev/random is a character device");
	orlix_test_result(path_is_character_device("/dev/urandom"),
			  "/dev/urandom is a character device");
	orlix_test_result(dev_null_accepts_write(), "/dev/null accepts writes");
	orlix_test_result(dev_zero_returns_zero() && dev_urandom_returns_byte(),
			  "random pseudo devices are readable");
	orlix_test_result(devpts_allocates_ptmx(),
			  "devpts exposes a usable ptmx device");

	orlix_test_exit();
}
