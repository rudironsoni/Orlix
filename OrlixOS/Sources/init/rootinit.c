// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define ORLIX_CMDLINE_BUFFER_SIZE 4096
#define ORLIX_SYSCTL_ASSIGNMENT_SIZE 512
#define ORLIX_SYSCTL_PATH_SIZE 256
#define ORLIX_SYSCTL_KEY_PREFIX "orlix.sysctl"
#define ORLIX_RUNTIME_PATH_SIZE 512
#define ORLIX_MASKED_PATH_KEY_PREFIX "orlix.maskedpath"
#define ORLIX_READONLY_PATH_KEY_PREFIX "orlix.readonlypath"

static void write_literal(int fd, const char *message)
{
	size_t length = 0;

	while (message[length] != '\0')
		length++;

	while (length > 0) {
		ssize_t written = write(fd, message, length);

		if (written <= 0)
			return;

		message += written;
		length -= (size_t)written;
	}
}

static int ensure_dir(const char *path, mode_t mode)
{
	if (mkdir(path, mode) == 0 || errno == EEXIST)
		return 0;

	return -1;
}

static int mount_if_needed(const char *source, const char *target,
			   const char *fstype, unsigned long flags,
			   const void *data)
{
	if (mount(source, target, fstype, flags, data) == 0 || errno == EBUSY)
		return 0;

	return -1;
}

static int read_cmdline(char *buffer, size_t buffer_size)
{
	ssize_t nread;
	int fd;

	fd = open("/proc/cmdline", O_RDONLY);
	if (fd < 0)
		return -1;

	nread = read(fd, buffer, buffer_size - 1);
	close(fd);
	if (nread <= 0)
		return -1;

	buffer[nread] = '\0';
	return 0;
}

static bool is_cmdline_space(char c)
{
	return c == ' ' || c == '\n' || c == '\t';
}

static bool cmdline_has_token(const char *token)
{
	char buffer[ORLIX_CMDLINE_BUFFER_SIZE];
	size_t token_len;
	char *cursor;

	if (read_cmdline(buffer, sizeof(buffer)) != 0)
		return false;

	token_len = strlen(token);
	cursor = buffer;
	while (*cursor != '\0') {
		while (is_cmdline_space(*cursor))
			cursor++;
		if (memcmp(cursor, token, token_len) == 0 &&
		    (cursor[token_len] == '\0' ||
		     is_cmdline_space(cursor[token_len])))
			return true;
		while (*cursor != '\0' && !is_cmdline_space(*cursor))
			cursor++;
	}

	return false;
}

static int cmdline_value_equals(const char *key, const char *value)
{
	char buffer[ORLIX_CMDLINE_BUFFER_SIZE];
	size_t key_len;
	size_t value_len;
	char *cursor;

	if (read_cmdline(buffer, sizeof(buffer)) != 0)
		return 0;

	key_len = strlen(key);
	value_len = strlen(value);
	cursor = buffer;
	while (*cursor != '\0') {
		while (is_cmdline_space(*cursor))
			cursor++;
		if (memcmp(cursor, key, key_len) == 0 &&
		    cursor[key_len] == '=' &&
		    memcmp(cursor + key_len + 1, value, value_len) == 0 &&
		    (cursor[key_len + 1 + value_len] == '\0' ||
		     is_cmdline_space(cursor[key_len + 1 + value_len])))
			return 1;
		while (*cursor != '\0' && !is_cmdline_space(*cursor))
			cursor++;
	}

	return 0;
}

