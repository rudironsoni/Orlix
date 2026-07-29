// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <sys/syscall.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "orlix_kselftest_user.h"

static char test_list[4096];
static char selected_test[128];

static bool cmdline_has_token(const char *token);

static void park_init(void)
{
	for (;;)
		(void)pause();
}

static int mount_procfs(void)
{
	if (mkdir("/proc", 0555) < 0 && errno != EEXIST)
		return -1;
	if (mount("proc", "/proc", "proc", 0, NULL) < 0 && errno != EBUSY)
		return -1;
	return 0;
}

static int mount_sysfs(void)
{
	if (mkdir("/sys", 0555) < 0 && errno != EEXIST)
		return -1;
	if (mount("sysfs", "/sys", "sysfs", 0, NULL) < 0 && errno != EBUSY)
		return -1;
	return 0;
}

static int mount_cgroup2(void)
{
	if (mkdir("/sys/fs", 0755) < 0 && errno != EEXIST)
		return -1;
	if (mkdir("/sys/fs/cgroup", 0755) < 0 && errno != EEXIST)
		return -1;
	if (mount("cgroup2", "/sys/fs/cgroup", "cgroup2", 0, NULL) < 0 &&
	    errno != EBUSY)
		return -1;
	return 0;
}

static int mount_devtmpfs(void)
{
	if (mkdir("/dev", 0755) < 0 && errno != EEXIST)
		return -1;
	if (mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) < 0 && errno != EBUSY)
		return -1;
	return 0;
}

static int mount_devpts(void)
{
	if (mkdir("/dev/pts", 0755) < 0 && errno != EEXIST)
		return -1;
	if (mount("devpts", "/dev/pts", "devpts", 0,
		  "gid=5,mode=620,ptmxmode=666") < 0 && errno != EBUSY)
		return -1;
	return 0;
}

static int install_fd_alias(const char *target, const char *linkpath)
{
	(void)unlink(linkpath);
	if (symlink(target, linkpath) == 0 || errno == EEXIST)
		return 0;
	return -1;
}

static int install_fd_aliases(void)
{
	if (install_fd_alias("/proc/self/fd", "/dev/fd") != 0)
		return -1;
	if (install_fd_alias("/proc/self/fd/0", "/dev/stdin") != 0)
		return -1;
	if (install_fd_alias("/proc/self/fd/1", "/dev/stdout") != 0)
		return -1;
	if (install_fd_alias("/proc/self/fd/2", "/dev/stderr") != 0)
		return -1;
	return 0;
}

static int mount_tmpfs_at(const char *target, const char *data)
{
	if (mkdir(target, 01777) < 0 && errno != EEXIST)
		return -1;
	if (mount("tmpfs", target, "tmpfs", 0, data) < 0 && errno != EBUSY)
		return -1;
	return 0;
}

static bool parse_orlix_test(const char *line, size_t len, const char **name,
			     size_t *name_len)
{
	static const char prefix[] = "orlix:";
	size_t i;

	for (i = 0; prefix[i]; i++) {
		if (i >= len || line[i] != prefix[i])
			return false;
	}

	*name = line + i;
	*name_len = len - i;
	return *name_len > 0;
}

static bool parse_arm64_mte_test(const char *line, size_t len, const char **name,
				 size_t *name_len)
{
	static const char prefix[] = "arm64-mte:";
	size_t i;

	for (i = 0; prefix[i]; i++)
		if (i >= len || line[i] != prefix[i])
			return false;
	*name = line + i;
	*name_len = len - i;
	return *name_len > 0;
}

static void read_selected_test(void)
{
	static const char prefix[] = "orlix.kselftest=";
	char cmdline[1024];
	size_t size = 0;
	size_t pos;

	selected_test[0] = '\0';
	if (orlix_read_file("/proc/cmdline", cmdline, sizeof(cmdline),
			    &size) != 0)
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
		       cmdline[pos] != '\n' && out + 1 < sizeof(selected_test))
			selected_test[out++] = cmdline[pos++];
		selected_test[out] = '\0';
		return;
	}
}

static bool selected_test_matches(const char *name, size_t name_len)
{
	if (!selected_test[0])
		return true;
	return orlix_strlen(selected_test) == name_len &&
	       orlix_memcmp(selected_test, name, name_len) == 0;
}

static bool default_test_is_runnable(const char *name, size_t name_len)
{
	static const char crossboot_verify[] =
		"environment_state_crossboot_verify_probe";
	static const char oci_prefix[] = "oci_";

	if (selected_test[0])
		return true;
	if (name_len >= sizeof(oci_prefix) - 1 &&
	    orlix_memcmp(name, oci_prefix, sizeof(oci_prefix) - 1) == 0)
		return false;
	return name_len != sizeof(crossboot_verify) - 1 ||
	       orlix_memcmp(name, crossboot_verify, name_len) != 0;
}

static bool test_name_equals(const char *name, size_t name_len,
			     const char *expected)
{
	return orlix_strlen(expected) == name_len &&
	       orlix_memcmp(name, expected, name_len) == 0;
}

