// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <linux/ioprio.h>
#include <poll.h>
#include <sched.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <linux/capability.h>
#include <linux/personality.h>
#include <linux/sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define ORLIX_INIT_CMDLINE_SIZE 16384
#define ORLIX_INIT_MAX_RLIMITS 16
#define ORLIX_INIT_MAX_CGROUP_UNIFIED 8
#define ORLIX_INIT_MAX_DEVICE_NODES 16
#define ORLIX_INIT_MAX_TIME_OFFSETS 2
#define ORLIX_INIT_MAX_ID_MAPPINGS 16
#define ORLIX_INIT_MAX_TMPFS_MOUNTS 16
#define ORLIX_INIT_MAX_HOST_DIRECTORIES 16
#define ORLIX_INIT_MAX_NAMESPACES 8
#define ORLIX_INIT_MAX_NAMESPACE_JOINS 8
#define ORLIX_INIT_MAX_SYSCTLS 16
#define ORLIX_INIT_MAX_SUPPLEMENTARY_GROUPS 32
#define ORLIX_INIT_OOM_SCORE_ADJ_MIN -1000
#define ORLIX_INIT_OOM_SCORE_ADJ_MAX 1000
#define ORLIX_INIT_HOST_MOUNT_TARGET_SIZE 256
#define ORLIX_INIT_CGROUP_PATH_SIZE 256
#define ORLIX_INIT_CGROUP_FILE_SIZE 32
#define ORLIX_INIT_CGROUP_VALUE_SIZE 64
#define ORLIX_INIT_VALUE_SIZE 2048

struct orlix_rlimit_config {
	int resource;
	rlim_t soft;
	rlim_t hard;
};

struct orlix_cgroup_unified_config {
	char file[ORLIX_INIT_CGROUP_FILE_SIZE];
	char value[ORLIX_INIT_CGROUP_VALUE_SIZE];
};

struct orlix_device_node_config {
	char path[ORLIX_INIT_VALUE_SIZE];
	char type;
	unsigned long major;
	unsigned long minor;
	unsigned long mode;
	unsigned long uid;
	unsigned long gid;
};

struct orlix_sysctl_config {
	char key[ORLIX_INIT_VALUE_SIZE];
	char value[ORLIX_INIT_VALUE_SIZE];
};

struct orlix_time_offset_config {
	char clock[16];
	long secs;
	long nanosecs;
};

struct orlix_id_mapping_config {
	unsigned long container_id;
	unsigned long host_id;
	unsigned long size;
};

static void die(const char *message);
static int read_cmdline_decoded(const char *key, char *value,
				size_t value_size);
static int read_cmdline_unsigned(const char *key, unsigned long *value);

static int write_all(int fd, const void *bytes, size_t length)
{
	const char *cursor = bytes;

	while (length > 0) {
		ssize_t written = write(fd, cursor, length);

		if (written < 0 && errno == EINTR)
			continue;
		if (written <= 0)
			return -1;

		cursor += written;
		length -= (size_t)written;
	}

	return 0;
}

static void write_literal(int fd, const char *message)
{
	size_t length = 0;

	while (message[length] != '\0')
		length++;

	(void)write_all(fd, message, length);
}

static void write_errno_message(int fd, const char *prefix, int error)
{
	write_literal(fd, prefix);
	write_literal(fd, strerror(error));
	write_literal(fd, "\n");
}

static void write_process_started(pid_t pid)
{
	char buffer[96];
	int length = snprintf(buffer, sizeof(buffer),
			      "orlix-init: process started pid=%ld\n",
			      (long)pid);
	if (length > 0 && (size_t)length < sizeof(buffer))
		(void)write_all(STDERR_FILENO, buffer, (size_t)length);
}

static void write_process_completion(pid_t pid, int status)
{
	char buffer[128];
	int length;

	if (WIFEXITED(status)) {
		length = snprintf(buffer, sizeof(buffer),
				  "orlix-init: process exited pid=%ld status=%d\n",
				  (long)pid, WEXITSTATUS(status));
	} else if (WIFSIGNALED(status)) {
		length = snprintf(buffer, sizeof(buffer),
				  "orlix-init: process signaled pid=%ld signal=%d\n",
				  (long)pid, WTERMSIG(status));
	} else {
		length = snprintf(buffer, sizeof(buffer),
				  "orlix-init: process completed pid=%ld status=%d\n",
				  (long)pid, status);
	}

	if (length > 0 && (size_t)length < sizeof(buffer))
		(void)write_all(STDERR_FILENO, buffer, (size_t)length);
}

static void write_unsigned_decimal(int fd, unsigned long value)
{
	char buffer[32];
	size_t offset = sizeof(buffer);

	buffer[--offset] = '\0';
	do {
		buffer[--offset] = (char)('0' + (value % 10));
		value /= 10;
	} while (value != 0);

	(void)write_all(fd, &buffer[offset], sizeof(buffer) - offset - 1);
}

static int open_controlling_tty(void)
{
	static const char *const tty_candidates[] = {
		"/dev/hvc0",
		"/dev/ttyS0",
		NULL,
	};
	int fd = -1;

	if (setsid() < 0 && errno != EPERM)
		write_literal(STDERR_FILENO, "orlix-init: setsid failed\n");

	for (const char *const *path = tty_candidates; *path != NULL; path++) {
		write_literal(STDERR_FILENO, "orlix-init: opening tty candidate\n");
		fd = open(*path, O_RDWR | O_NONBLOCK);
		if (fd >= 0) {
			int flags = fcntl(fd, F_GETFL, 0);

			if (flags >= 0)
				(void)fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
			break;
		}
	}

	if (fd < 0) {
		fd = open("/dev/console", O_RDWR);
		if (fd < 0)
			return -1;
	}

	if (ioctl(fd, TIOCSCTTY, 0) < 0 && errno != EPERM)
		write_literal(fd, "orlix-init: TIOCSCTTY failed\n");

	return fd;
}

static int ensure_dir(const char *path, mode_t mode)
{
	if (mkdir(path, mode) == 0 || errno == EEXIST)
		return 0;

	return -1;
}

static int ensure_dir_recursive(const char *path, mode_t mode)
{
	char buffer[ORLIX_INIT_HOST_MOUNT_TARGET_SIZE];
	size_t length = strnlen(path, sizeof(buffer));

	if (length == 0 || length >= sizeof(buffer) || path[0] != '/')
		return -1;
	memcpy(buffer, path, length + 1);

	for (char *cursor = buffer + 1; *cursor != '\0'; cursor++) {
		if (*cursor != '/')
			continue;
		*cursor = '\0';
		if (ensure_dir(buffer, mode) != 0)
			return -1;
		*cursor = '/';
	}

	return ensure_dir(buffer, mode);
}

static int mount_if_needed(const char *source, const char *target,
			   const char *fstype, unsigned long flags,
			   const void *data)
{
	if (mount(source, target, fstype, flags, data) == 0 || errno == EBUSY)
		return 0;

	return -1;
}

static void mount_device_filesystem(void)
{
	if (ensure_dir("/dev", 0755) == 0 &&
	    mount_if_needed("devtmpfs", "/dev", "devtmpfs", 0, NULL) != 0)
		write_literal(STDERR_FILENO, "orlix-init: mount /dev failed\n");
}

static int install_fd_alias(const char *target, const char *linkpath)
{
	(void)unlink(linkpath);
	if (symlink(target, linkpath) == 0 || errno == EEXIST)
		return 0;

	return -1;
}

static void install_standard_fd_aliases(void)
{
	if (install_fd_alias("/proc/self/fd", "/dev/fd") != 0)
		write_literal(STDERR_FILENO, "orlix-init: /dev/fd alias failed\n");
	if (install_fd_alias("/proc/self/fd/0", "/dev/stdin") != 0)
		write_literal(STDERR_FILENO, "orlix-init: /dev/stdin alias failed\n");
	if (install_fd_alias("/proc/self/fd/1", "/dev/stdout") != 0)
		write_literal(STDERR_FILENO, "orlix-init: /dev/stdout alias failed\n");
	if (install_fd_alias("/proc/self/fd/2", "/dev/stderr") != 0)
		write_literal(STDERR_FILENO, "orlix-init: /dev/stderr alias failed\n");
}

static void mount_runtime_filesystems(void)
{
	if (ensure_dir("/proc", 0555) == 0 &&
	    mount_if_needed("proc", "/proc", "proc", 0, NULL) != 0)
		write_literal(STDERR_FILENO, "orlix-init: mount /proc failed\n");

	if (ensure_dir("/sys", 0555) == 0 &&
	    mount_if_needed("sysfs", "/sys", "sysfs", 0, NULL) != 0)
		write_literal(STDERR_FILENO, "orlix-init: mount /sys failed\n");

	if (ensure_dir("/sys/fs", 0755) == 0 &&
	    ensure_dir("/sys/fs/cgroup", 0755) == 0 &&
	    mount_if_needed("cgroup2", "/sys/fs/cgroup", "cgroup2",
			    MS_NOSUID | MS_NODEV | MS_NOEXEC, NULL) != 0)
		write_literal(STDERR_FILENO,
			      "orlix-init: mount /sys/fs/cgroup failed\n");

	mount_device_filesystem();

	if (ensure_dir("/dev/pts", 0755) == 0 &&
	    mount_if_needed("devpts", "/dev/pts", "devpts", 0,
			    "gid=5,mode=620,ptmxmode=666") != 0)
		write_literal(STDERR_FILENO, "orlix-init: mount /dev/pts failed\n");

	install_standard_fd_aliases();

	if (ensure_dir("/dev/shm", 01777) == 0 &&
	    mount_if_needed("tmpfs", "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV,
			    "mode=1777") != 0)
		write_literal(STDERR_FILENO, "orlix-init: mount /dev/shm failed\n");

	if (ensure_dir("/run", 0755) == 0 &&
	    mount_if_needed("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV,
			    "mode=0755") != 0)
		write_literal(STDERR_FILENO, "orlix-init: mount /run failed\n");

	if (ensure_dir("/tmp", 01777) == 0 &&
	    mount_if_needed("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV,
			    "mode=1777") != 0)
		write_literal(STDERR_FILENO, "orlix-init: mount /tmp failed\n");

	if (ensure_dir("/sys/fs", 0755) == 0 &&
	    ensure_dir("/sys/fs/selinux", 0755) == 0 &&
	    mount_if_needed("selinuxfs", "/sys/fs/selinux", "selinuxfs",
			    MS_NOSUID | MS_NOEXEC, NULL) != 0)
		write_literal(STDERR_FILENO, "orlix-init: mount /sys/fs/selinux failed\n");
}

static int linux_mount_target_is_allowed(const char *target)
{
	static const char *const reserved[] = {
		"/dev",
		"/proc",
		"/run",
		"/sys",
		"/tmp",
		NULL,
	};

	if (target[0] != '/' || target[1] == '\0')
		return 0;
	for (const char *cursor = target; *cursor != '\0'; cursor++) {
		if (*cursor == '\0')
			return 0;
		if (*cursor == '/' && cursor[1] == '/')
			return 0;
		if (*cursor == '.' &&
		    (cursor == target + 1 || cursor[-1] == '/') &&
		    cursor[1] == '.' &&
		    (cursor[2] == '/' || cursor[2] == '\0'))
			return 0;
	}
	for (const char *const *entry = reserved; *entry != NULL; entry++) {
		size_t length = strlen(*entry);

		if (strcmp(target, *entry) == 0 ||
		    (strncmp(target, *entry, length) == 0 &&
		     target[length] == '/'))
			return 0;
	}
	return 1;
}

