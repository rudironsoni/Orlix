#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "vfs.h"

static int ixland_directory_validate_path(const char *path) {
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

int chdir_impl(const char *path) {
    if (ixland_directory_validate_path(path) != 0) {
        return -1;
    }

    char translated_path[MAX_PATH];
    if (vfs_translate_path(path, translated_path, sizeof(translated_path)) != 0) {
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

    if (chdir(translated_path) != 0) {
        return -1;
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
    if (size == 0) {
        errno = EINVAL;
        return NULL;
    }

    if (buf == NULL) {
        errno = EINVAL;
        return NULL;
    }

    char ios_cwd[MAX_PATH];
    if (getcwd(ios_cwd, sizeof(ios_cwd)) == NULL) {
        return NULL;
    }

    char virtual_path[MAX_PATH];
    const char *selected_path = ios_cwd;

    if (vfs_reverse_translate(ios_cwd, virtual_path, sizeof(virtual_path)) == 0) {
        selected_path = virtual_path;
    }

    const size_t selected_len = strlen(selected_path);
    if (selected_len >= size) {
        errno = ERANGE;
        return NULL;
    }

    memcpy(buf, selected_path, selected_len + 1);
    return buf;
}

int mkdir_impl(const char *pathname, mode_t mode) {
    if (ixland_directory_validate_path(pathname) != 0) {
        return -1;
    }

    char translated_path[MAX_PATH];
    if (vfs_translate_path(pathname, translated_path, sizeof(translated_path)) != 0) {
        return -1;
    }

    return mkdir(translated_path, mode);
}

int rmdir_impl(const char *pathname) {
    if (ixland_directory_validate_path(pathname) != 0) {
        return -1;
    }

    char translated_path[MAX_PATH];
    if (vfs_translate_path(pathname, translated_path, sizeof(translated_path)) != 0) {
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
    if (ixland_directory_validate_path(pathname) != 0) {
        return -1;
    }

    char translated_path[MAX_PATH];
    if (vfs_translate_path(pathname, translated_path, sizeof(translated_path)) != 0) {
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
    if (oldpath == NULL || newpath == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (oldpath[0] == '\0' || newpath[0] == '\0') {
        errno = ENOENT;
        return -1;
    }

    char translated_old[MAX_PATH];
    char translated_new[MAX_PATH];
    if (vfs_translate_path(oldpath, translated_old, sizeof(translated_old)) != 0) {
        return -1;
    }

    if (vfs_translate_path(newpath, translated_new, sizeof(translated_new)) != 0) {
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
    if (target == NULL || linkpath == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (linkpath[0] == '\0') {
        errno = ENOENT;
        return -1;
    }

    char translated_link[MAX_PATH];
    if (vfs_translate_path(linkpath, translated_link, sizeof(translated_link)) != 0) {
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

    char translated_path[MAX_PATH];
    if (vfs_translate_path(pathname, translated_path, sizeof(translated_path)) != 0) {
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
  if (pathname == NULL) {
    errno = EFAULT;
    return -1;
  }

  if (dirfd == AT_FDCWD) {
    return mkdir(pathname, mode);
  }

  (void)dirfd;
  (void)mode;
  errno = ENOSYS;
  return -1;
}

__attribute__((visibility("default"))) int rmdir(const char *pathname) {
  return rmdir_impl(pathname);
}

__attribute__((visibility("default"))) int unlink(const char *pathname) {
  return unlink_impl(pathname);
}

__attribute__((visibility("default"))) int unlinkat(int dirfd, const char *pathname, int flags) {
  if (pathname == NULL) {
    errno = EFAULT;
    return -1;
  }

  if (dirfd == AT_FDCWD) {
    if ((flags & AT_REMOVEDIR) != 0) {
      return rmdir(pathname);
    }
    return unlink(pathname);
  }

  (void)dirfd;
  (void)flags;
  errno = ENOSYS;
  return -1;
}

__attribute__((visibility("default"))) int link(const char *oldpath, const char *newpath) {
  return link_impl(oldpath, newpath);
}

__attribute__((visibility("default"))) int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags) {
  if (oldpath == NULL || newpath == NULL) {
    errno = EFAULT;
    return -1;
  }

  if (olddirfd == AT_FDCWD && newdirfd == AT_FDCWD) {
    return link(oldpath, newpath);
  }

  (void)olddirfd;
  (void)newdirfd;
  (void)flags;
  errno = ENOSYS;
  return -1;
}

int ixland_symlink(const char *target, const char *linkpath) {
    return symlink_impl(target, linkpath);
}

int ixland_symlinkat(const char *target, int newdirfd, const char *linkpath) {
    if (target == NULL || linkpath == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (newdirfd == AT_FDCWD) {
        return ixland_symlink(target, linkpath);
    }

    (void)newdirfd;
    errno = ENOSYS;
    return -1;
}

ssize_t ixland_readlink(const char *pathname, char *buf, size_t bufsiz) {
    return readlink_impl(pathname, buf, bufsiz);
}

ssize_t ixland_readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz) {
    if (pathname == NULL || buf == NULL) {
        errno = EFAULT;
        return -1;
    }

    if (dirfd == AT_FDCWD) {
        return ixland_readlink(pathname, buf, bufsiz);
    }

    (void)dirfd;
    (void)bufsiz;
    errno = ENOSYS;
    return -1;
}

int ixland_chroot(const char *path) {
    return chroot_impl(path);
}