static int enter_readonly_test_root(void)
{
	static const char root[] = "/orlix-readonly-root";

	if (mkdir(root, 0755) != 0 && errno != EEXIST)
		return -1;
	if (mount("/", root, NULL, MS_BIND, NULL) != 0)
		return -1;
	if (mount("/proc", "/orlix-readonly-root/proc", NULL, MS_BIND, NULL) != 0)
		return -1;
	if (mount(NULL, root, NULL, MS_BIND | MS_REMOUNT | MS_RDONLY,
		  NULL) != 0)
		return -1;
	if (chdir(root) != 0 || chroot(".") != 0 || chdir("/") != 0)
		return -1;

	return 0;
}

static void build_test_path(char *path, size_t capacity, const char *name,
			    size_t name_len)
{
	static const char prefix[] = "/orlix/";
	size_t pos = 0;
	size_t i;

	for (i = 0; prefix[i] && pos + 1 < capacity; i++)
		path[pos++] = prefix[i];
	for (i = 0; i < name_len && pos + 1 < capacity; i++)
		path[pos++] = name[i];
	path[pos] = '\0';
}

static int run_test(const char *name, size_t name_len)
{
	char path[160];
	pid_t child;
	int status;

	build_test_path(path, sizeof(path), name, name_len);
	orlix_test_comment("exec /orlix/", name, name_len);
	child = fork();
	if (child == 0) {
		char *const argv[] = { path, NULL };

		orlix_test_comment("child exec /orlix/", name, name_len);
		if (test_name_equals(name, name_len, "readonly_root_probe") &&
		    cmdline_has_token("orlix.root.readonly=1") &&
		    enter_readonly_test_root() != 0) {
			orlix_test_comment_uint("readonly root setup errno ", errno);
			_exit(127);
		}
		execv(path, argv);
		orlix_test_comment_uint("exec errno ", errno);
		_exit(127);
	}
	if (child < 0)
		return -1;
	if (waitpid(child, &status, 0) != child)
		return -1;
	if (!WIFEXITED(status)) {
		if (WIFSIGNALED(status))
			orlix_test_comment_uint("child signal ", WTERMSIG(status));
		return -1;
	}
	if (WEXITSTATUS(status) != 0)
		orlix_test_comment_uint("child exit status ", WEXITSTATUS(status));
	return WEXITSTATUS(status);
}

static int run_arm64_mte_test(const char *name, size_t name_len)
{
	char path[160] = "/arm64/mte/";
	size_t pos = sizeof("/arm64/mte/") - 1;
	pid_t child;
	int status;

	if (name_len + pos + 1 > sizeof(path))
		return -1;
	for (size_t i = 0; i < name_len; i++)
		path[pos + i] = name[i];
	path[pos + name_len] = '\0';
	orlix_test_comment("exec pristine upstream ", name, name_len);
	child = fork();
	if (child == 0) {
		char *const argv[] = { path, NULL };
		execv(path, argv);
		_exit(127);
	}
	if (child < 0 || waitpid(child, &status, 0) != child)
		return -1;
	if (!WIFEXITED(status))
		return -1;
	/* Linux kselftest's canonical skip status is accounted, never hidden. */
	if (WEXITSTATUS(status) == 4) {
		orlix_test_comment("upstream skipped ", name, name_len);
		return 4;
	}
	return WEXITSTATUS(status);
}

static unsigned int count_orlix_tests(const char *data, size_t size)
{
	unsigned int count = 0;
	size_t line_start = 0;
	size_t pos;

	for (pos = 0; pos <= size; pos++) {
		if (pos != size && data[pos] != '\n')
			continue;

		const char *name;
		size_t name_len;

		if ((parse_orlix_test(data + line_start, pos - line_start,
				     &name, &name_len) &&
		    selected_test_matches(name, name_len) &&
		    default_test_is_runnable(name, name_len)) ||
		    parse_arm64_mte_test(data + line_start, pos - line_start,
					 &name, &name_len))
			count++;
		line_start = pos + 1;
	}
	return count;
}

static void run_orlix_tests(const char *data, size_t size)
{
	size_t line_start = 0;
	size_t pos;

	for (pos = 0; pos <= size; pos++) {
		if (pos != size && data[pos] != '\n')
			continue;

		const char *name;
		size_t name_len;

		if (parse_orlix_test(data + line_start, pos - line_start,
				     &name, &name_len) &&
		    selected_test_matches(name, name_len) &&
		    default_test_is_runnable(name, name_len)) {
			int result = run_test(name, name_len);

			orlix_test_result(result == 0, name);
		}
		line_start = pos + 1;
	}
}

static void run_arm64_mte_tests(const char *data, size_t size)
{
	size_t line_start = 0;
	size_t pos;

	for (pos = 0; pos <= size; pos++) {
		const char *name;
		size_t name_len;
		int result;

		if (pos != size && data[pos] != '\n')
			continue;
		if (parse_arm64_mte_test(data + line_start, pos - line_start,
					 &name, &name_len)) {
			result = run_arm64_mte_test(name, name_len);
			if (result == 4)
				orlix_test_skip(name);
			else
				orlix_test_result(result == 0, name);
		}
		line_start = pos + 1;
	}
}