static int cgroup_path_is_valid(const char *path)
{
	if (path[0] != '/' || path[1] == '\0')
		return 0;
	for (const char *cursor = path; *cursor != '\0'; cursor++) {
		if (*cursor == '\n' || *cursor == '\r')
			return 0;
		if (*cursor == '/' && cursor[1] == '/')
			return 0;
		if (*cursor == '.' && (cursor == path + 1 || cursor[-1] == '/') &&
		    (cursor[1] == '/' || cursor[1] == '\0'))
			return 0;
		if (*cursor == '.' && (cursor == path + 1 || cursor[-1] == '/') &&
		    cursor[1] == '.' && (cursor[2] == '/' || cursor[2] == '\0'))
			return 0;
	}
	return 1;
}

static int runtime_path_is_valid(const char *path)
{
	if (path[0] != '/' || path[1] == '\0')
		return 0;
	for (const char *cursor = path; *cursor != '\0'; cursor++) {
		if (*cursor == '\n' || *cursor == '\r')
			return 0;
		if (*cursor == '/' && cursor[1] == '/')
			return 0;
		if (*cursor == '.' && (cursor == path + 1 || cursor[-1] == '/') &&
		    (cursor[1] == '/' || cursor[1] == '\0'))
			return 0;
		if (*cursor == '.' && (cursor == path + 1 || cursor[-1] == '/') &&
		    cursor[1] == '.' && (cursor[2] == '/' || cursor[2] == '\0'))
			return 0;
	}
	return 1;
}

static int ensure_parent_directory(const char *path)
{
	char parent[ORLIX_INIT_VALUE_SIZE];
	char *slash;
	size_t length;

	length = strnlen(path, sizeof(parent));
	if (length == 0 || length >= sizeof(parent))
		return -1;
	memcpy(parent, path, length + 1);
	slash = strrchr(parent, '/');
	if (slash == NULL || slash == parent)
		return 0;
	*slash = '\0';
	return ensure_dir_recursive(parent, 0755);
}

static int parse_sysctl_assignment(char *assignment,
				   struct orlix_sysctl_config *sysctl)
{
	char *separator = strchr(assignment, '=');
	size_t key_length;
	size_t value_length;

	if (separator == NULL || separator == assignment)
		return -1;
	*separator++ = '\0';

	key_length = strlen(assignment);
	value_length = strlen(separator);
	if (key_length >= sizeof(sysctl->key) ||
	    value_length >= sizeof(sysctl->value))
		return -1;
	if (assignment[0] == '.' || assignment[key_length - 1] == '.')
		return -1;
	if (strstr(assignment, "..") != NULL)
		return -1;
	for (size_t index = 0; index < key_length; index++) {
		char value = assignment[index];

		if (!((value >= 'a' && value <= 'z') ||
		      (value >= 'A' && value <= 'Z') ||
		      (value >= '0' && value <= '9') || value == '_' ||
		      value == '-' || value == '.'))
			return -1;
	}

	memcpy(sysctl->key, assignment, key_length + 1);
	memcpy(sysctl->value, separator, value_length + 1);
	return 0;
}

static void sysctl_proc_path(const char *key, char *path, size_t path_size)
{
	int length = snprintf(path, path_size, "/proc/sys/%s", key);

	if (length < 0 || (size_t)length >= path_size)
		die("sysctl path too long");
	for (char *cursor = path + strlen("/proc/sys/"); *cursor != '\0';
	     cursor++) {
		if (*cursor == '.')
			*cursor = '/';
	}
}

static void apply_sysctl(const struct orlix_sysctl_config *sysctl)
{
	char path[ORLIX_INIT_VALUE_SIZE];
	int fd;

	sysctl_proc_path(sysctl->key, path, sizeof(path));
	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		die("open sysctl");
	if (write_all(fd, sysctl->value, strlen(sysctl->value)) != 0) {
		close(fd);
		die("write sysctl");
	}
	close(fd);
}

static void apply_device_node(const struct orlix_device_node_config *node)
{
	mode_t mode = (mode_t)(node->mode & 07777);
	mode_t type_bits;

	if (!runtime_path_is_valid(node->path))
		die("invalid device path");
	if (ensure_parent_directory(node->path) != 0)
		die("create device parent");
	switch (node->type) {
	case 'c':
	case 'u':
		type_bits = S_IFCHR;
		break;
	case 'b':
		type_bits = S_IFBLK;
		break;
	case 'p':
		type_bits = S_IFIFO;
		break;
	default:
		die("invalid device type");
	}
	if (node->type == 'p') {
		if (mkfifo(node->path, mode) != 0 && errno != EEXIST)
			die("mkfifo device");
	} else if (mknod(node->path, type_bits | mode,
			 makedev(node->major, node->minor)) != 0 &&
		   errno != EEXIST) {
		die("mknod device");
	}
	if (chmod(node->path, mode) != 0)
		die("chmod device");
	if (chown(node->path, (uid_t)node->uid, (gid_t)node->gid) != 0)
		die("chown device");
}

static void write_decimal_to_buffer(char *buffer, size_t buffer_size,
				    unsigned long value)
{
	char reversed[32];
	size_t count = 0;
	size_t out = 0;

	if (buffer_size == 0)
		return;
	if (value == 0)
		reversed[count++] = '0';
	while (value > 0 && count < sizeof(reversed)) {
		reversed[count++] = (char)('0' + (value % 10));
		value /= 10;
	}
	while (count > 0 && out + 1 < buffer_size)
		buffer[out++] = reversed[--count];
	if (out + 1 < buffer_size)
		buffer[out++] = '\n';
	buffer[out] = '\0';
}

