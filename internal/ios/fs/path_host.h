/* internal/ios/fs/path_host.h
 * Narrow seam for host path operations
 */

#ifndef PATH_HOST_H
#define PATH_HOST_H

#include <sys/stat.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Host stat operations */
int host_stat_impl(const char *path, struct stat *statbuf);
int host_lstat_impl(const char *path, struct stat *statbuf);
int host_access_impl(const char *path, int mode);

/* Host rename operation (Darwin renameatx_np) */
int host_renameatx_np_impl(int fromfd, const char *from, int tofd, const char *to, unsigned int flags);

/* Directory operations */
int host_mkdir_impl(const char *pathname, mode_t mode);
int host_rmdir_impl(const char *pathname);

/* File operations */
int host_unlink_impl(const char *pathname);
int host_link_impl(const char *oldpath, const char *newpath);
int host_symlink_impl(const char *target, const char *linkpath);
ssize_t host_readlink_impl(const char *pathname, char *buf, size_t bufsiz);

/* Fchdir */
int host_fchdir_impl(int fd);

#ifdef __cplusplus
}
#endif

#endif /* PATH_HOST_H */
