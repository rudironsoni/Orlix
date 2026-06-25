// SPDX-License-Identifier: GPL-2.0
#define _GNU_SOURCE
#include <fcntl.h>
#include <sched.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

static bool child_exits_successfully(bool (*probe)(void))
{
	pid_t child;
	int status;

	child = fork();
	if (child < 0)
		return false;

	if (child == 0)
		_exit(probe() ? 0 : 1);

	if (waitpid(child, &status, 0) != child)
		return false;

	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static bool proc_file_is_readable(const char *path)
{
	char byte;
	int fd;

	fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return false;

	(void)read(fd, &byte, 1);
	close(fd);
	return true;
}

static size_t append_decimal(char *buffer, size_t pos, size_t capacity,
			     unsigned int value)
{
	char digits[10];
	size_t count = 0;

	if (value == 0)
		return orlix_append_bytes(buffer, pos, capacity, "0", 1);

	while (value > 0 && count < sizeof(digits)) {
		digits[count++] = (char)('0' + (value % 10));
		value /= 10;
	}

	while (count > 0)
		pos = orlix_append_bytes(buffer, pos, capacity, &digits[--count], 1);

	return pos;
}

static size_t build_mapping(char *buffer, size_t capacity, unsigned int host_id)
{
	size_t pos = 0;

	pos = orlix_append_bytes(buffer, pos, capacity, "0 ", 2);
	pos = append_decimal(buffer, pos, capacity, host_id);
	pos = orlix_append_bytes(buffer, pos, capacity, " 1\n", 3);
	return pos;
}

static bool write_file(const char *path, const char *payload, size_t length)
{
	int fd;
	size_t offset = 0;

	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		return false;

	while (offset < length) {
		ssize_t written = write(fd, payload + offset, length - offset);

		if (written <= 0) {
			close(fd);
			return false;
		}

		offset += (size_t)written;
	}

	close(fd);
	return true;
}

static bool user_namespace_inode_changes_after_unshare(void)
{
	struct stat before;
	struct stat after;

	if (stat("/proc/self/ns/user", &before) != 0)
		return false;

	if (unshare(CLONE_NEWUSER) != 0)
		return false;

	if (stat("/proc/self/ns/user", &after) != 0)
		return false;

	return before.st_dev != after.st_dev || before.st_ino != after.st_ino;
}

static bool user_namespace_exposes_id_maps(void)
{
	if (unshare(CLONE_NEWUSER) != 0)
		return false;

	return proc_file_is_readable("/proc/self/uid_map") &&
	       proc_file_is_readable("/proc/self/gid_map");
}

static bool user_namespace_exposes_setgroups_control(void)
{
	if (unshare(CLONE_NEWUSER) != 0)
		return false;

	return access("/proc/self/setgroups", F_OK) == 0;
}

static bool user_namespace_accepts_id_map_writes(void)
{
	char uid_map[64];
	char gid_map[64];
	size_t uid_map_len;
	size_t gid_map_len;
	uid_t host_uid = getuid();
	gid_t host_gid = getgid();

	if (unshare(CLONE_NEWUSER) != 0)
		return false;

	uid_map_len = build_mapping(uid_map, sizeof(uid_map),
				    (unsigned int)host_uid);
	gid_map_len = build_mapping(gid_map, sizeof(gid_map),
				    (unsigned int)host_gid);

	return write_file("/proc/self/setgroups", "deny\n", 5) &&
	       write_file("/proc/self/uid_map", uid_map, uid_map_len) &&
	       write_file("/proc/self/gid_map", gid_map, gid_map_len) &&
	       proc_file_is_readable("/proc/self/uid_map") &&
	       proc_file_is_readable("/proc/self/gid_map");
}

int main(void)
{
	orlix_test_plan(4);
	orlix_test_result(
		child_exits_successfully(user_namespace_inode_changes_after_unshare),
		"user namespace unshare changes /proc/self/ns/user");
	orlix_test_result(
		child_exits_successfully(user_namespace_exposes_id_maps),
		"user namespace exposes readable uid_map and gid_map");
	orlix_test_result(
		child_exits_successfully(user_namespace_exposes_setgroups_control),
		"user namespace exposes setgroups control");
	orlix_test_result(
		child_exits_successfully(user_namespace_accepts_id_map_writes),
		"user namespace accepts uid_map and gid_map writes");
	orlix_test_exit();
	return 0;
}