static void join_configured_cgroup(const char *path)
{
	char directory[ORLIX_INIT_CGROUP_PATH_SIZE + 16];
	char procs[ORLIX_INIT_CGROUP_PATH_SIZE + 32];
	char pid_buffer[32];
	int fd;

	if (!cgroup_path_is_valid(path))
		die("invalid cgroups path");
	if (snprintf(directory, sizeof(directory), "/sys/fs/cgroup%s", path) >=
	    (int)sizeof(directory))
		die("cgroups path too long");
	if (ensure_dir_recursive(directory, 0755) != 0)
		die("create cgroup path");
	if (snprintf(procs, sizeof(procs), "%s/cgroup.procs", directory) >=
	    (int)sizeof(procs))
		die("cgroup.procs path too long");
	write_decimal_to_buffer(pid_buffer, sizeof(pid_buffer),
				(unsigned long)getpid());
	fd = open(procs, O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		die("open cgroup.procs");
	if (write_all(fd, pid_buffer, strlen(pid_buffer)) != 0) {
		close(fd);
		die("write cgroup.procs");
	}
	close(fd);
}

static void cgroup_control_path(const char *directory, const char *file,
				char *buffer, size_t buffer_size)
{
	if (snprintf(buffer, buffer_size, "%s/%s", directory, file) >=
	    (int)buffer_size)
		die("cgroup control path too long");
}

static void write_cgroup_control(const char *directory, const char *file,
				 const char *value)
{
	char path[ORLIX_INIT_CGROUP_PATH_SIZE + 64];
	int fd;

	cgroup_control_path(directory, file, path, sizeof(path));
	fd = open(path, O_WRONLY | O_CLOEXEC);
	if (fd < 0)
		die("open cgroup control");
	if (write_all(fd, value, strlen(value)) != 0) {
		close(fd);
		die("write cgroup control");
	}
	close(fd);
}

static void enable_cgroup_controller(const char *path, const char *controller)
{
	char directory[ORLIX_INIT_CGROUP_PATH_SIZE + 16] = "/sys/fs/cgroup";
	size_t used = strlen(directory);
	const char *cursor = path + 1;

	while (*cursor != '\0') {
		const char *slash = strchr(cursor, '/');
		size_t component_length = slash ? (size_t)(slash - cursor) :
						 strlen(cursor);

		write_cgroup_control(directory, "cgroup.subtree_control", controller);
		if (!slash)
			break;
		if (used + 1 + component_length >= sizeof(directory))
			die("cgroup path too long");
		directory[used++] = '/';
		memcpy(&directory[used], cursor, component_length);
		used += component_length;
		directory[used] = '\0';
		if (ensure_dir_recursive(directory, 0755) != 0)
			die("create cgroup path");
		cursor = slash + 1;
	}
}

static void enable_cgroup_pids_controller(const char *path)
{
	enable_cgroup_controller(path, "+pids\n");
}

static void enable_cgroup_cpu_controller(const char *path)
{
enable_cgroup_controller(path, "+cpu\n");
}

static void enable_cgroup_memory_controller(const char *path)
{
	enable_cgroup_controller(path, "+memory\n");
}

static void enable_cgroup_io_controller(const char *path)
{
	enable_cgroup_controller(path, "+io\n");
}

static void apply_cgroup_pids_limit(const char *path, const char *value)
{
char directory[ORLIX_INIT_CGROUP_PATH_SIZE + 16];

	if (!cgroup_path_is_valid(path))
		die("invalid cgroups path");
	if (strchr(value, '\n') != NULL || strchr(value, '\r') != NULL ||
	    value[0] == '\0')
		die("invalid cgroup pids limit");
	if (snprintf(directory, sizeof(directory), "/sys/fs/cgroup%s", path) >=
	    (int)sizeof(directory))
		die("cgroups path too long");
	enable_cgroup_pids_controller(path);
	if (ensure_dir_recursive(directory, 0755) != 0)
		die("create cgroup path");
	write_cgroup_control(directory, "pids.max", value);
}

static void apply_cgroup_cpu_settings(const char *path,
				      const char *max_value,
				      int has_max,
				      const char *weight_value,
				      int has_weight)
{
	char directory[ORLIX_INIT_CGROUP_PATH_SIZE + 16];

	if (!cgroup_path_is_valid(path))
		die("invalid cgroups path");
	if (has_max && (strchr(max_value, '\n') != NULL ||
			strchr(max_value, '\r') != NULL ||
			max_value[0] == '\0'))
		die("invalid cgroup cpu max");
	if (has_weight && (strchr(weight_value, '\n') != NULL ||
			   strchr(weight_value, '\r') != NULL ||
			   weight_value[0] == '\0'))
		die("invalid cgroup cpu weight");
	if (snprintf(directory, sizeof(directory), "/sys/fs/cgroup%s", path) >=
	    (int)sizeof(directory))
		die("cgroups path too long");
	enable_cgroup_cpu_controller(path);
	if (ensure_dir_recursive(directory, 0755) != 0)
		die("create cgroup path");
	if (has_max)
		write_cgroup_control(directory, "cpu.max", max_value);
	if (has_weight)
		write_cgroup_control(directory, "cpu.weight", weight_value);
}

static void apply_cgroup_memory_max(const char *path, const char *value)
{
char directory[ORLIX_INIT_CGROUP_PATH_SIZE + 16];

if (!cgroup_path_is_valid(path))
die("invalid cgroups path");
if (strchr(value, '\n') != NULL || strchr(value, '\r') != NULL ||
value[0] == '\0')
die("invalid cgroup memory max");
if (snprintf(directory, sizeof(directory), "/sys/fs/cgroup%s", path) >=
(int)sizeof(directory))
die("cgroups path too long");
enable_cgroup_memory_controller(path);
if (ensure_dir_recursive(directory, 0755) != 0)
die("create cgroup path");
	write_cgroup_control(directory, "memory.max", value);
}

static void apply_cgroup_io_weight(const char *path, const char *value)
{
	char directory[ORLIX_INIT_CGROUP_PATH_SIZE + 16];
	char weight[ORLIX_INIT_CGROUP_VALUE_SIZE + 16];

	if (!cgroup_path_is_valid(path))
		die("invalid cgroups path");
	if (strchr(value, '\n') != NULL || strchr(value, '\r') != NULL ||
	    value[0] == '\0')
		die("invalid cgroup io weight");
	if (snprintf(directory, sizeof(directory), "/sys/fs/cgroup%s", path) >=
	    (int)sizeof(directory))
		die("cgroups path too long");
	if (snprintf(weight, sizeof(weight), "default %s", value) >=
	    (int)sizeof(weight))
		die("cgroup io weight too long");
	enable_cgroup_io_controller(path);
	if (ensure_dir_recursive(directory, 0755) != 0)
		die("create cgroup path");
	write_cgroup_control(directory, "io.weight", weight);
}

static const char *cgroup_controller_for_file(const char *file)
{
	if (strcmp(file, "pids.max") == 0)
		return "+pids\n";
	if (strcmp(file, "cpu.max") == 0 || strcmp(file, "cpu.weight") == 0)
		return "+cpu\n";
	if (strcmp(file, "memory.max") == 0)
		return "+memory\n";
	if (strcmp(file, "io.weight") == 0 || strcmp(file, "io.max") == 0)
		return "+io\n";
	return NULL;
}

static void apply_cgroup_unified_entry(const char *path, const char *file,
				       const char *value)
{
	char directory[ORLIX_INIT_CGROUP_PATH_SIZE + 16];
	const char *controller = cgroup_controller_for_file(file);

	if (!cgroup_path_is_valid(path))
		die("invalid cgroups path");
	if (controller == NULL || strchr(file, '/') != NULL ||
	    strchr(file, '\n') != NULL || strchr(file, '\r') != NULL ||
	    file[0] == '\0')
		die("invalid cgroup unified file");
	if (strchr(value, '\n') != NULL || strchr(value, '\r') != NULL ||
	    value[0] == '\0')
		die("invalid cgroup unified value");
	if (snprintf(directory, sizeof(directory), "/sys/fs/cgroup%s", path) >=
	    (int)sizeof(directory))
		die("cgroups path too long");
	enable_cgroup_controller(path, controller);
	if (ensure_dir_recursive(directory, 0755) != 0)
		die("create cgroup path");
	write_cgroup_control(directory, file, value);
}

static int parse_cgroup_unified_assignment(
	char *assignment,
	struct orlix_cgroup_unified_config *entry)
{
	char *separator = strchr(assignment, '=');
	size_t file_length;

	if (separator == NULL || separator == assignment ||
	    separator[1] == '\0')
		return -1;
	*separator = '\0';
	file_length = strlen(assignment);
	if (file_length >= sizeof(entry->file) ||
	    strlen(separator + 1) >= sizeof(entry->value))
		return -1;
	strcpy(entry->file, assignment);
	strcpy(entry->value, separator + 1);
	return 0;
}

static int parse_time_offset_assignment(char *assignment,
					struct orlix_time_offset_config *entry)
{
	char *secs_text;
	char *nanosecs_text;
	char *end = NULL;
	size_t clock_length;

	secs_text = strchr(assignment, ':');
	if (secs_text == NULL || secs_text == assignment)
		return -1;
	*secs_text++ = '\0';
	nanosecs_text = strchr(secs_text, ':');
	if (nanosecs_text == NULL || nanosecs_text == secs_text)
		return -1;
	*nanosecs_text++ = '\0';
	if (*nanosecs_text == '\0')
		return -1;
	if (strcmp(assignment, "monotonic") != 0 &&
	    strcmp(assignment, "boottime") != 0)
		return -1;
	clock_length = strlen(assignment);
	if (clock_length >= sizeof(entry->clock))
		return -1;
	errno = 0;
	entry->secs = strtol(secs_text, &end, 10);
	if (errno != 0 || end == secs_text || *end != '\0')
		return -1;
	errno = 0;
	entry->nanosecs = strtol(nanosecs_text, &end, 10);
	if (errno != 0 || end == nanosecs_text || *end != '\0' ||
	    entry->nanosecs < 0 || entry->nanosecs >= 1000000000L)
		return -1;
	strcpy(entry->clock, assignment);
	return 0;
}

static int parse_id_mapping_assignment(char *assignment,
				       struct orlix_id_mapping_config *entry)
{
	char *host_text;
	char *size_text;
	char *end = NULL;

	host_text = strchr(assignment, ':');
	if (host_text == NULL || host_text == assignment)
		return -1;
	*host_text++ = '\0';
	size_text = strchr(host_text, ':');
	if (size_text == NULL || size_text == host_text)
		return -1;
	*size_text++ = '\0';
	if (*size_text == '\0')
		return -1;
	errno = 0;
	entry->container_id = strtoul(assignment, &end, 10);
	if (errno != 0 || end == assignment || *end != '\0')
		return -1;
	errno = 0;
	entry->host_id = strtoul(host_text, &end, 10);
	if (errno != 0 || end == host_text || *end != '\0')
		return -1;
	errno = 0;
	entry->size = strtoul(size_text, &end, 10);
	if (errno != 0 || end == size_text || *end != '\0' || entry->size == 0)
		return -1;
	return 0;
}

static unsigned long namespace_flag_for_name(const char *name)
{
if (strcmp(name, "mount") == 0)
		return CLONE_NEWNS;
	if (strcmp(name, "ipc") == 0)
		return CLONE_NEWIPC;
	if (strcmp(name, "uts") == 0)
		return CLONE_NEWUTS;
	if (strcmp(name, "network") == 0)
		return CLONE_NEWNET;
	if (strcmp(name, "cgroup") == 0)
		return CLONE_NEWCGROUP;
	if (strcmp(name, "pid") == 0)
		return CLONE_NEWPID;
	if (strcmp(name, "time") == 0)
		return CLONE_NEWTIME;
	if (strcmp(name, "user") == 0)
		return CLONE_NEWUSER;
	return 0;
}

static int mount_configured_host_directory(int index)
{
	char key[48];
	char source[32];
	char target[ORLIX_INIT_HOST_MOUNT_TARGET_SIZE];
	unsigned long read_only = 0;
	unsigned long no_exec = 0;
	unsigned long flags = MS_NOSUID | MS_NODEV;

	snprintf(key, sizeof(key), "orlix.mount.host%d.target=", index);
	if (read_cmdline_decoded(key, target, sizeof(target)) != 0)
		return 0;
	if (!linux_mount_target_is_allowed(target))
		die("invalid host mount target");

	snprintf(key, sizeof(key), "orlix.mount.host%d.readonly=", index);
	if (read_cmdline_unsigned(key, &read_only) == 0 &&
	    read_only != 0)
		flags |= MS_RDONLY;

	snprintf(key, sizeof(key), "orlix.mount.host%d.noexec=", index);
	if (read_cmdline_unsigned(key, &no_exec) == 0 &&
	    no_exec != 0)
		flags |= MS_NOEXEC;

	if (ensure_dir_recursive(target, 0755) != 0)
		die("create host mount target");
	snprintf(source, sizeof(source), "orlix-host%d", index);
	if (mount_if_needed(source, target, "virtiofs", flags, NULL) != 0)
		die("mount host directory");
	return 1;
}

static int tmpfs_mount_target_is_allowed(const char *target)
{
	static const char *const reserved[] = {
		"/dev",
		"/proc",
		"/sys",
		"/sys/fs/cgroup",
		NULL,
	};

	if (target[0] != '/' || target[1] == '\0')
		return 0;
	for (const char *cursor = target; *cursor != '\0'; cursor++) {
		if (*cursor == '/' && cursor[1] == '/')
			return 0;
		if (*cursor == '.' &&
		    (cursor == target + 1 || cursor[-1] == '/') &&
		    (cursor[1] == '/' || cursor[1] == '\0'))
			return 0;
		if (*cursor == '.' &&
		    (cursor == target + 1 || cursor[-1] == '/') &&
		    cursor[1] == '.' &&
		    (cursor[2] == '/' || cursor[2] == '\0'))
			return 0;
	}
	for (const char *const *entry = reserved; *entry != NULL; entry++) {
		size_t length = strlen(*entry);

		if (strcmp(target, *entry) == 0 ||
		    (strncmp(target, *entry, length) == 0 &&
		     target[length] == '/'))
			return 0;
	}
	return 1;
}

static int mount_configured_tmpfs(int index)
{
	char key[48];
	char target[ORLIX_INIT_HOST_MOUNT_TARGET_SIZE];
	char data[ORLIX_INIT_VALUE_SIZE];
	unsigned long value = 0;
	unsigned long flags = 0;
	const char *mount_data = NULL;

	snprintf(key, sizeof(key), "orlix.mount.tmpfs%d.target=", index);
	if (read_cmdline_decoded(key, target, sizeof(target)) != 0)
		return 0;
	if (!tmpfs_mount_target_is_allowed(target))
		die("invalid tmpfs mount target");

	snprintf(key, sizeof(key), "orlix.mount.tmpfs%d.readonly=", index);
	if (read_cmdline_unsigned(key, &value) == 0 && value != 0)
		flags |= MS_RDONLY;
	snprintf(key, sizeof(key), "orlix.mount.tmpfs%d.nosuid=", index);
	if (read_cmdline_unsigned(key, &value) == 0 && value != 0)
		flags |= MS_NOSUID;
	snprintf(key, sizeof(key), "orlix.mount.tmpfs%d.nodev=", index);
	if (read_cmdline_unsigned(key, &value) == 0 && value != 0)
		flags |= MS_NODEV;
	snprintf(key, sizeof(key), "orlix.mount.tmpfs%d.noexec=", index);
	if (read_cmdline_unsigned(key, &value) == 0 && value != 0)
		flags |= MS_NOEXEC;

	snprintf(key, sizeof(key), "orlix.mount.tmpfs%d.data=", index);
	if (read_cmdline_decoded(key, data, sizeof(data)) == 0)
		mount_data = data;

	if (ensure_dir_recursive(target, 0755) != 0)
		die("create tmpfs mount target");
	if (mount_if_needed("tmpfs", target, "tmpfs", flags, mount_data) != 0)
		die("mount tmpfs");
	return 1;
}

static void mount_configured_tmpfs_mounts(void)
{
	for (int i = 0; i < ORLIX_INIT_MAX_TMPFS_MOUNTS; i++) {
		if (!mount_configured_tmpfs(i))
			break;
	}
}

static void mount_configured_host_directories(void)
{
	for (int i = 0; i < ORLIX_INIT_MAX_HOST_DIRECTORIES; i++) {
		if (!mount_configured_host_directory(i))
			break;
	}
}

static void make_transport_raw(int fd)
{
	struct termios termios;

	if (tcgetattr(fd, &termios) != 0) {
		write_literal(STDERR_FILENO,
			      "orlix-init: tcgetattr transport failed\n");
		return;
	}

	termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	termios.c_oflag &= ~(OPOST);
	termios.c_cflag |= CS8;
	termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
	termios.c_cc[VMIN] = 1;
	termios.c_cc[VTIME] = 0;

	if (tcsetattr(fd, TCSAFLUSH, &termios) != 0)
		write_literal(STDERR_FILENO,
			      "orlix-init: tcsetattr transport failed\n");
}

static int open_pty_master(void)
{
	int fd = open("/dev/ptmx", O_RDWR | O_NOCTTY);

	if (fd >= 0)
		return fd;

	return open("/dev/pts/ptmx", O_RDWR | O_NOCTTY);
}

static int open_pty_slave(int master)
{
	char path[64];
	unsigned int pty_number = 0;
	int unlock = 0;
	int length;

	if (ioctl(master, TIOCSPTLCK, &unlock) != 0) {
		write_literal(STDERR_FILENO, "orlix-init: unlock PTY failed\n");
		return -1;
	}

	if (ioctl(master, TIOCGPTN, &pty_number) != 0) {
		write_literal(STDERR_FILENO, "orlix-init: get PTY number failed\n");
		return -1;
	}

	length = snprintf(path, sizeof(path), "/dev/pts/%u", pty_number);
	if (length < 0 || (size_t)length >= sizeof(path)) {
		write_literal(STDERR_FILENO, "orlix-init: PTY path overflow\n");
		return -1;
	}

	return open(path, O_RDWR | O_NOCTTY);
}

static void apply_initial_pty_winsize(int master, int slave)
{
	unsigned long rows = 0;
	unsigned long columns = 0;
	struct winsize size;

	if (read_cmdline_unsigned("orlix.terminal.rows=", &rows) != 0 ||
	    read_cmdline_unsigned("orlix.terminal.cols=", &columns) != 0)
		return;
	if (rows == 0 || rows > USHRT_MAX ||
	    columns == 0 || columns > USHRT_MAX)
		return;

	memset(&size, 0, sizeof(size));
	size.ws_row = (unsigned short)rows;
	size.ws_col = (unsigned short)columns;
	if (ioctl(slave, TIOCSWINSZ, &size) != 0 &&
	    ioctl(master, TIOCSWINSZ, &size) != 0)
		write_literal(STDERR_FILENO,
			      "orlix-init: set PTY window size failed\n");
}

static void install_stdio(int fd)
{
	for (int target = STDIN_FILENO; target <= STDERR_FILENO; target++) {
		if (fd != target)
			dup2(fd, target);
	}

	if (fd > STDERR_FILENO)
		close(fd);
}

static int read_cmdline_value(const char *key, char *value, size_t value_size)
{
	char *buffer;
	ssize_t bytes;
	size_t key_length = strlen(key);
	char *cursor;
	int fd;

	if (value_size == 0)
		return -1;
	value[0] = '\0';
	buffer = malloc(ORLIX_INIT_CMDLINE_SIZE);
	if (buffer == NULL)
		return -1;

	fd = open("/proc/cmdline", O_RDONLY);
	if (fd < 0) {
		free(buffer);
		return -1;
	}

	do {
		bytes = read(fd, buffer, ORLIX_INIT_CMDLINE_SIZE - 1);
	} while (bytes < 0 && errno == EINTR);
	close(fd);

	if (bytes <= 0) {
		free(buffer);
		return -1;
	}

	if (buffer[bytes - 1] == '\n')
		bytes--;
	buffer[bytes] = '\0';
	cursor = buffer;
	while (*cursor != '\0') {
		char *end = cursor;
		size_t token_length;

		while (*end != '\0' && *end != ' ')
			end++;
		token_length = (size_t)(end - cursor);

		if (token_length >= key_length &&
		    strncmp(cursor, key, key_length) == 0) {
			size_t length = token_length - key_length;

			if (length >= value_size) {
				free(buffer);
				return -1;
			}
			memcpy(value, cursor + key_length, length);
			value[length] = '\0';
			free(buffer);
			return 0;
		}

		cursor = end;
		while (*cursor == ' ')
			cursor++;
	}

	free(buffer);
	return -1;
}

static int hex_value(char value)
{
	if (value >= '0' && value <= '9')
		return value - '0';
	if (value >= 'a' && value <= 'f')
		return value - 'a' + 10;
	if (value >= 'A' && value <= 'F')
		return value - 'A' + 10;

	return -1;
}

static int percent_decode(char *value)
{
	char *read = value;
	char *write = value;

	while (*read != '\0') {
		if (*read == '%') {
			int high = hex_value(read[1]);
			int low = hex_value(read[2]);

			if (high < 0 || low < 0)
				return -1;
			*write++ = (char)((high << 4) | low);
			read += 3;
			continue;
		}
		*write++ = *read++;
	}
	*write = '\0';
	return 0;
}

static int read_cmdline_decoded(const char *key, char *value,
				size_t value_size)
{
	if (read_cmdline_value(key, value, value_size) != 0)
		return -1;

	return percent_decode(value);
}

static int parse_rlimit_number(const char *value, rlim_t *out)
{
	char *end = NULL;
	unsigned long long parsed;

	errno = 0;
	parsed = strtoull(value, &end, 10);
	if (errno != 0 || end == value || *end != '\0')
		return -1;
	*out = (rlim_t)parsed;
	return 0;
}

static int rlimit_resource_from_name(const char *name, int *resource)
{
#ifdef RLIMIT_AS
	if (strcmp(name, "RLIMIT_AS") == 0) {
		*resource = RLIMIT_AS;
		return 0;
	}
#endif
#ifdef RLIMIT_CORE
	if (strcmp(name, "RLIMIT_CORE") == 0) {
		*resource = RLIMIT_CORE;
		return 0;
	}
#endif
#ifdef RLIMIT_CPU
	if (strcmp(name, "RLIMIT_CPU") == 0) {
		*resource = RLIMIT_CPU;
		return 0;
	}
#endif
#ifdef RLIMIT_DATA
	if (strcmp(name, "RLIMIT_DATA") == 0) {
		*resource = RLIMIT_DATA;
		return 0;
	}
#endif
#ifdef RLIMIT_FSIZE
	if (strcmp(name, "RLIMIT_FSIZE") == 0) {
		*resource = RLIMIT_FSIZE;
		return 0;
	}
#endif
#ifdef RLIMIT_LOCKS
	if (strcmp(name, "RLIMIT_LOCKS") == 0) {
		*resource = RLIMIT_LOCKS;
		return 0;
	}
#endif
#ifdef RLIMIT_MEMLOCK
	if (strcmp(name, "RLIMIT_MEMLOCK") == 0) {
		*resource = RLIMIT_MEMLOCK;
		return 0;
	}
#endif
#ifdef RLIMIT_MSGQUEUE
	if (strcmp(name, "RLIMIT_MSGQUEUE") == 0) {
		*resource = RLIMIT_MSGQUEUE;
		return 0;
	}
#endif
#ifdef RLIMIT_NICE
	if (strcmp(name, "RLIMIT_NICE") == 0) {
		*resource = RLIMIT_NICE;
		return 0;
	}
#endif
#ifdef RLIMIT_NOFILE
	if (strcmp(name, "RLIMIT_NOFILE") == 0) {
		*resource = RLIMIT_NOFILE;
		return 0;
	}
#endif
#ifdef RLIMIT_NPROC
	if (strcmp(name, "RLIMIT_NPROC") == 0) {
		*resource = RLIMIT_NPROC;
		return 0;
	}
#endif
#ifdef RLIMIT_RSS
	if (strcmp(name, "RLIMIT_RSS") == 0) {
		*resource = RLIMIT_RSS;
		return 0;
	}
#endif
#ifdef RLIMIT_RTPRIO
	if (strcmp(name, "RLIMIT_RTPRIO") == 0) {
		*resource = RLIMIT_RTPRIO;
		return 0;
	}
#endif
#ifdef RLIMIT_RTTIME
	if (strcmp(name, "RLIMIT_RTTIME") == 0) {
		*resource = RLIMIT_RTTIME;
		return 0;
	}
#endif
#ifdef RLIMIT_SIGPENDING
	if (strcmp(name, "RLIMIT_SIGPENDING") == 0) {
		*resource = RLIMIT_SIGPENDING;
		return 0;
	}
#endif
#ifdef RLIMIT_STACK
	if (strcmp(name, "RLIMIT_STACK") == 0) {
		*resource = RLIMIT_STACK;
		return 0;
	}
#endif
	return -1;
}

static int parse_rlimit_value(char *value, struct orlix_rlimit_config *limit)
{
	char *soft_text;
	char *hard_text;

	soft_text = strchr(value, ':');
	if (soft_text == NULL)
		return -1;
	*soft_text++ = '\0';
	hard_text = strchr(soft_text, ':');
	if (hard_text == NULL)
		return -1;
	*hard_text++ = '\0';
	if (strchr(hard_text, ':') != NULL)
		return -1;
	if (rlimit_resource_from_name(value, &limit->resource) != 0)
		return -1;
	if (parse_rlimit_number(soft_text, &limit->soft) != 0 ||
	    parse_rlimit_number(hard_text, &limit->hard) != 0)
		return -1;
	if (limit->soft > limit->hard)
		return -1;
	return 0;
}

static int read_cmdline_unsigned(const char *key, unsigned long *value)
{
	char buffer[32];
	char *end = NULL;
	unsigned long parsed;

	if (read_cmdline_value(key, buffer, sizeof(buffer)) != 0)
		return -1;

	errno = 0;
	parsed = strtoul(buffer, &end, 10);
	if (errno != 0 || end == buffer || *end != '\0')
		return -1;

	*value = parsed;
	return 0;
}

static int read_cmdline_signed(const char *key, long *value)
{
	int fd = open("/proc/cmdline", O_RDONLY);
	if (fd < 0)
		return -1;
	char buffer[4096];
	ssize_t nread = read(fd, buffer, sizeof(buffer) - 1);
	close(fd);
	if (nread <= 0)
		return -1;
	buffer[nread] = '\0';

	size_t key_length = strlen(key);
	char *cursor = buffer;
	while (*cursor != '\0') {
		while (*cursor == ' ')
			cursor++;
		if (strncmp(cursor, key, key_length) == 0) {
			char *end = NULL;
			errno = 0;
			long parsed = strtol(cursor + key_length, &end, 10);
			if (errno != 0 || end == cursor + key_length)
				return -1;
			*value = parsed;
			return 0;
		}
		while (*cursor != '\0' && *cursor != ' ')
			cursor++;
	}
	return -1;
}

static int scheduler_policy_from_name(const char *policy)
{
	if (strcmp(policy, "SCHED_OTHER") == 0)
		return SCHED_OTHER;
	if (strcmp(policy, "SCHED_BATCH") == 0)
		return SCHED_BATCH;
	if (strcmp(policy, "SCHED_IDLE") == 0)
		return SCHED_IDLE;
	if (strcmp(policy, "SCHED_FIFO") == 0)
		return SCHED_FIFO;
	if (strcmp(policy, "SCHED_RR") == 0)
		return SCHED_RR;
	return -1;
}

static int io_priority_class_from_name(const char *priority_class)
{
	if (strcmp(priority_class, "IOPRIO_CLASS_RT") == 0)
		return IOPRIO_CLASS_RT;
	if (strcmp(priority_class, "IOPRIO_CLASS_BE") == 0)
		return IOPRIO_CLASS_BE;
	if (strcmp(priority_class, "IOPRIO_CLASS_IDLE") == 0)
		return IOPRIO_CLASS_IDLE;
	return -1;
}

static int personality_from_domain(const char *domain, unsigned long *personality)
{
	if (strcmp(domain, "LINUX") == 0) {
		*personality = PER_LINUX;
		return 0;
	}
	if (strcmp(domain, "LINUX32") == 0) {
		*personality = PER_LINUX32;
		return 0;
	}
	return -1;
}

static int parse_cpu_affinity(const char *value, cpu_set_t *set)
{
	CPU_ZERO(set);
	const char *cursor = value;
	while (*cursor != '\0') {
		errno = 0;
		char *end = NULL;
		unsigned long start = strtoul(cursor, &end, 10);
		if (errno != 0 || end == cursor || start >= CPU_SETSIZE)
			return -1;
		unsigned long last = start;
		if (*end == '-') {
			cursor = end + 1;
			errno = 0;
			last = strtoul(cursor, &end, 10);
			if (errno != 0 || end == cursor || last < start || last >= CPU_SETSIZE)
				return -1;
		}
		for (unsigned long cpu = start; cpu <= last; ++cpu)
			CPU_SET((int)cpu, set);
		if (*end == '\0')
			return 0;
		if (*end != ',')
			return -1;
		cursor = end + 1;
		if (*cursor == '\0')
			return -1;
	}
	return -1;
}

static int valid_exec_path(const char *path)
{
	if (path[0] == '\0')
		return 0;
	if (path[0] == '/')
		return 1;
	if (strchr(path, '/') != NULL)
		return 0;

	return 1;
}

static int valid_working_directory(const char *path)
{
	return path[0] == '/';
}

static int valid_environment_assignment(const char *value)
{
	const char *separator = strchr(value, '=');

	if (separator == NULL || separator == value)
		return 0;

	return 1;
}

enum {
	ORLIX_INIT_MAX_ARGS = 16,
	ORLIX_INIT_MAX_ENV = 32,
};

#define ORLIX_CAPABILITY_WORDS _LINUX_CAPABILITY_U32S_3
#define ORLIX_CAPABILITY_BUFFER_SIZE 512

static void die(const char *message)
{
	static const char prefix[] = "orlix-init: ";
	static const char suffix[] = "\n";

	(void)write(STDERR_FILENO, prefix, sizeof(prefix) - 1);
	(void)write(STDERR_FILENO, message, strlen(message));
	(void)write(STDERR_FILENO, suffix, sizeof(suffix) - 1);
	_exit(127);
}

struct orlix_capability_set {
	int present;
	__u32 words[ORLIX_CAPABILITY_WORDS];
};

struct orlix_capability_sets {
	struct orlix_capability_set bounding;
	struct orlix_capability_set permitted;
	struct orlix_capability_set inheritable;
	struct orlix_capability_set effective;
	struct orlix_capability_set ambient;
};

static int capability_index_for_name(const char *name)
{
	static const char *const capability_names[] = {
		"CAP_CHOWN",
		"CAP_DAC_OVERRIDE",
		"CAP_DAC_READ_SEARCH",
		"CAP_FOWNER",
		"CAP_FSETID",
		"CAP_KILL",
		"CAP_SETGID",
		"CAP_SETUID",
		"CAP_SETPCAP",
		"CAP_LINUX_IMMUTABLE",
		"CAP_NET_BIND_SERVICE",
		"CAP_NET_BROADCAST",
		"CAP_NET_ADMIN",
		"CAP_NET_RAW",
		"CAP_IPC_LOCK",
		"CAP_IPC_OWNER",
		"CAP_SYS_MODULE",
		"CAP_SYS_RAWIO",
		"CAP_SYS_CHROOT",
		"CAP_SYS_PTRACE",
		"CAP_SYS_PACCT",
		"CAP_SYS_ADMIN",
		"CAP_SYS_BOOT",
		"CAP_SYS_NICE",
		"CAP_SYS_RESOURCE",
		"CAP_SYS_TIME",
		"CAP_SYS_TTY_CONFIG",
		"CAP_MKNOD",
		"CAP_LEASE",
		"CAP_AUDIT_WRITE",
		"CAP_AUDIT_CONTROL",
		"CAP_SETFCAP",
		"CAP_MAC_OVERRIDE",
		"CAP_MAC_ADMIN",
		"CAP_SYSLOG",
		"CAP_WAKE_ALARM",
		"CAP_BLOCK_SUSPEND",
		"CAP_AUDIT_READ",
		"CAP_PERFMON",
		"CAP_BPF",
		"CAP_CHECKPOINT_RESTORE",
	};

	for (size_t index = 0; index < sizeof(capability_names) / sizeof(capability_names[0]); index++) {
		if (strcmp(name, capability_names[index]) == 0)
			return (int)index;
	}
	return -1;
}

static int parse_capability_list(const char *value, struct orlix_capability_set *set)
{
	char buffer[ORLIX_CAPABILITY_BUFFER_SIZE];
	size_t length = strnlen(value, sizeof(buffer));

	if (length >= sizeof(buffer))
		return 0;

	memset(set->words, 0, sizeof(set->words));
	set->present = 1;
	memcpy(buffer, value, length + 1);

	char *cursor = buffer;
	while (cursor != NULL) {
		char *next = strchr(cursor, ',');
		if (next != NULL)
			*next++ = '\0';

		if (*cursor == '\0') {
			cursor = next;
			continue;
		}

		int capability = capability_index_for_name(cursor);
		if (capability < 0 || capability > CAP_LAST_CAP)
			return 0;

		size_t word = (size_t)capability / 32;
		if (word >= ORLIX_CAPABILITY_WORDS)
			return 0;
		set->words[word] |= CAP_TO_MASK(capability);
		cursor = next;
	}

	return 1;
}

static int capability_set_contains(const struct orlix_capability_set *set, int capability)
{
	size_t word = (size_t)capability / 32;
	if (word >= ORLIX_CAPABILITY_WORDS)
		return 0;
	return (set->words[word] & CAP_TO_MASK(capability)) != 0;
}

static int final_capability_set_present(const struct orlix_capability_sets *sets)
{
	return sets->permitted.present ||
	       sets->inheritable.present ||
	       sets->effective.present ||
	       sets->ambient.present;
}

static void apply_capability_bounding_set(const struct orlix_capability_sets *sets)
{
	if (!sets->bounding.present)
		return;

	for (int capability = 0; capability <= CAP_LAST_CAP; capability++) {
		if (!capability_set_contains(&sets->bounding, capability) &&
		    prctl(PR_CAPBSET_DROP, capability, 0, 0, 0) != 0)
			die("prctl(PR_CAPBSET_DROP)");
	}
}

static void apply_final_capability_sets(const struct orlix_capability_sets *sets)
{
	if (!final_capability_set_present(sets))
		return;

	if (sets->permitted.present || sets->inheritable.present || sets->effective.present) {
		struct __user_cap_header_struct header = {
			.version = _LINUX_CAPABILITY_VERSION_3,
			.pid = 0,
		};
		struct __user_cap_data_struct data[ORLIX_CAPABILITY_WORDS];

		memset(data, 0, sizeof(data));
		if (syscall(SYS_capget, &header, data) != 0)
			die("capget");

		for (size_t index = 0; index < ORLIX_CAPABILITY_WORDS; index++) {
			if (sets->permitted.present)
				data[index].permitted = sets->permitted.words[index];
			if (sets->inheritable.present)
				data[index].inheritable = sets->inheritable.words[index];
			if (sets->effective.present)
				data[index].effective = sets->effective.words[index];
		}

		if (syscall(SYS_capset, &header, data) != 0)
			die("capset");
	}

	if (sets->ambient.present) {
		if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0) != 0)
			die("prctl(PR_CAP_AMBIENT_CLEAR_ALL)");
		for (int capability = 0; capability <= CAP_LAST_CAP; capability++) {
			if (capability_set_contains(&sets->ambient, capability) &&
			    prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, capability, 0, 0) != 0)
				die("prctl(PR_CAP_AMBIENT_RAISE)");
		}
	}
}