static bool cmdline_has_token(const char *token)
{
	char cmdline[1024];
	size_t token_len;
	size_t pos;
	size_t nread;

	if (orlix_read_file("/proc/cmdline", cmdline, sizeof(cmdline),
			    &nread) != 0)
		return false;
	if (nread >= sizeof(cmdline))
		nread = sizeof(cmdline) - 1;
	cmdline[nread] = '\0';

	token_len = orlix_strlen(token);
	for (pos = 0; pos < nread;) {
		while (pos < nread &&
		       (cmdline[pos] == ' ' || cmdline[pos] == '\n' ||
			cmdline[pos] == '\t'))
			pos++;
		if (pos + token_len <= nread &&
		    orlix_memcmp(cmdline + pos, token, token_len) == 0 &&
		    (pos + token_len == nread ||
		     cmdline[pos + token_len] == ' ' ||
		     cmdline[pos + token_len] == '\n' ||
		     cmdline[pos + token_len] == '\t'))
			return true;
		while (pos < nread && cmdline[pos] != ' ' &&
		       cmdline[pos] != '\n' && cmdline[pos] != '\t')
			pos++;
	}

	return false;
}

static bool cmdline_value(const char *key, char *value, size_t value_size)
{
	char cmdline[1024];
	size_t key_len;
	size_t nread;
	size_t pos;
	size_t out;

	if (value_size == 0)
		return false;
	value[0] = '\0';

	if (orlix_read_file("/proc/cmdline", cmdline, sizeof(cmdline),
			    &nread) != 0)
		return false;
	if (nread >= sizeof(cmdline))
		nread = sizeof(cmdline) - 1;
	cmdline[nread] = '\0';

	key_len = orlix_strlen(key);
	for (pos = 0; pos < nread;) {
		while (pos < nread &&
		       (cmdline[pos] == ' ' || cmdline[pos] == '\n' ||
			cmdline[pos] == '\t'))
			pos++;
		if (pos + key_len <= nread &&
		    orlix_memcmp(cmdline + pos, key, key_len) == 0) {
			pos += key_len;
			for (out = 0; pos < nread && out + 1 < value_size &&
			     cmdline[pos] != ' ' && cmdline[pos] != '\n' &&
			     cmdline[pos] != '\t'; pos++, out++)
				value[out] = cmdline[pos];
			value[out] = '\0';
			return out > 0;
		}
		while (pos < nread && cmdline[pos] != ' ' &&
		       cmdline[pos] != '\n' && cmdline[pos] != '\t')
			pos++;
	}

	return false;
}

static void apply_boot_identity_tokens(void)
{
	char value[128];

	if (cmdline_value("orlix.hostname=", value, sizeof(value)))
		(void)sethostname(value, orlix_strlen(value));
	if (cmdline_value("orlix.domainname=", value, sizeof(value)))
		(void)syscall(SYS_setdomainname, value, orlix_strlen(value));
}

int main(void)
{
	size_t list_size = 0;
	unsigned int test_count;
	bool have_list;
	int procfs_result;

	orlix_write_all("ORLIX-KSELFTEST-INIT\n");
	procfs_result = mount_procfs();
	read_selected_test();
	have_list = orlix_read_file("/kselftest-list.txt", test_list,
				    sizeof(test_list), &list_size) == 0;
	test_count = have_list ? count_orlix_tests(test_list, list_size) : 0;

	orlix_test_plan(test_count + 10);
	orlix_test_result(procfs_result == 0, "procfs mounted for kselftest");
	orlix_test_result(mount_sysfs() == 0, "sysfs mounted for kselftest");
	orlix_test_result(mount_cgroup2() == 0,
			  "cgroup2 mounted for kselftest");
	orlix_test_result(mount_devtmpfs() == 0, "devtmpfs mounted for kselftest");
	orlix_test_result(mount_devpts() == 0, "devpts mounted for kselftest");
	orlix_test_result(install_fd_aliases() == 0,
			  "standard fd aliases installed for kselftest");
	orlix_test_result(mount_tmpfs_at("/dev/shm", "mode=1777") == 0,
			  "dev shm tmpfs mounted for kselftest");
	orlix_test_result(mount_tmpfs_at("/run", "mode=0755") == 0,
			  "run tmpfs mounted for kselftest");
	orlix_test_result(mount_tmpfs_at("/tmp", "mode=1777") == 0,
			  "tmpfs mounted at /tmp for kselftest");
	orlix_test_result(have_list && test_count > 0,
			  "installed Orlix kselftest list is readable");
	apply_boot_identity_tokens();
	if (have_list)
		run_orlix_tests(test_list, list_size);
	if (have_list)
		run_arm64_mte_tests(test_list, list_size);
	orlix_write_all("ORLIX-KSELFTEST-END\n");
	park_init();
}