static int hex_value(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

static int percent_decode(char *destination, size_t destination_size,
			  const char *source, size_t source_length)
{
	size_t out = 0;

	for (size_t index = 0; index < source_length; index++) {
		char value = source[index];

		if (out + 1 >= destination_size)
			return -1;
		if (value == '%') {
			int high;
			int low;

			if (index + 2 >= source_length)
				return -1;
			high = hex_value(source[index + 1]);
			low = hex_value(source[index + 2]);
			if (high < 0 || low < 0)
				return -1;
			value = (char)((high << 4) | low);
			index += 2;
		}
		if (value == '\0' || value == '\n' || value == '\r')
			return -1;
		destination[out++] = value;
	}

	destination[out] = '\0';
	return 0;
}

static bool is_sysctl_key_char(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
}

static int sysctl_path_from_key(char *path, size_t path_size, const char *key)
{
	const char *prefix = "/newroot/proc/sys/";
	size_t out = 0;
	bool previous_dot = true;

	while (prefix[out] != '\0') {
		if (out + 1 >= path_size)
			return -1;
		path[out] = prefix[out];
		out++;
	}

	for (size_t index = 0; key[index] != '\0'; index++) {
		char c = key[index];

		if (!is_sysctl_key_char(c))
			return -1;
		if (c == '.') {
			if (previous_dot)
				return -1;
			c = '/';
			previous_dot = true;
		} else {
			previous_dot = false;
		}
		if (out + 1 >= path_size)
			return -1;
		path[out++] = c;
	}
	if (previous_dot)
		return -1;

	path[out] = '\0';
	return 0;
}

static int write_all(int fd, const char *buffer, size_t length)
{
	while (length > 0) {
		ssize_t written = write(fd, buffer, length);

		if (written <= 0)
			return -1;
		buffer += written;
		length -= (size_t)written;
	}

	return 0;
}

static int apply_sysctl_assignment(char *assignment)
{
	char path[ORLIX_SYSCTL_PATH_SIZE];
	char *separator;
	char *value;
	int fd;
	int result;

	separator = strchr(assignment, '=');
	if (separator == NULL || separator == assignment)
		return -1;
	*separator = '\0';
	value = separator + 1;

	if (sysctl_path_from_key(path, sizeof(path), assignment) != 0)
		return -1;

	fd = open(path, O_WRONLY);
	if (fd < 0)
		return -1;

	result = write_all(fd, value, strlen(value));
	close(fd);
	return result;
}

static int apply_configured_sysctls(void)
{
	char buffer[ORLIX_CMDLINE_BUFFER_SIZE];
	char *cursor;
	size_t prefix_len = strlen(ORLIX_SYSCTL_KEY_PREFIX);

	if (read_cmdline(buffer, sizeof(buffer)) != 0)
		return 0;

	cursor = buffer;
	while (*cursor != '\0') {
		char *token_start;
		char *value_start;
		size_t value_length;
		char assignment[ORLIX_SYSCTL_ASSIGNMENT_SIZE];

		while (is_cmdline_space(*cursor))
			cursor++;
		token_start = cursor;
		while (*cursor != '\0' && !is_cmdline_space(*cursor))
			cursor++;
		if (token_start == cursor)
			continue;

		if (memcmp(token_start, ORLIX_SYSCTL_KEY_PREFIX, prefix_len) != 0)
			continue;

		value_start = token_start + prefix_len;
		while (*value_start >= '0' && *value_start <= '9')
			value_start++;
		if (*value_start != '=')
			continue;
		value_start++;
		value_length = (size_t)(cursor - value_start);

		if (percent_decode(assignment, sizeof(assignment), value_start,
				   value_length) != 0)
			return -1;
		if (apply_sysctl_assignment(assignment) != 0)
			return -1;
	}

	return 0;
}

static int newroot_path_from_runtime_path(char *path, size_t path_size,
					  const char *runtime_path)
{
	const char *prefix = "/newroot";
	size_t out = 0;
	size_t component_length = 0;
	size_t component_dot_count = 0;

	if (runtime_path[0] != '/' || runtime_path[1] == '\0')
		return -1;

	while (prefix[out] != '\0') {
		if (out + 1 >= path_size)
			return -1;
		path[out] = prefix[out];
		out++;
	}

	for (size_t index = 0; runtime_path[index] != '\0'; index++) {
		char c = runtime_path[index];

		if (c == '\n' || c == '\r')
			return -1;
		if (c == '/') {
			if ((component_length == 1 || component_length == 2) &&
			    component_length == component_dot_count)
				return -1;
			component_length = 0;
			component_dot_count = 0;
		} else {
			component_length++;
			if (c == '.')
				component_dot_count++;
		}
		if (out + 1 >= path_size)
			return -1;
		path[out++] = c;
	}
	if ((component_length == 1 || component_length == 2) &&
	    component_length == component_dot_count)
		return -1;

	path[out] = '\0';
	return 0;
}

static int remount_bind_readonly(const char *path, bool recursive)
{
	unsigned long flags = MS_BIND | MS_REMOUNT | MS_RDONLY | MS_NOSUID |
			      MS_NODEV | MS_NOEXEC;

	if (recursive)
		flags |= MS_REC;
	return mount(NULL, path, NULL, flags, NULL);
}

static int apply_masked_path(const char *runtime_path)
{
	char path[ORLIX_RUNTIME_PATH_SIZE];
	struct stat st;

	if (newroot_path_from_runtime_path(path, sizeof(path), runtime_path) != 0)
		return -1;
	if (lstat(path, &st) != 0)
		return -1;

	if (S_ISDIR(st.st_mode)) {
		if (mount("tmpfs", path, "tmpfs",
			  MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC,
			  "mode=000,size=0") == 0)
			return 0;
		return -1;
	}

	if (mount("/dev/null", path, NULL, MS_BIND, NULL) != 0)
		return -1;
	return remount_bind_readonly(path, false);
}

static int apply_readonly_path(const char *runtime_path)
{
	char path[ORLIX_RUNTIME_PATH_SIZE];
	struct stat st;

	if (newroot_path_from_runtime_path(path, sizeof(path), runtime_path) != 0)
		return -1;
	if (lstat(path, &st) != 0)
		return -1;
	if (mount(path, path, NULL, MS_BIND | (S_ISDIR(st.st_mode) ? MS_REC : 0),
		  NULL) != 0)
		return -1;
	return remount_bind_readonly(path, S_ISDIR(st.st_mode));
}

static int apply_configured_runtime_paths(const char *prefix,
					  int (*apply_path)(const char *))
{
	char buffer[ORLIX_CMDLINE_BUFFER_SIZE];
	char *cursor;
	size_t prefix_len = strlen(prefix);

	if (read_cmdline(buffer, sizeof(buffer)) != 0)
		return 0;

	cursor = buffer;
	while (*cursor != '\0') {
		char *token_start;
		char *value_start;
		size_t value_length;
		char runtime_path[ORLIX_RUNTIME_PATH_SIZE];

		while (is_cmdline_space(*cursor))
			cursor++;
		token_start = cursor;
		while (*cursor != '\0' && !is_cmdline_space(*cursor))
			cursor++;
		if (token_start == cursor)
			continue;

		if (memcmp(token_start, prefix, prefix_len) != 0)
			continue;

		value_start = token_start + prefix_len;
		while (*value_start >= '0' && *value_start <= '9')
			value_start++;
		if (*value_start != '=')
			continue;
		value_start++;
		value_length = (size_t)(cursor - value_start);

		if (percent_decode(runtime_path, sizeof(runtime_path), value_start,
				   value_length) != 0)
			return -1;
		if (apply_path(runtime_path) != 0)
			return -1;
	}

	return 0;
}

static int apply_new_root_propagation(void)
{
	unsigned long propagation = MS_PRIVATE;

	if (cmdline_value_equals("orlix.root.propagation", "shared"))
		propagation = MS_SHARED;
	else if (cmdline_value_equals("orlix.root.propagation", "slave"))
		propagation = MS_SLAVE;
	else if (cmdline_value_equals("orlix.root.propagation", "unbindable"))
		propagation = MS_UNBINDABLE;

	if (mount(NULL, "/newroot", NULL, MS_REC | propagation, NULL) == 0)
		return 0;

	return -1;
}

static int wait_for_path(const char *path)
{
	for (int attempt = 0; attempt < 250; attempt++) {
		struct timespec delay = {
			.tv_sec = 0,
			.tv_nsec = 20000000,
		};
		int fd = open(path, O_RDONLY);

		if (fd >= 0) {
			close(fd);
			return 0;
		}

		nanosleep(&delay, NULL);
	}

	return -1;
}

static int mount_overlay_root(void)
{
	bool readonly_root;

	if (ensure_dir("/dev", 0755) != 0 ||
	    ensure_dir("/proc", 0755) != 0 ||
	    ensure_dir("/sys", 0755) != 0 ||
	    ensure_dir("/lower", 0755) != 0 ||
	    ensure_dir("/state", 0755) != 0 ||
	    ensure_dir("/newroot", 0755) != 0)
		return -1;

	if (mount_if_needed("devtmpfs", "/dev", "devtmpfs", 0, NULL) != 0 ||
	    mount_if_needed("proc", "/proc", "proc", 0, NULL) != 0 ||
	    mount_if_needed("sysfs", "/sys", "sysfs", 0, NULL) != 0)
		return -1;

	if (mount_if_needed("proc", "/proc", "proc", 0, NULL) != 0)
		return -1;
	readonly_root = cmdline_has_token("orlix.root.readonly=1");

	if (wait_for_path("/dev/vda") != 0 || wait_for_path("/dev/vdb") != 0)
		return -1;

	if (readonly_root)
		return mount_if_needed("/dev/vda", "/newroot", "ext4",
				       MS_RDONLY, NULL);

	if (mount_if_needed("/dev/vda", "/lower", "ext4", MS_RDONLY, NULL) != 0 ||
	    mount_if_needed("/dev/vdb", "/state", "ext4", 0, NULL) != 0)
		return -1;

	if (ensure_dir("/state/upper", 0755) != 0 ||
	    ensure_dir("/state/work", 0755) != 0)
		return -1;

	return mount_if_needed("overlay", "/newroot", "overlay", 0,
			       "lowerdir=/lower,upperdir=/state/upper,workdir=/state/work");
}

static int mount_new_root_api_filesystems(void)
{
	if (ensure_dir("/newroot/dev", 0755) != 0 ||
	    ensure_dir("/newroot/proc", 0755) != 0 ||
	    ensure_dir("/newroot/sys", 0755) != 0)
		return -1;

	return mount_if_needed("devtmpfs", "/newroot/dev", "devtmpfs", 0, NULL) == 0 &&
		       mount_if_needed("proc", "/newroot/proc", "proc", 0, NULL) == 0 &&
		       mount_if_needed("sysfs", "/newroot/sys", "sysfs", 0, NULL) == 0 ?
		0 : -1;
}

static int switch_to_new_root(void)
{
	if (chdir("/newroot") != 0)
		return -1;
	if (mount(".", "/", NULL, MS_MOVE, NULL) != 0)
		return -1;
	if (chroot(".") != 0)
		return -1;

	return chdir("/");
}

int main(void)
{
	char *const argv[] = { "/sbin/init", NULL };
	char *const envp[] = {
		"HOME=/root",
		"PATH=/bin:/usr/bin:/sbin:/usr/sbin",
		"TERM=xterm-256color",
		NULL,
	};

	write_literal(STDERR_FILENO, "ORLIX-ROOTINIT-START\n");

	if (mount_overlay_root() != 0) {
		write_literal(STDERR_FILENO,
			      "orlix-rootinit: overlay root assembly failed\n");
		return 127;
	}
	if (mount_new_root_api_filesystems() != 0) {
		write_literal(STDERR_FILENO,
			      "orlix-rootinit: api filesystem setup failed\n");
		return 127;
	}
	if (apply_new_root_propagation() != 0) {
		write_literal(STDERR_FILENO,
			      "orlix-rootinit: root propagation setup failed\n");
		return 127;
	}
	if (apply_configured_sysctls() != 0) {
		write_literal(STDERR_FILENO,
			      "orlix-rootinit: sysctl setup failed\n");
		return 127;
	}
	if (apply_configured_runtime_paths(ORLIX_MASKED_PATH_KEY_PREFIX,
					   apply_masked_path) != 0) {
		write_literal(STDERR_FILENO,
			      "orlix-rootinit: masked path setup failed\n");
		return 127;
	}
	if (apply_configured_runtime_paths(ORLIX_READONLY_PATH_KEY_PREFIX,
					   apply_readonly_path) != 0) {
		write_literal(STDERR_FILENO,
			      "orlix-rootinit: readonly path setup failed\n");
		return 127;
	}
	if (switch_to_new_root() != 0) {
		write_literal(STDERR_FILENO,
			      "orlix-rootinit: switch_root failed\n");
		return 127;
	}

	write_literal(STDERR_FILENO, "ORLIX-ROOT-OVERLAY-READY\n");
	execve(argv[0], argv, envp);
	write_literal(STDERR_FILENO, "orlix-rootinit: exec /sbin/init failed\n");

	for (;;)
		pause();
}