struct orlix_command_config {
	char argv_storage[ORLIX_INIT_MAX_ARGS][ORLIX_INIT_VALUE_SIZE];
	char env_storage[ORLIX_INIT_MAX_ENV][ORLIX_INIT_VALUE_SIZE];
	char cwd[ORLIX_INIT_VALUE_SIZE];
	char hostname[ORLIX_INIT_VALUE_SIZE];
	char domainname[ORLIX_INIT_VALUE_SIZE];
	struct orlix_rlimit_config rlimits[ORLIX_INIT_MAX_RLIMITS];
	struct orlix_sysctl_config sysctls[ORLIX_INIT_MAX_SYSCTLS];
	char *argv[ORLIX_INIT_MAX_ARGS + 2];
	char *envp[ORLIX_INIT_MAX_ENV + 1];
	int argc;
	int envc;
	int has_hostname;
	int has_domainname;
	int rlimitc;
	size_t sysctl_count;
	unsigned long uid;
	unsigned long gid;
	unsigned long supplementary_groups[ORLIX_INIT_MAX_SUPPLEMENTARY_GROUPS];
	size_t supplementary_group_count;
	struct orlix_capability_sets capabilities;
	int no_new_privileges;
	int close_additional_fds;
	int has_oom_score_adjustment;
	long oom_score_adjustment;
	int has_scheduler;
	int scheduler_policy;
	int scheduler_priority;
	int has_io_priority;
	int io_priority_class;
	int io_priority_priority;
	int has_cpu_affinity;
	cpu_set_t cpu_affinity;
	unsigned long umask_value;
	int has_umask;
	unsigned long personality;
	int has_personality;
	char cgroups_path[ORLIX_INIT_CGROUP_PATH_SIZE];
	int has_cgroups_path;
char cgroup_pids_max[ORLIX_INIT_CGROUP_VALUE_SIZE];
int has_cgroup_pids_max;
char cgroup_cpu_max[ORLIX_INIT_CGROUP_VALUE_SIZE];
int has_cgroup_cpu_max;
char cgroup_cpu_weight[ORLIX_INIT_CGROUP_VALUE_SIZE];
int has_cgroup_cpu_weight;
	char cgroup_memory_max[ORLIX_INIT_CGROUP_VALUE_SIZE];
	int has_cgroup_memory_max;
	char cgroup_io_weight[ORLIX_INIT_CGROUP_VALUE_SIZE];
	int has_cgroup_io_weight;
	struct orlix_cgroup_unified_config
		cgroup_unified[ORLIX_INIT_MAX_CGROUP_UNIFIED];
	size_t cgroup_unified_count;
	struct orlix_device_node_config device_nodes[ORLIX_INIT_MAX_DEVICE_NODES];
	size_t device_node_count;
	struct orlix_time_offset_config time_offsets[ORLIX_INIT_MAX_TIME_OFFSETS];
	size_t time_offset_count;
	struct orlix_id_mapping_config uid_mappings[ORLIX_INIT_MAX_ID_MAPPINGS];
	size_t uid_mapping_count;
	struct orlix_id_mapping_config gid_mappings[ORLIX_INIT_MAX_ID_MAPPINGS];
	size_t gid_mapping_count;
	unsigned long namespace_flags;
	unsigned long namespace_join_flags[ORLIX_INIT_MAX_NAMESPACE_JOINS];
	char namespace_join_paths[ORLIX_INIT_MAX_NAMESPACE_JOINS][ORLIX_INIT_VALUE_SIZE];
	size_t namespace_join_count;
};

