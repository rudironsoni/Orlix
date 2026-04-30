#include <linux/fcntl.h>

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef S_IFMT
#define S_IFMT 0170000
#endif

#ifndef S_IFDIR
#define S_IFDIR 0040000
#endif

#ifndef S_IFREG
#define S_IFREG 0100000
#endif

#include "fs/fdtable.h"
#include "fs/vfs.h"
#include "kernel/init.h"
#include "kernel/task.h"

extern int open_impl(const char *pathname, int flags, linux_mode_t mode);
extern int close_impl(int fd);
extern long read_impl(int fd, void *buf, size_t count);
extern ssize_t getdents64(int fd, void *dirp, size_t count);
extern int readlink_impl(const char *pathname, char *buf, size_t bufsiz);
extern int fstat_impl(int fd, struct linux_stat *statbuf);

struct linux_dirent64 {
    uint64_t d_ino;
    int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

static int buffer_contains(const char *buf, size_t len, const char *needle) {
    size_t needle_len;
    size_t i;

    if (!buf || !needle) {
        return 0;
    }

    needle_len = strlen(needle);
    if (needle_len == 0 || needle_len > len) {
        return 0;
    }

    for (i = 0; i + needle_len <= len; i++) {
        if (memcmp(buf + i, needle, needle_len) == 0) {
            return 1;
        }
    }

    return 0;
}

static int dir_contains_name(int fd, const char *needle) {
    char buf[4096];
    ssize_t nread;
    size_t offset = 0;

    if (!needle) {
        errno = EINVAL;
        return -1;
    }

    nread = getdents64(fd, buf, sizeof(buf));
    if (nread < 0) {
        return -1;
    }

    while (offset < (size_t)nread) {
        struct linux_dirent64 *entry = (struct linux_dirent64 *)(buf + offset);
        if (strcmp(entry->d_name, needle) == 0) {
            return 1;
        }
        if (entry->d_reclen == 0) {
            break;
        }
        offset += entry->d_reclen;
    }

    return 0;
}

int kernel_init_contract_start_kernel_creates_current_init_task(void) {
    struct task_struct *task = get_current();

    if (!kernel_is_booted()) {
        errno = EPROTO;
        return -1;
    }
    if (!task || task != init_task) {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

int kernel_init_contract_init_task_identity_is_linux_shaped(void) {
    struct task_struct *task = get_current();

    if (!task) {
        errno = ESRCH;
        return -1;
    }
    if (task->pid != 1) {
        errno = EPROTO;
        return -1;
    }
    if (task->tgid != task->pid || task->ppid != 0) {
        errno = EPROTO;
        return -1;
    }
    if (task->pgid != task->pid || task->sid != task->pid) {
        errno = EPROTO;
        return -1;
    }
    if (task->state != TASK_RUNNING) {
        errno = EPROTO;
        return -1;
    }
    if (strcmp(task->comm, "init") != 0) {
        errno = EPROTO;
        return -1;
    }
    if (task->exe[0] != '\0') {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

int kernel_init_contract_init_task_cwd_and_root_are_slash(void) {
    struct task_struct *task = get_current();

    if (!task || !task->fs) {
        errno = ESRCH;
        return -1;
    }
    if (strcmp(task->fs->pwd_path, "/") != 0) {
        errno = EPROTO;
        return -1;
    }
    if (strcmp(task->fs->root_path, "/") != 0) {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

int kernel_init_contract_kernel_boot_exposes_root(void) {
    struct linux_stat st;

    if (vfs_fstatat(AT_FDCWD, "/", &st, 0) != 0) {
        errno = EPROTO;
        return -1;
    }
    if ((st.st_mode & S_IFMT) != S_IFDIR) {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

int kernel_init_contract_kernel_boot_exposes_etc_passwd(void) {
    struct linux_stat st;

    if (vfs_fstatat(AT_FDCWD, "/etc", &st, 0) != 0) {
        return -1;
    }
    if (vfs_fstatat(AT_FDCWD, "/etc/passwd", &st, 0) != 0) {
        return -1;
    }
    if ((st.st_mode & S_IFMT) != S_IFREG) {
        errno = EPROTO;
        return -1;
    }
    return 0;
}

int kernel_init_contract_kernel_boot_exposes_dev_root(void) {
    struct linux_stat st;

    if (!vfs_path_is_synthetic("/dev")) {
        errno = EPROTO;
        return -1;
    }
    if (vfs_fstatat(AT_FDCWD, "/dev", &st, 0) != 0) {
        return -1;
    }
    return 0;
}

int kernel_init_contract_kernel_boot_exposes_proc_root(void) {
    struct linux_stat st;

    if (!vfs_path_is_synthetic("/proc")) {
        errno = EPROTO;
        return -1;
    }
    if (vfs_fstatat(AT_FDCWD, "/proc", &st, 0) != 0) {
        return -1;
    }
    if (vfs_fstatat(AT_FDCWD, "/proc/self", &st, 0) != 0) {
        return -1;
    }
    return 0;
}

int kernel_init_contract_kernel_boot_exposes_sys_root_or_documents_policy(void) {
    struct linux_stat st;

    if (!vfs_path_is_synthetic("/sys")) {
        errno = EPROTO;
        return -1;
    }
    if (vfs_fstatat(AT_FDCWD, "/sys", &st, 0) != 0) {
        return -1;
    }
    return 0;
}

int kernel_init_contract_kernel_boot_exposes_tmp_and_var_cache_routes(void) {
    struct linux_stat st;

    if (vfs_backing_class_for_path("/tmp") != VFS_BACKING_TEMP) {
        errno = EPROTO;
        return -1;
    }
    if (vfs_backing_class_for_path("/var/cache") != VFS_BACKING_CACHE) {
        errno = EPROTO;
        return -1;
    }
    if (vfs_fstatat(AT_FDCWD, "/tmp", &st, 0) != 0) {
        return -1;
    }
    if (vfs_fstatat(AT_FDCWD, "/var/cache", &st, 0) != 0) {
        return -1;
    }
    return 0;
}

int kernel_init_contract_kernel_boot_stdio_policy_is_explicit(void) {
    char buf[64];
    int link_len;
    struct linux_stat st;

    if (!fdtable_is_used_impl(0) || !fdtable_is_used_impl(1) || !fdtable_is_used_impl(2)) {
        errno = EPROTO;
        return -1;
    }
    link_len = readlink_impl("/proc/self/fd/0", buf, sizeof(buf) - 1);
    if (link_len < 0) {
        return -1;
    }
    buf[link_len] = '\0';
    if (strcmp(buf, "/dev/null") != 0) {
        errno = EPROTO;
        return -1;
    }
    link_len = readlink_impl("/proc/self/fd/1", buf, sizeof(buf) - 1);
    if (link_len < 0) {
        return -1;
    }
    buf[link_len] = '\0';
    if (strcmp(buf, "/dev/null") != 0) {
        errno = EPROTO;
        return -1;
    }
    link_len = readlink_impl("/proc/self/fd/2", buf, sizeof(buf) - 1);
    if (link_len < 0) {
        return -1;
    }
    buf[link_len] = '\0';
    if (strcmp(buf, "/dev/null") != 0) {
        errno = EPROTO;
        return -1;
    }
    if (fstat_impl(0, &st) != 0 || fstat_impl(1, &st) != 0 || fstat_impl(2, &st) != 0) {
        return -1;
    }
    return 0;
}

int kernel_init_contract_proc_self_reflects_current_task(void) {
    char buf[512];
    ssize_t nread;
    int fd;

    nread = readlink_impl("/proc/self/cwd", buf, sizeof(buf) - 1);
    if (nread < 0) {
        return -1;
    }
    buf[nread] = '\0';
    if (strcmp(buf, "/") != 0) {
        errno = EPROTO;
        return -1;
    }

    fd = open_impl("/proc/self/comm", O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }
    nread = read_impl(fd, buf, sizeof(buf) - 1);
    close_impl(fd);
    if (nread <= 0) {
        errno = EPROTO;
        return -1;
    }
    buf[nread] = '\0';
    if (strcmp(buf, "init\n") != 0) {
        errno = EPROTO;
        return -1;
    }

    fd = open_impl("/proc/self/stat", O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }
    nread = read_impl(fd, buf, sizeof(buf) - 1);
    close_impl(fd);
    if (nread <= 0) {
        errno = EPROTO;
        return -1;
    }
    buf[nread] = '\0';
    if (!buffer_contains(buf, (size_t)nread, "1 (init) R 0 1 1 ")) {
        errno = EPROTO;
        return -1;
    }

    return 0;
}

int kernel_init_contract_proc_self_fd_reflects_boot_descriptors(void) {
    int fd;
    int found0;
    int found1;
    int found2;

    fd = open_impl("/proc/self/fd", O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        return -1;
    }

    found0 = dir_contains_name(fd, "0");
    close_impl(fd);
    if (found0 != 1) {
        errno = EPROTO;
        return -1;
    }

    fd = open_impl("/proc/self/fd", O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        return -1;
    }
    found1 = dir_contains_name(fd, "1");
    close_impl(fd);
    if (found1 != 1) {
        errno = EPROTO;
        return -1;
    }

    fd = open_impl("/proc/self/fd", O_RDONLY | O_DIRECTORY, 0);
    if (fd < 0) {
        return -1;
    }
    found2 = dir_contains_name(fd, "2");
    close_impl(fd);
    if (found2 != 1) {
        errno = EPROTO;
        return -1;
    }

    return 0;
}

int kernel_init_contract_proc_self_fdinfo_reflects_boot_descriptors(void) {
    char buf[256];
    ssize_t nread;
    int fd;

    fd = open_impl("/proc/self/fdinfo/0", O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }
    nread = read_impl(fd, buf, sizeof(buf) - 1);
    close_impl(fd);
    if (nread <= 0) {
        errno = EPROTO;
        return -1;
    }
    buf[nread] = '\0';
    if (!buffer_contains(buf, (size_t)nread, "pos:\t0\n") || !buffer_contains(buf, (size_t)nread, "flags:\t00")) {
        errno = EPROTO;
        return -1;
    }

    fd = open_impl("/proc/self/fdinfo/1", O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }
    nread = read_impl(fd, buf, sizeof(buf) - 1);
    close_impl(fd);
    if (nread <= 0) {
        errno = EPROTO;
        return -1;
    }
    buf[nread] = '\0';
    if (!buffer_contains(buf, (size_t)nread, "flags:\t01")) {
        errno = EPROTO;
        return -1;
    }

    return 0;
}

int kernel_init_contract_proc_self_exe_policy_before_exec_is_explicit(void) {
    char buf[64];
    int ret = readlink_impl("/proc/self/exe", buf, sizeof(buf));

    if (ret >= 0) {
        errno = EPROTO;
        return -1;
    }
    if (errno != ENOENT) {
        return -1;
    }
    return 0;
}

int kernel_init_contract_kernel_shutdown_and_reboot_restores_init_state(void) {
    if (kernel_shutdown() != 0) {
        return -1;
    }
    if (start_kernel() != 0) {
        return -1;
    }
    if (kernel_init_contract_start_kernel_creates_current_init_task() != 0) {
        return -1;
    }
    if (kernel_init_contract_init_task_identity_is_linux_shaped() != 0) {
        return -1;
    }
    if (kernel_init_contract_kernel_boot_stdio_policy_is_explicit() != 0) {
        return -1;
    }
    return 0;
}
