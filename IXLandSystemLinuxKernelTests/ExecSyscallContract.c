#include <linux/fcntl.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include <string.h>

#include "fs/fdtable.h"
#include "fs/vfs.h"
#include "kernel/task.h"
#include "runtime/native/registry.h"

extern int execve(const char *pathname, char *const argv[], char *const envp[]);
extern int open_impl(const char *pathname, int flags, linux_mode_t mode);
extern long readlink_impl(const char *pathname, char *buf, size_t bufsiz);

static int close_if_open(int fd) {
    if (fd >= 0 && fdtable_is_used_impl(fd)) {
        return close_impl(fd);
    }
    return 0;
}

static int expect_errno(int expected) {
    if (errno != expected) {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

static int native_exec_status(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    return 23;
}

static int verify_state_unchanged(struct task_struct *task,
                                  const char *expected_exe,
                                  const char *expected_comm,
                                  bool expected_execed,
                                  int cloexec_fd,
                                  int keep_fd) {
    if (strcmp(task->exe, expected_exe) != 0) {
        errno = EPROTO;
        return -1;
    }
    if (strcmp(task->comm, expected_comm) != 0) {
        errno = EPROTO;
        return -1;
    }
    if (atomic_load(&task->execed) != expected_execed) {
        errno = EPROTO;
        return -1;
    }
    if ((cloexec_fd >= 0 && !fdtable_is_used_impl(cloexec_fd)) ||
        (keep_fd >= 0 && !fdtable_is_used_impl(keep_fd))) {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

int exec_syscall_contract_rejects_null_path_without_transition(void) {
    struct task_struct *task = get_current();

    if (!task) {
        errno = ESRCH;
        return -1;
    }

    memcpy(task->exe, "/before", 8);
    memset(task->comm, 0, sizeof(task->comm));
    memcpy(task->comm, "before", 7);
    atomic_store(&task->execed, false);

    errno = 0;
    if (execve(NULL, NULL, NULL) != -1) {
        errno = EPROTO;
        return -1;
    }
    if (expect_errno(EFAULT) != 0) {
        return -1;
    }
    return verify_state_unchanged(task, "/before", "before", false, -1, -1);
}

int exec_syscall_contract_rejects_empty_path_without_transition(void) {
    struct task_struct *task = get_current();

    if (!task) {
        errno = ESRCH;
        return -1;
    }

    memcpy(task->exe, "/before", 8);
    memset(task->comm, 0, sizeof(task->comm));
    memcpy(task->comm, "before", 7);
    atomic_store(&task->execed, false);

    errno = 0;
    if (execve("", NULL, NULL) != -1) {
        errno = EPROTO;
        return -1;
    }
    if (expect_errno(ENOENT) != 0) {
        return -1;
    }
    return verify_state_unchanged(task, "/before", "before", false, -1, -1);
}

int exec_syscall_contract_missing_path_preserves_state_and_cloexec_fds(void) {
    struct task_struct *task = get_current();
    int cloexec_fd = -1;
    int keep_fd = -1;
    int result = -1;

    if (!task) {
        errno = ESRCH;
        return -1;
    }

    memcpy(task->exe, "/before", 8);
    memset(task->comm, 0, sizeof(task->comm));
    memcpy(task->comm, "before", 7);
    atomic_store(&task->execed, false);

    cloexec_fd = open_impl("/dev/null", O_RDONLY | O_CLOEXEC, 0);
    if (cloexec_fd < 0) {
        return -1;
    }

    keep_fd = open_impl("/dev/zero", O_RDONLY, 0);
    if (keep_fd < 0) {
        goto out;
    }

    errno = 0;
    if (execve("/missing", NULL, NULL) != -1) {
        errno = EPROTO;
        goto out;
    }
    if (expect_errno(ENOENT) != 0) {
        goto out;
    }
    if (verify_state_unchanged(task, "/before", "before", false, cloexec_fd, keep_fd) != 0) {
        goto out;
    }

    result = 0;

out:
    close_if_open(keep_fd);
    close_if_open(cloexec_fd);
    return result;
}

int exec_syscall_contract_native_success_applies_transition_and_returns_entry_status(void) {
    struct task_struct *task = get_current();
    char *argv[] = {"custom-shell", "arg1", NULL};
    char *envp[] = {"A=B", NULL};
    int cloexec_fd = -1;
    int keep_fd = -1;
    char link_target[MAX_PATH];
    long link_len;
    int status;
    int result = -1;

    if (!task) {
        errno = ESRCH;
        return -1;
    }

    native_registry_clear();
    if (native_register("//usr//bin///env/", native_exec_status) != 0) {
        return -1;
    }

    memcpy(task->exe, "/before", 8);
    memset(task->comm, 0, sizeof(task->comm));
    memcpy(task->comm, "before", 7);
    atomic_store(&task->execed, false);

    cloexec_fd = open_impl("/dev/null", O_RDONLY | O_CLOEXEC, 0);
    if (cloexec_fd < 0) {
        goto out;
    }

    keep_fd = open_impl("/dev/zero", O_RDONLY, 0);
    if (keep_fd < 0) {
        goto out;
    }

    status = execve("//usr//bin///env/", argv, envp);
    if (status != 23) {
        errno = EPROTO;
        goto out;
    }
    if (!atomic_load(&task->execed)) {
        errno = EPROTO;
        goto out;
    }
    if (strcmp(task->exe, "/usr/bin/env") != 0) {
        errno = EPROTO;
        goto out;
    }
    if (strcmp(task->comm, "custom-shell") != 0) {
        errno = EPROTO;
        goto out;
    }
    if (fdtable_is_used_impl(cloexec_fd) || !fdtable_is_used_impl(keep_fd)) {
        errno = EPROTO;
        goto out;
    }
    if (!task->exec_image) {
        errno = EPROTO;
        goto out;
    }
    if (strcmp(task->exec_image->path, "//usr//bin///env/") != 0) {
        errno = EPROTO;
        goto out;
    }

    link_len = readlink_impl("/proc/self/exe", link_target, sizeof(link_target));
    if (link_len < 0) {
        goto out;
    }
    if ((size_t)link_len >= sizeof(link_target)) {
        errno = EPROTO;
        goto out;
    }
    link_target[link_len] = '\0';
    if (strcmp(link_target, "/usr/bin/env") != 0) {
        errno = EPROTO;
        goto out;
    }

    result = 0;

out:
    native_registry_clear();
    close_if_open(keep_fd);
    close_if_open(cloexec_fd);
    return result;
}