static void selected_command_config(struct orlix_command_config *config)
{
	char key[32];
	char exec_path[ORLIX_INIT_VALUE_SIZE];

	memset(config, 0, sizeof(*config));
	strncpy(config->argv_storage[0], "/bin/sh", ORLIX_INIT_VALUE_SIZE);
	strncpy(config->argv_storage[1], "-i", ORLIX_INIT_VALUE_SIZE);
	config->argv[0] = config->argv_storage[0];
	config->argv[1] = config->argv_storage[1];
	config->argc = 2;

	strncpy(config->env_storage[0], "HOME=/root", ORLIX_INIT_VALUE_SIZE);
	strncpy(config->env_storage[1],
		"PATH=/bin:/usr/bin:/sbin:/usr/sbin",
		ORLIX_INIT_VALUE_SIZE);
	strncpy(config->env_storage[2], "TERM=xterm-256color",
		ORLIX_INIT_VALUE_SIZE);
	config->envp[0] = config->env_storage[0];
	config->envp[1] = config->env_storage[1];
	config->envp[2] = config->env_storage[2];
	config->envc = 3;
	strncpy(config->cwd, "/", ORLIX_INIT_VALUE_SIZE);

	if (read_cmdline_decoded("orlix.exec=", exec_path, sizeof(exec_path)) != 0 ||
	    !valid_exec_path(exec_path))
		return;

	config->argc = 0;
	for (int index = 0; index < ORLIX_INIT_MAX_ARGS; index++) {
		snprintf(key, sizeof(key), "orlix.argv%d=", index);
		if (read_cmdline_decoded(key, config->argv_storage[index],
					 ORLIX_INIT_VALUE_SIZE) != 0)
			break;
		if (index == 0 && !valid_exec_path(config->argv_storage[index]))
			break;
		config->argv[index] = config->argv_storage[index];
		config->argc++;
	}
	if (config->argc == 0) {
		strncpy(config->argv_storage[0], exec_path, ORLIX_INIT_VALUE_SIZE);
		config->argv[0] = config->argv_storage[0];
		config->argc = 1;
	}
	if (config->argc == 1 && strcmp(config->argv[0], "/bin/sh") == 0) {
		strncpy(config->argv_storage[1], "-i", ORLIX_INIT_VALUE_SIZE);
		config->argv[1] = config->argv_storage[1];
		config->argc = 2;
	}
	config->argv[config->argc] = NULL;

	config->envc = 0;
	for (int index = 0; index < ORLIX_INIT_MAX_ENV; index++) {
		snprintf(key, sizeof(key), "orlix.env%d=", index);
		if (read_cmdline_decoded(key, config->env_storage[index],
					 ORLIX_INIT_VALUE_SIZE) != 0)
			break;
		if (!valid_environment_assignment(config->env_storage[index]))
			break;
		config->envp[index] = config->env_storage[index];
		config->envc++;
	}
	if (config->envc == 0) {
		strncpy(config->env_storage[0], "HOME=/root",
			ORLIX_INIT_VALUE_SIZE);
		strncpy(config->env_storage[1],
			"PATH=/bin:/usr/bin:/sbin:/usr/sbin",
			ORLIX_INIT_VALUE_SIZE);
		strncpy(config->env_storage[2], "TERM=xterm-256color",
			ORLIX_INIT_VALUE_SIZE);
		config->envp[0] = config->env_storage[0];
		config->envp[1] = config->env_storage[1];
		config->envp[2] = config->env_storage[2];
		config->envc = 3;
	}
	config->envp[config->envc] = NULL;

	if (read_cmdline_decoded("orlix.cwd=", config->cwd,
				 sizeof(config->cwd)) != 0 ||
	    !valid_working_directory(config->cwd))
		strncpy(config->cwd, "/", sizeof(config->cwd));

	(void)read_cmdline_unsigned("orlix.uid=", &config->uid);
	(void)read_cmdline_unsigned("orlix.gid=", &config->gid);
	for (int index = 0; index < ORLIX_INIT_MAX_SUPPLEMENTARY_GROUPS; ++index) {
		char key[32];
		unsigned long group_id = 0;
		snprintf(key, sizeof(key), "orlix.suppgid%d=", index);
		if (read_cmdline_unsigned(key, &group_id) != 0)
			break;
		config->supplementary_groups[config->supplementary_group_count++] = group_id;
	}
	unsigned long no_new_privileges = 0;
	char capability_value[ORLIX_CAPABILITY_BUFFER_SIZE];
	if (read_cmdline_decoded("orlix.cap.bounding=", capability_value, sizeof(capability_value)) == 0 &&
	    !parse_capability_list(capability_value, &config->capabilities.bounding))
		die("invalid orlix.cap.bounding");
	if (read_cmdline_decoded("orlix.cap.permitted=", capability_value, sizeof(capability_value)) == 0 &&
	    !parse_capability_list(capability_value, &config->capabilities.permitted))
		die("invalid orlix.cap.permitted");
	if (read_cmdline_decoded("orlix.cap.inheritable=", capability_value, sizeof(capability_value)) == 0 &&
	    !parse_capability_list(capability_value, &config->capabilities.inheritable))
		die("invalid orlix.cap.inheritable");
	if (read_cmdline_decoded("orlix.cap.effective=", capability_value, sizeof(capability_value)) == 0 &&
	    !parse_capability_list(capability_value, &config->capabilities.effective))
		die("invalid orlix.cap.effective");
	if (read_cmdline_decoded("orlix.cap.ambient=", capability_value, sizeof(capability_value)) == 0 &&
	    !parse_capability_list(capability_value, &config->capabilities.ambient))
		die("invalid orlix.cap.ambient");

	if (read_cmdline_unsigned("orlix.nonewprivs=", &no_new_privileges) == 0 && no_new_privileges != 0)
		config->no_new_privileges = 1;
	unsigned long close_additional_fds = 0;
	if (read_cmdline_unsigned("orlix.closefds=", &close_additional_fds) == 0 && close_additional_fds != 0)
		config->close_additional_fds = 1;
	long oom_score_adjustment = 0;
	if (read_cmdline_signed("orlix.oomscoreadj=", &oom_score_adjustment) == 0 &&
	    oom_score_adjustment >= ORLIX_INIT_OOM_SCORE_ADJ_MIN &&
	    oom_score_adjustment <= ORLIX_INIT_OOM_SCORE_ADJ_MAX) {
		config->oom_score_adjustment = oom_score_adjustment;
		config->has_oom_score_adjustment = 1;
	}
	char scheduler_policy[32];
	unsigned long scheduler_priority = 0;
	if (read_cmdline_decoded("orlix.scheduler.policy=", scheduler_policy, sizeof(scheduler_policy)) == 0 &&
	    read_cmdline_unsigned("orlix.scheduler.priority=", &scheduler_priority) == 0) {
		int policy = scheduler_policy_from_name(scheduler_policy);
		if (policy >= 0 && scheduler_priority <= INT_MAX) {
			config->scheduler_policy = policy;
			config->scheduler_priority = (int)scheduler_priority;
			config->has_scheduler = 1;
		}
	}
	char io_priority_class[32];
	unsigned long io_priority_priority = 0;
	if (read_cmdline_decoded("orlix.ioprio.class=", io_priority_class, sizeof(io_priority_class)) == 0 &&
	    read_cmdline_unsigned("orlix.ioprio.priority=", &io_priority_priority) == 0) {
		int priority_class = io_priority_class_from_name(io_priority_class);
		if (priority_class >= 0 && io_priority_priority <= 7) {
			config->io_priority_class = priority_class;
			config->io_priority_priority = (int)io_priority_priority;
			config->has_io_priority = 1;
		}
	}
	char cpu_affinity[128];
	if (read_cmdline_decoded("orlix.cpuaffinity=", cpu_affinity, sizeof(cpu_affinity)) == 0 &&
	    parse_cpu_affinity(cpu_affinity, &config->cpu_affinity) == 0)
		config->has_cpu_affinity = 1;
	char personality_domain[32];
	if (read_cmdline_decoded("orlix.personality=", personality_domain,
				 sizeof(personality_domain)) == 0 &&
	    personality_from_domain(personality_domain, &config->personality) == 0)
		config->has_personality = 1;
	if (read_cmdline_unsigned("orlix.umask=", &config->umask_value) == 0)
		config->has_umask = 1;
	for (int i = 0; i < ORLIX_INIT_MAX_SYSCTLS; i++) {
		char key[32];
		char assignment[ORLIX_INIT_VALUE_SIZE * 2];

		snprintf(key, sizeof(key), "orlix.sysctl%d=", i);
		if (read_cmdline_decoded(key, assignment, sizeof(assignment)) != 0)
			break;
		if (parse_sysctl_assignment(
			    assignment, &config->sysctls[config->sysctl_count]) != 0)
			die("invalid sysctl");
		config->sysctl_count++;
	}
	if (read_cmdline_decoded("orlix.cgroups.path=", config->cgroups_path,
				 sizeof(config->cgroups_path)) == 0 &&
	    config->cgroups_path[0] != '\0')
		config->has_cgroups_path = 1;
	if (read_cmdline_decoded("orlix.cgroups.pids.max=",
		config->cgroup_pids_max,
		sizeof(config->cgroup_pids_max)) == 0 &&
	    config->cgroup_pids_max[0] != '\0')
		config->has_cgroup_pids_max = 1;
if (read_cmdline_decoded("orlix.cgroups.cpu.max=",
config->cgroup_cpu_max,
sizeof(config->cgroup_cpu_max)) == 0 &&
config->cgroup_cpu_max[0] != '\0')
config->has_cgroup_cpu_max = 1;
if (read_cmdline_decoded("orlix.cgroups.cpu.weight=",
config->cgroup_cpu_weight,
sizeof(config->cgroup_cpu_weight)) == 0 &&
config->cgroup_cpu_weight[0] != '\0')
config->has_cgroup_cpu_weight = 1;
	if (read_cmdline_decoded("orlix.cgroups.memory.max=",
	    config->cgroup_memory_max,
	    sizeof(config->cgroup_memory_max)) == 0 &&
	    config->cgroup_memory_max[0] != '\0')
		config->has_cgroup_memory_max = 1;
	if (read_cmdline_decoded("orlix.cgroups.io.weight=",
	    config->cgroup_io_weight,
	    sizeof(config->cgroup_io_weight)) == 0 &&
	    config->cgroup_io_weight[0] != '\0')
		config->has_cgroup_io_weight = 1;
	for (int i = 0; i < ORLIX_INIT_MAX_CGROUP_UNIFIED; i++) {
		char key[40];
		char assignment[ORLIX_INIT_CGROUP_FILE_SIZE +
				ORLIX_INIT_CGROUP_VALUE_SIZE + 2];
		snprintf(key, sizeof(key), "orlix.cgroups.unified%d=", i);
		if (read_cmdline_decoded(key, assignment, sizeof(assignment)) != 0)
			break;
		if (parse_cgroup_unified_assignment(
			    assignment,
			    &config->cgroup_unified[config->cgroup_unified_count]) !=
		    0)
			die("invalid cgroup unified assignment");
		config->cgroup_unified_count++;
	}
	for (int i = 0; i < ORLIX_INIT_MAX_DEVICE_NODES; i++) {
		char key[40];
		char type[8];
		struct orlix_device_node_config *node =
			&config->device_nodes[config->device_node_count];

		snprintf(key, sizeof(key), "orlix.device.path%d=", i);
		if (read_cmdline_decoded(key, node->path,
					 sizeof(node->path)) != 0)
			break;
		snprintf(key, sizeof(key), "orlix.device.type%d=", i);
		if (read_cmdline_decoded(key, type, sizeof(type)) != 0 ||
		    type[0] == '\0' || type[1] != '\0')
			die("invalid device type");
		node->type = type[0];
		snprintf(key, sizeof(key), "orlix.device.major%d=", i);
		if (read_cmdline_unsigned(key, &node->major) != 0)
			die("invalid device major");
		snprintf(key, sizeof(key), "orlix.device.minor%d=", i);
		if (read_cmdline_unsigned(key, &node->minor) != 0)
			die("invalid device minor");
		snprintf(key, sizeof(key), "orlix.device.mode%d=", i);
		if (read_cmdline_unsigned(key, &node->mode) != 0)
			die("invalid device mode");
		snprintf(key, sizeof(key), "orlix.device.uid%d=", i);
		if (read_cmdline_unsigned(key, &node->uid) != 0)
			die("invalid device uid");
		snprintf(key, sizeof(key), "orlix.device.gid%d=", i);
		if (read_cmdline_unsigned(key, &node->gid) != 0)
			die("invalid device gid");
		config->device_node_count++;
	}
	for (int i = 0; i < ORLIX_INIT_MAX_TIME_OFFSETS; i++) {
		char key[32];
		char assignment[ORLIX_INIT_VALUE_SIZE];
		snprintf(key, sizeof(key), "orlix.timeoffset%d=", i);
		if (read_cmdline_decoded(key, assignment, sizeof(assignment)) != 0)
			break;
		if (parse_time_offset_assignment(
			    assignment,
			    &config->time_offsets[config->time_offset_count]) != 0)
			die("invalid time offset");
		config->time_offset_count++;
	}
	for (int i = 0; i < ORLIX_INIT_MAX_ID_MAPPINGS; i++) {
		char key[32];
		char assignment[ORLIX_INIT_VALUE_SIZE];
		snprintf(key, sizeof(key), "orlix.uidmap%d=", i);
		if (read_cmdline_decoded(key, assignment, sizeof(assignment)) != 0)
			break;
		if (parse_id_mapping_assignment(
			    assignment,
			    &config->uid_mappings[config->uid_mapping_count]) != 0)
			die("invalid uid mapping");
		config->uid_mapping_count++;
	}
	for (int i = 0; i < ORLIX_INIT_MAX_ID_MAPPINGS; i++) {
		char key[32];
		char assignment[ORLIX_INIT_VALUE_SIZE];
		snprintf(key, sizeof(key), "orlix.gidmap%d=", i);
		if (read_cmdline_decoded(key, assignment, sizeof(assignment)) != 0)
			break;
		if (parse_id_mapping_assignment(
			    assignment,
			    &config->gid_mappings[config->gid_mapping_count]) != 0)
			die("invalid gid mapping");
		config->gid_mapping_count++;
	}
	for (int i = 0; i < ORLIX_INIT_MAX_NAMESPACES; i++) {
		char key[32];
		char value[ORLIX_INIT_VALUE_SIZE];
		unsigned long flag;

		snprintf(key, sizeof(key), "orlix.namespace%d=", i);
		if (read_cmdline_decoded(key, value, sizeof(value)) != 0)
			continue;
		flag = namespace_flag_for_name(value);
		if (flag == 0)
			die("invalid namespace");
		config->namespace_flags |= flag;
	}
	for (int i = 0; i < ORLIX_INIT_MAX_NAMESPACE_JOINS; i++) {
		char key[32];
		char value[ORLIX_INIT_VALUE_SIZE];
		char *separator;
		unsigned long flag;

		snprintf(key, sizeof(key), "orlix.namespacepath%d=", i);
		if (read_cmdline_decoded(key, value, sizeof(value)) != 0)
			continue;
		separator = strchr(value, '=');
		if (separator == NULL || separator == value || separator[1] == '\0')
			die("invalid namespace path");
		*separator = '\0';
		flag = namespace_flag_for_name(value);
		if (flag == 0)
			die("invalid namespace path type");
		if (config->namespace_join_count >= ORLIX_INIT_MAX_NAMESPACE_JOINS)
			die("too many namespace paths");
		config->namespace_join_flags[config->namespace_join_count] = flag;
		strncpy(config->namespace_join_paths[config->namespace_join_count],
			separator + 1,
			sizeof(config->namespace_join_paths[config->namespace_join_count]) - 1);
		config->namespace_join_count++;
	}
	for (int i = 0; i < ORLIX_INIT_MAX_RLIMITS; i++) {
		char key[32];
		char value[ORLIX_INIT_VALUE_SIZE];

		snprintf(key, sizeof(key), "orlix.rlimit%d=", i);
		if (read_cmdline_value(key, value, sizeof(value)) != 0)
			continue;
		if (parse_rlimit_value(value, &config->rlimits[config->rlimitc]) != 0) {
			write_literal(STDERR_FILENO,
				      "orlix-init: invalid rlimit config\n");
			_exit(127);
		}
		config->rlimitc++;
	}
	if (read_cmdline_decoded("orlix.hostname=", config->hostname,
				 sizeof(config->hostname)) == 0 &&
	    config->hostname[0] != '\0')
		config->has_hostname = 1;
	if (read_cmdline_decoded("orlix.domainname=", config->domainname,
				 sizeof(config->domainname)) == 0 &&
	    config->domainname[0] != '\0')
		config->has_domainname = 1;

	config->cwd[sizeof(config->cwd) - 1] = '\0';
	config->hostname[sizeof(config->hostname) - 1] = '\0';
	config->domainname[sizeof(config->domainname) - 1] = '\0';
}

