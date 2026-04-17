#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "vfs.h"
#include "../kernel/task.h"

static int directory_validate_path(const char *path) {
    if (path == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (path[0] == '\0') {
        errno = ENOENT;
        return -1;
    }

    return 0;
}

/* Get current task - forward declaration */
struct task_struct *get_current(void);

static int directory_translate_task_path(const char *path, char *translated_path,
                                         size_t translated_path_len,
                                         struct task_struct **task_out) {
    struct task_struct *task;
    int ret;

    task = get_current();
    if (!task) {
        errno = ESRCH;
        return -1;
    }

    ret = vfs_translate_path_task(path, translated_path, translated_path_len, task->fs);
    if (ret != 0) {
        errno = -ret;
        return -1;
    }

    if (task_out) {
        *task_out = task;
    }

    return 0;
}

int chdir_impl(const char *path) {
    struct task_struct *task;
    char translated_path[MAX_PATH];
    char resolved_virtual[MAX_PATH];

    if (directory_validate_path(path) != 0) {
        return -1;
    }

    if (directory_translate_task_path(path, translated_path, sizeof(translated_path), &task) != 0) {
        return -1;
    }

    struct stat st;
    if (stat(translated_path, &st) != 0) {
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }

    if (access(translated_path, X_OK) != 0) {
        errno = EACCES;
        return -1;
    }

    if (task->fs) {
        int ret = vfs_resolve_virtual_path_task(path, resolved_virtual, sizeof(resolved_virtual), task->fs);
        if (ret != 0) {
            errno = -ret;
            return -1;
        }
        fs_set_pwd(task->fs, resolved_virtual);
    }

    return 0;
}

int fchdir_impl(int fd) {
    if (fd < 0 || fd >= NR_OPEN_DEFAULT || fd <= STDERR_FILENO) {
        errno = EBADF;
        return -1;
    }

    return fchdir(fd);
}

char *getcwd_impl(char *buf, size_t size) {
    struct task_struct *task;
    char virtual_path[MAX_PATH];
    int ret;

    if (size == 0) {
        errno = EINVAL;
        return NULL;
    }

    if (buf == NULL) {
        errno = EINVAL;
        return NULL;
    }

    task = get_current();
    if (!task) {
        errno = ESRCH;
        return NULL;
    }

    ret = vfs_getcwd_path_task(task->fs, virtual_path, sizeof(virtual_path));
    if (ret != 0) {
        errno = -ret;
        return NULL;
    }

    const size_t selected_len = strlen(virtual_path);
    if (selected_len >= size) {
        errno = ERANGE;
        return NULL;
    }

    memcpy(buf, virtual_path, selected_len + 1);
    return buf;
}

int mkdir_impl(const char *pathname, mode_t mode) {
    char translated_path[MAX_PATH];

    if (directory_validate_path(pathname) != 0) {
        return -1;
    }

    if (directory_translate_task_path(pathname, translated_path, sizeof(translated_path), NULL) != 0) {
        return -1;
    }

    return mkdir(translated_path, mode);
}

int rmdir_impl(const char *pathname) {
    char translated_path[MAX_PATH];

    if (directory_validate_path(pathname) != 0) {
        return -1;
    }

    if (directory_translate_task_path(pathname, translated_path, sizeof(translated_path), NULL) != 0) {
        return -1;
    }

    struct stat st;
    if (stat(translated_path, &st) != 0) {
        return -1;
    }

    if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        return -1;
    }

    return rmdir(translated_path);
}