static int copy_available(int input_fd, int output_fd)
{
	unsigned char buffer[4096];
	ssize_t bytes;

	do {
		bytes = read(input_fd, buffer, sizeof(buffer));
	} while (bytes < 0 && errno == EINTR);

	if (bytes <= 0)
		return -1;

	return write_all(output_fd, buffer, (size_t)bytes);
}

static int copy_available_or_eof(int input_fd, int output_fd)
{
	unsigned char buffer[4096];
	ssize_t bytes;

	do {
		bytes = read(input_fd, buffer, sizeof(buffer));
	} while (bytes < 0 && errno == EINTR);

	if (bytes < 0)
		return -1;
	if (bytes == 0)
		return 1;

	return write_all(output_fd, buffer, (size_t)bytes);
}

static int shell_exit_status(int status)
{
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	if (WIFSIGNALED(status))
		return 128 + WTERMSIG(status);

	return 1;
}

static int reap_shell_if_exited(pid_t shell, int *status)
{
	int child_status;
	pid_t reaped;

	do {
		reaped = waitpid(shell, &child_status, WNOHANG);
	} while (reaped < 0 && errno == EINTR);

	if (reaped == 0)
		return 0;
	if (reaped != shell)
		return -1;

	*status = child_status;
	return 1;
}

static int wait_for_shell_exit(pid_t shell, int *status)
{
	int child_status;
	pid_t reaped;

	do {
		reaped = waitpid(shell, &child_status, 0);
	} while (reaped < 0 && errno == EINTR);

	if (reaped != shell)
		return -1;

	*status = child_status;
	return 1;
}

static void write_shell_exit_status(int exit_status)
{
	write_literal(STDERR_FILENO, "orlix-init: shell exit status=");
	write_unsigned_decimal(STDERR_FILENO, (unsigned long)exit_status);
	write_literal(STDERR_FILENO, "\n");
}

static int relay_pty(int console_fd, int master, pid_t shell, int *child_status)
{
	struct pollfd fds[] = {
		{
			.fd = console_fd,
			.events = POLLIN,
		},
		{
			.fd = master,
			.events = POLLIN,
		},
	};

	for (;;) {
		int status = 0;
		int reaped = reap_shell_if_exited(shell, &status);
		int ready;

		if (reaped > 0) {
			*child_status = status;
			return shell_exit_status(status);
		}
		if (reaped < 0)
			return 1;

		do {
			ready = poll(fds, 2, 100);
		} while (ready < 0 && errno == EINTR);

		if (ready < 0)
			return 1;

		short console_revents = fds[0].revents;
		short pty_revents = fds[1].revents;

		if ((console_revents & POLLIN) != 0) {
			int copy_status = copy_available_or_eof(console_fd, master);
			if (copy_status > 0) {
				fds[0].fd = -1;
				console_revents = 0;
			} else if (copy_status < 0) {
				write_literal(STDERR_FILENO,
					      "orlix-init: console relay failed\n");
				return 1;
			}
		}

		if ((pty_revents & POLLIN) != 0) {
			int copy_status = copy_available_or_eof(master, STDOUT_FILENO);
			if (copy_status > 0) {
				reaped = wait_for_shell_exit(shell, &status);
				if (reaped > 0) {
					*child_status = status;
					return shell_exit_status(status);
				}
				return 1;
			}
			if (copy_status < 0) {
				write_literal(STDERR_FILENO,
					      "orlix-init: pty relay failed\n");
				return 1;
			}
		}

		if ((console_revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			fds[0].fd = -1;
			console_revents = 0;
		}

		if ((pty_revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			reaped = reap_shell_if_exited(shell, &status);
			if (reaped == 0)
				reaped = wait_for_shell_exit(shell, &status);
			if (reaped > 0) {
				*child_status = status;
				return shell_exit_status(status);
			}
			return 1;
		}
	}
}

static const char *configured_path(char *const envp[])
{
	for (char *const *entry = envp; *entry != NULL; entry++) {
		if (strncmp(*entry, "PATH=", 5) == 0)
			return *entry + 5;
	}

	return "/bin:/usr/bin";
}

static void exec_path_candidate(const char *directory, size_t directory_length,
				const char *command, char *const argv[],
				char *const envp[])
{
	char candidate[ORLIX_INIT_VALUE_SIZE];
	size_t command_length = strlen(command);

	if (directory_length == 0) {
		if (command_length >= sizeof(candidate))
			return;
		memcpy(candidate, command, command_length + 1);
		execve(candidate, argv, envp);
		return;
	}

	if (directory_length + 1 + command_length >= sizeof(candidate))
		return;
	memcpy(candidate, directory, directory_length);
	candidate[directory_length] = '/';
	memcpy(candidate + directory_length + 1, command, command_length + 1);
	execve(candidate, argv, envp);
}

static void exec_configured_command(struct orlix_command_config *config)
{
	const char *command = config->argv[0];
	const char *path;
	const char *component;

	if (strchr(command, '/') != NULL) {
		execve(command, config->argv, config->envp);
		return;
	}

	path = configured_path(config->envp);
	component = path;
	for (;;) {
		const char *separator = strchr(component, ':');
		size_t length = separator != NULL ?
			(size_t)(separator - component) : strlen(component);

		exec_path_candidate(component, length, command, config->argv,
				    config->envp);
		if (separator == NULL)
			break;
		component = separator + 1;
	}
}

static void apply_namespace_config(const struct orlix_command_config *config)
{
	for (size_t i = 0; i < config->namespace_join_count; i++) {
		int fd;

		if ((config->namespace_flags & config->namespace_join_flags[i]) != 0)
			die("namespace join conflicts with namespace create");
		if (!cgroup_path_is_valid(config->namespace_join_paths[i]))
			die("invalid namespace path");
		fd = open(config->namespace_join_paths[i], O_RDONLY | O_CLOEXEC);
		if (fd < 0)
			die("open namespace path");
		if (syscall(SYS_setns, fd, (int)config->namespace_join_flags[i]) != 0) {
			close(fd);
			die("setns namespace");
		}
		close(fd);
	}
	if (config->namespace_flags == 0)
		return;
	if (unshare((int)config->namespace_flags) != 0)
		die("unshare namespaces");
}

static int write_proc_file(const char *path, const char *value, size_t length)
{
	int fd = open(path, O_WRONLY | O_CLOEXEC);
	int result;

	if (fd < 0)
		return -1;
	result = write_all(fd, value, length);
	close(fd);
	return result;
}

static int write_id_mapping_file(const char *path,
				 const struct orlix_id_mapping_config *mappings,
				 size_t mapping_count)
{
	char buffer[ORLIX_INIT_MAX_ID_MAPPINGS * 48];
	size_t used = 0;

	for (size_t i = 0; i < mapping_count; i++) {
		int length = snprintf(buffer + used, sizeof(buffer) - used,
				      "%lu %lu %lu\n",
				      mappings[i].container_id,
				      mappings[i].host_id,
				      mappings[i].size);
		if (length <= 0 || (size_t)length >= sizeof(buffer) - used)
			return -1;
		used += (size_t)length;
	}
	return write_proc_file(path, buffer, used);
}

static void apply_user_namespace_mappings(const struct orlix_command_config *config)
{
	if (config->uid_mapping_count == 0 && config->gid_mapping_count == 0)
		return;
	if ((config->namespace_flags & CLONE_NEWUSER) == 0)
		die("id mappings without user namespace");
	if (config->uid_mapping_count > 0 &&
	    write_id_mapping_file("/proc/self/uid_map", config->uid_mappings,
				  config->uid_mapping_count) != 0)
		die("write uid_map");
	if (config->gid_mapping_count == 0)
		return;
	if (write_id_mapping_file("/proc/self/gid_map", config->gid_mappings,
				  config->gid_mapping_count) == 0)
		return;
	if (config->supplementary_group_count > 0)
		die("write gid_map");
	if (write_proc_file("/proc/self/setgroups", "deny\n", 5) != 0)
		die("write setgroups deny");
	if (write_id_mapping_file("/proc/self/gid_map", config->gid_mappings,
				  config->gid_mapping_count) != 0)
		die("write gid_map");
}

static void apply_time_offsets(const struct orlix_command_config *config)
{
	if (config->time_offset_count == 0)
		return;
	if ((config->namespace_flags & CLONE_NEWTIME) == 0)
		die("time offsets without time namespace");
	for (size_t i = 0; i < config->time_offset_count; i++) {
		char buffer[96];
		int length = snprintf(buffer, sizeof(buffer), "%s %ld %ld\n",
				      config->time_offsets[i].clock,
				      config->time_offsets[i].secs,
				      config->time_offsets[i].nanosecs);
		int fd;

		if (length <= 0 || (size_t)length >= sizeof(buffer))
			die("format timens_offsets");
		fd = open("/proc/self/timens_offsets", O_WRONLY | O_CLOEXEC);
		if (fd < 0)
			die("open timens_offsets");
		if (write_all(fd, buffer, (size_t)length) != 0) {
			close(fd);
			die("write timens_offsets");
		}
		close(fd);
	}
}

static void apply_uts_config(const struct orlix_command_config *config)
{
	if (!config->has_hostname && !config->has_domainname)
		return;

	if ((config->namespace_flags & CLONE_NEWUTS) == 0 &&
	    unshare(CLONE_NEWUTS) != 0) {
		write_literal(STDERR_FILENO, "orlix-init: unshare UTS failed\n");
		return;
	}
	if (config->has_hostname &&
	    sethostname(config->hostname, strlen(config->hostname)) != 0)
		write_literal(STDERR_FILENO, "orlix-init: sethostname failed\n");
	if (config->has_domainname &&
	    syscall(SYS_setdomainname, config->domainname,
		    strlen(config->domainname)) != 0)
		write_literal(STDERR_FILENO, "orlix-init: setdomainname failed\n");
}

static void apply_rlimits(const struct orlix_command_config *config)
{
	for (int i = 0; i < config->rlimitc; i++) {
		struct rlimit limit;

		limit.rlim_cur = config->rlimits[i].soft;
		limit.rlim_max = config->rlimits[i].hard;
		if (setrlimit(config->rlimits[i].resource, &limit) != 0) {
			write_literal(STDERR_FILENO,
				      "orlix-init: setrlimit failed\n");
			_exit(127);
		}
	}
}

static void close_additional_fds(void)
{
	struct rlimit limit;
	rlim_t max_fd = 1024;
	if (getrlimit(RLIMIT_NOFILE, &limit) == 0 && limit.rlim_cur != RLIM_INFINITY)
		max_fd = limit.rlim_cur;
	if (max_fd > 1048576)
		max_fd = 1048576;
	for (int fd = 3; (rlim_t)fd < max_fd; ++fd)
		close(fd);
}

static void apply_oom_score_adjustment(long value)
{
	char buffer[32];
	int length = snprintf(buffer, sizeof(buffer), "%ld\n", value);
	if (length <= 0 || (size_t)length >= sizeof(buffer))
		return;
	int fd = open("/proc/self/oom_score_adj", O_WRONLY);
	if (fd < 0) {
		write_literal(STDERR_FILENO, "orlix-init: open oom_score_adj failed\n");
		return;
	}
	if (write(fd, buffer, (size_t)length) != length)
		write_literal(STDERR_FILENO, "orlix-init: write oom_score_adj failed\n");
	close(fd);
}

static void apply_scheduler(int policy, int priority)
{
	struct sched_param param;
	memset(&param, 0, sizeof(param));
	param.sched_priority = priority;
	if (sched_setscheduler(0, policy, &param) != 0)
		write_literal(STDERR_FILENO, "orlix-init: sched_setscheduler failed\n");
}

static void apply_io_priority(int priority_class, int priority)
{
	int value = IOPRIO_PRIO_VALUE(priority_class, priority);
	if (syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, 0, value) != 0)
		write_literal(STDERR_FILENO, "orlix-init: ioprio_set failed\n");
}

static void apply_cpu_affinity(const cpu_set_t *set)
{
	if (sched_setaffinity(0, sizeof(*set), set) != 0)
		write_literal(STDERR_FILENO, "orlix-init: sched_setaffinity failed\n");
}

static void apply_personality(unsigned long personality)
{
	if (syscall(SYS_personality, personality) < 0)
		die("personality");
}

static void exec_or_fork_configured_command(struct orlix_command_config *config)
{
	unsigned long fork_required = CLONE_NEWTIME;
	if ((config->namespace_flags & CLONE_NEWPID) != 0)
		fork_required |= CLONE_NEWPID;
	for (size_t i = 0; i < config->namespace_join_count; i++) {
		if ((config->namespace_join_flags[i] & CLONE_NEWPID) != 0)
			fork_required |= CLONE_NEWPID;
	}

	if ((config->namespace_flags & fork_required) == 0 &&
	    (fork_required & CLONE_NEWPID) == 0) {
		exec_configured_command(config);
		return;
	}

	pid_t child = fork();
	if (child < 0)
		die("fork namespace child");
	if (child == 0) {
		exec_configured_command(config);
		write_errno_message(STDERR_FILENO,
				     "orlix-init: exec command failed: ",
				     errno);
		_exit(127);
	}

	int status;
	while (waitpid(child, &status, 0) < 0) {
		if (errno != EINTR)
			_exit(127);
	}
	_exit(shell_exit_status(status));
}

static int terminal_enabled(void)
{
	char value[16];

	if (read_cmdline_decoded("orlix.terminal=", value, sizeof(value)) != 0)
		return 1;

	return strcmp(value, "0") != 0 && strcmp(value, "false") != 0;
}

static void run_configured_command_child(void)
{
	struct orlix_command_config *config;

	config = calloc(1, sizeof(*config));
	if (config == NULL) {
		write_literal(STDERR_FILENO,
			      "orlix-init: command config allocation failed\n");
		_exit(127);
	}
	selected_command_config(config);
	apply_namespace_config(config);
	apply_user_namespace_mappings(config);
	apply_time_offsets(config);
	apply_uts_config(config);
	if (chdir(config->cwd) != 0)
		write_literal(STDERR_FILENO, "orlix-init: chdir failed\n");
	if (config->has_umask)
		(void)umask((mode_t)config->umask_value);
	for (size_t i = 0; i < config->sysctl_count; i++)
		apply_sysctl(&config->sysctls[i]);
	for (size_t i = 0; i < config->device_node_count; i++)
		apply_device_node(&config->device_nodes[i]);
	if (config->has_cgroup_pids_max) {
		if (!config->has_cgroups_path)
			die("cgroup pids limit without cgroup path");
		apply_cgroup_pids_limit(config->cgroups_path,
			config->cgroup_pids_max);
	}
	if (config->has_cgroup_cpu_max || config->has_cgroup_cpu_weight) {
		if (!config->has_cgroups_path)
			die("cgroup cpu setting without cgroup path");
		apply_cgroup_cpu_settings(config->cgroups_path,
			config->cgroup_cpu_max,
			config->has_cgroup_cpu_max,
			config->cgroup_cpu_weight,
			config->has_cgroup_cpu_weight);
	}
	if (config->has_cgroup_memory_max) {
		if (!config->has_cgroups_path)
			die("cgroup memory limit without cgroup path");
		apply_cgroup_memory_max(config->cgroups_path,
			config->cgroup_memory_max);
	}
	if (config->has_cgroup_io_weight) {
		if (!config->has_cgroups_path)
			die("cgroup io weight without cgroup path");
		apply_cgroup_io_weight(config->cgroups_path,
			config->cgroup_io_weight);
	}
	for (size_t i = 0; i < config->cgroup_unified_count; i++) {
		if (!config->has_cgroups_path)
			die("cgroup unified without cgroup path");
		apply_cgroup_unified_entry(config->cgroups_path,
			config->cgroup_unified[i].file,
			config->cgroup_unified[i].value);
	}
	if (config->has_cgroups_path)
		join_configured_cgroup(config->cgroups_path);
	apply_rlimits(config);
	apply_capability_bounding_set(&config->capabilities);
	if (final_capability_set_present(&config->capabilities) &&
	    prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0) != 0)
		die("prctl(PR_SET_KEEPCAPS)");

	if (config->supplementary_group_count > 0) {
		gid_t groups[ORLIX_INIT_MAX_SUPPLEMENTARY_GROUPS];
		for (size_t index = 0; index < config->supplementary_group_count; ++index)
			groups[index] = (gid_t)config->supplementary_groups[index];
		if (setgroups(config->supplementary_group_count, groups) != 0)
			write_literal(STDERR_FILENO, "orlix-init: setgroups failed\n");
	}
	if (config->has_oom_score_adjustment)
		apply_oom_score_adjustment(config->oom_score_adjustment);
	if (config->has_scheduler)
		apply_scheduler(config->scheduler_policy, config->scheduler_priority);
	if (config->has_io_priority)
		apply_io_priority(config->io_priority_class, config->io_priority_priority);
	if (config->has_cpu_affinity)
		apply_cpu_affinity(&config->cpu_affinity);
	if (config->has_personality)
		apply_personality(config->personality);
	if (config->close_additional_fds)
		close_additional_fds();
	if (config->gid != 0 && setgid((gid_t)config->gid) != 0)
		write_literal(STDERR_FILENO, "orlix-init: setgid failed\n");
	if (config->uid != 0 && setuid((uid_t)config->uid) != 0)
		write_literal(STDERR_FILENO, "orlix-init: setuid failed\n");

	apply_final_capability_sets(&config->capabilities);
	if (final_capability_set_present(&config->capabilities) &&
	    prctl(PR_SET_KEEPCAPS, 0, 0, 0, 0) != 0)
		die("prctl(PR_SET_KEEPCAPS)");
	if (config->no_new_privileges) {
		if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
			write_literal(STDERR_FILENO, "orlix-init: prctl(PR_SET_NO_NEW_PRIVS) failed\n");
	}
	exec_or_fork_configured_command(config);
	write_errno_message(STDERR_FILENO,
			     "orlix-init: exec command failed: ",
			     errno);
	_exit(127);
}