int unlink_impl(const char *pathname) {
    char translated_path[MAX_PATH];

    if (directory_validate_path(pathname) != 0) {
        return -1;
    }

    if (directory_translate_task_path(pathname, translated_path, sizeof(translated_path), NULL) != 0) {
        return -1;
    }

    struct stat st;
    if (stat(translated_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        errno = EISDIR;
        return -1;
    }

    return unlink(translated_path);
}

int link_impl(const char *oldpath, const char *newpath) {
    char translated_old[MAX_PATH];
    char translated_new[MAX_PATH];

    if (oldpath == NULL || newpath == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (oldpath[0] == '\0' || newpath[0] == '\0') {
        errno = ENOENT;
        return -1;
    }

    if (directory_translate_task_path(oldpath, translated_old, sizeof(translated_old), NULL) != 0) {
        return -1;
    }

    if (directory_translate_task_path(newpath, translated_new, sizeof(translated_new), NULL) != 0) {
        return -1;
    }

    struct stat st;
    if (stat(translated_old, &st) != 0) {
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        errno = EPERM;
        return -1;
    }

    if (stat(translated_new, &st) == 0) {
        errno = EEXIST;
        return -1;
    }

    return link(translated_old, translated_new);
}

int symlink_impl(const char *target, const char *linkpath) {
    char translated_link[MAX_PATH];

    if (target == NULL || linkpath == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (linkpath[0] == '\0') {
        errno = ENOENT;
        return -1;
    }

    if (directory_translate_task_path(linkpath, translated_link, sizeof(translated_link), NULL) != 0) {
        return -1;
    }

    struct stat target_stat;
    if (stat(translated_link, &target_stat) == 0) {
        errno = EEXIST;
        return -1;
    }

    return symlink(target, translated_link);
}

ssize_t readlink_impl(const char *pathname, char *buf, size_t bufsiz) {
    char translated_path[MAX_PATH];

    if (pathname == NULL || buf == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (pathname[0] == '\0') {
        errno = ENOENT;
        return -1;
    }

    if (bufsiz == 0) {
        errno = EINVAL;
        return -1;
    }

    if (directory_translate_task_path(pathname, translated_path, sizeof(translated_path), NULL) != 0) {
        return -1;
    }

    struct stat path_stat;
    if (lstat(translated_path, &path_stat) != 0) {
        return -1;
    }

    if (!S_ISLNK(path_stat.st_mode)) {
        errno = EINVAL;
        return -1;
    }

    return readlink(translated_path, buf, bufsiz);
}

int chroot_impl(const char *path) {
  (void)path;
  errno = EPERM;
  return -1;
}

__attribute__((visibility("default"))) int chdir(const char *path) {
  return chdir_impl(path);
}

__attribute__((visibility("default"))) int fchdir(int fd) {
  return fchdir_impl(fd);
}

__attribute__((visibility("default"))) char *getcwd(char *buf, size_t size) {
  return getcwd_impl(buf, size);
}

__attribute__((visibility("default"))) int mkdir(const char *pathname, mode_t mode) {
  return mkdir_impl(pathname, mode);
}

__attribute__((visibility("default"))) int mkdirat(int dirfd, const char *pathname, mode_t mode) {
  char translated_path[MAX_PATH];
  int ret;

  if (pathname == NULL) {
    errno = EFAULT;
    return -1;
  }

  ret = vfs_translate_path_at(dirfd, pathname, translated_path, sizeof(translated_path));
  if (ret != 0) {
    errno = -ret;
    return -1;
  }

  return mkdir(translated_path, mode);
}

__attribute__((visibility("default"))) int rmdir(const char *pathname) {
  return rmdir_impl(pathname);
}

__attribute__((visibility("default"))) int unlink(const char *pathname) {
  return unlink_impl(pathname);
}

__attribute__((visibility("default"))) int unlinkat(int dirfd, const char *pathname, int flags) {
  char translated_path[MAX_PATH];
  int ret;

  if (pathname == NULL) {
    errno = EFAULT;
    return -1;
  }

  ret = vfs_translate_path_at(dirfd, pathname, translated_path, sizeof(translated_path));
  if (ret != 0) {
    errno = -ret;
    return -1;
  }

  if ((flags & AT_REMOVEDIR) != 0) {
    return rmdir(translated_path);
  }
  return unlink(translated_path);
}

__attribute__((visibility("default"))) int link(const char *oldpath, const char *newpath) {
  return link_impl(oldpath, newpath);
}

__attribute__((visibility("default"))) int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags) {
  char translated_old[MAX_PATH];
  char translated_new[MAX_PATH];
  int ret;

  if (oldpath == NULL || newpath == NULL) {
    errno = EFAULT;
    return -1;
  }

  if (flags & ~AT_SYMLINK_FOLLOW) {
    errno = EINVAL;
    return -1;
  }

  if (flags & AT_SYMLINK_FOLLOW) {
    /* Following symlinks is the default behavior; no special handling needed */
  }

  ret = vfs_translate_path_at(olddirfd, oldpath, translated_old, sizeof(translated_old));
  if (ret != 0) {
    errno = -ret;
    return -1;
  }

  ret = vfs_translate_path_at(newdirfd, newpath, translated_new, sizeof(translated_new));
  if (ret != 0) {
    errno = -ret;
    return -1;
  }

  return link(translated_old, translated_new);
}

__attribute__((visibility("default"))) int symlink(const char *target, const char *linkpath) {
    return symlink_impl(target, linkpath);
}

__attribute__((visibility("default"))) int symlinkat(const char *target, int newdirfd, const char *linkpath) {
    char translated_link[MAX_PATH];
    int ret;

    if (target == NULL || linkpath == NULL) {
        errno = EFAULT;
        return -1;
    }

    ret = vfs_translate_path_at(newdirfd, linkpath, translated_link, sizeof(translated_link));
    if (ret != 0) {
        errno = -ret;
        return -1;
    }

    return symlink(target, translated_link);
}

__attribute__((visibility("default"))) ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) {
    return readlink_impl(pathname, buf, bufsiz);
}

__attribute__((visibility("default"))) ssize_t readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz) {
    char translated_path[MAX_PATH];
    int ret;

    if (pathname == NULL || buf == NULL) {
        errno = EFAULT;
        return -1;
    }

    ret = vfs_translate_path_at(dirfd, pathname, translated_path, sizeof(translated_path));
    if (ret != 0) {
        errno = -ret;
        return -1;
    }

    return readlink(translated_path, buf, bufsiz);
}

__attribute__((visibility("default"))) int chroot(const char *path) {
    return chroot_impl(path);
}