static pid_t start_command_on_pty(int master, int slave)
{
	pid_t child = fork();

	if (child != 0)
		return child;

	close(master);

	if (setsid() < 0)
		write_literal(STDERR_FILENO, "orlix-init: shell setsid failed\n");

	if (ioctl(slave, TIOCSCTTY, 0) < 0)
		write_literal(STDERR_FILENO,
			      "orlix-init: shell TIOCSCTTY failed\n");

	install_stdio(slave);
	run_configured_command_child();
	_exit(127);
}

static int run_stdio_command(void)
{
	pid_t child = fork();
	int status;

	if (child < 0) {
		write_literal(STDERR_FILENO, "orlix-init: fork command failed\n");
		return 1;
	}
	if (child == 0)
		run_configured_command_child();

	write_process_started(child);
	while (waitpid(child, &status, 0) < 0) {
		if (errno != EINTR)
			return 1;
	}
	write_process_completion(child, status);
	return shell_exit_status(status);
}

static int run_pty_shell(int console_fd)
{
	int master = open_pty_master();
	int slave;
	pid_t shell;
	int status;
	int child_status = -1;

	if (master < 0) {
		write_literal(STDERR_FILENO, "orlix-init: open PTY master failed\n");
		return 1;
	}

	slave = open_pty_slave(master);
	if (slave < 0) {
		close(master);
		return 1;
	}

	apply_initial_pty_winsize(master, slave);
	shell = start_command_on_pty(master, slave);
	if (shell < 0) {
		write_literal(STDERR_FILENO, "orlix-init: fork shell failed\n");
		close(slave);
		close(master);
		return 1;
	}

	close(slave);
	write_process_started(shell);
	make_transport_raw(console_fd);
	status = relay_pty(console_fd, master, shell, &child_status);
	if (child_status < 0) {
		if (wait_for_shell_exit(shell, &child_status) > 0) {
			status = shell_exit_status(child_status);
		} else {
			write_errno_message(STDERR_FILENO,
					    "orlix-init: wait PTY shell failed: ",
					    errno);
		}
	}
	close(master);
	if (child_status >= 0) {
		write_process_completion(shell, child_status);
		write_shell_exit_status(shell_exit_status(child_status));
	}
	return status;
}

int main(void)
{
	int tty;
	int terminal;

	write_literal(STDERR_FILENO, "orlix-init: main entered\n");
	mount_device_filesystem();
	write_literal(STDERR_FILENO, "orlix-init: device filesystem mounted\n");
	terminal = terminal_enabled();
	if (!terminal)
		goto runtime_setup;
	tty = open_controlling_tty();
	if (tty < 0) {
		write_literal(STDERR_FILENO,
			      "orlix-init: unable to open a Linux console\n");
		return 127;
	}

	install_stdio(tty);
	write_literal(STDERR_FILENO, "orlix-init: stdio installed\n");
runtime_setup:
	mount_runtime_filesystems();
	write_literal(STDERR_FILENO, "orlix-init: runtime filesystems mounted\n");
	mount_configured_tmpfs_mounts();
	mount_configured_host_directories();
	if (terminal) {
		if (run_pty_shell(STDIN_FILENO) != 0)
			write_literal(STDERR_FILENO,
				      "orlix-init: PTY shell session ended\n");
	} else if (run_stdio_command() != 0) {
		write_literal(STDERR_FILENO,
			      "orlix-init: stdio command ended\n");
	}

	for (;;)
		pause();
}
