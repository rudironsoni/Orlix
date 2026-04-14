/*
 * IXLandSystem Public Syscall Interface
 *
 * Public package seam between IXLandSystem and IXLandLibC.
 * These are the ONLY functions IXLandLibC SHALL call from IXLandSystem.
 * Canonical Linux-shaped names for syscalls.
 * These functions delegate to internal IXLandSystem owners.
 */

#ifndef IXLAND_SYSCALLS_H
#define IXLAND_SYSCALLS_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * FILE I/O - Open/Close
 * ============================================================================ */

int open(const char *pathname, int flags, ...);
int openat(int dirfd, const char *pathname, int flags, ...);
int creat(const char *pathname, mode_t mode);
int close(int fd);

/* ============================================================================
 * FILE I/O - Read/Write
 * ============================================================================ */

ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);

/* ============================================================================
 * FILE I/O - Seeking
 * ============================================================================ */

off_t lseek(int fd, off_t offset, int whence);

/* ============================================================================
 * FILE I/O - Pread/Pwrite
 * ============================================================================ */

ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);

/* ============================================================================
 * FILE I/O - Duplication
 * ============================================================================ */

int dup(int oldfd);
int dup2(int oldfd, int newfd);
int dup3(int oldfd, int newfd, int flags);

/* ============================================================================
 * FILE I/O - File Control
 * ============================================================================ */

int fcntl(int fd, int cmd, ...);
int ioctl(int fd, unsigned long request, ...);

/* ============================================================================
 * PATH / CWD
 * ============================================================================ */

int chdir(const char *path);
int fchdir(int fd);
char *getcwd(char *buf, size_t size);

/* ============================================================================
 * DIRECTORY CREATION / REMOVAL
 * ============================================================================ */

int mkdir(const char *pathname, mode_t mode);
int mkdirat(int dirfd, const char *pathname, mode_t mode);
int rmdir(const char *pathname);

/* ============================================================================
 * FILE DELETION
 * ============================================================================ */

int unlink(const char *pathname);
int unlinkat(int dirfd, const char *pathname, int flags);

/* ============================================================================
 * HARD LINKS
 * ============================================================================ */

int link(const char *oldpath, const char *newpath);
int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags);

/* ============================================================================
 * SYMBOLIC LINKS
 * ============================================================================ */

int symlink(const char *target, const char *linkpath);
int symlinkat(const char *target, int newdirfd, const char *linkpath);
ssize_t readlink(const char *pathname, char *buf, size_t bufsiz);
ssize_t readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz);

/* ============================================================================
 * CHROOT
 * ============================================================================ */

int chroot(const char *path);

/* ============================================================================
 * FILE METADATA - Stat
 * ============================================================================ */

int stat(const char *pathname, struct stat *statbuf);
int fstat(int fd, struct stat *statbuf);
int lstat(const char *pathname, struct stat *statbuf);

/* ============================================================================
 * FILE PERMISSIONS - Access
 * ============================================================================ */

int access(const char *pathname, int mode);
int faccessat(int dirfd, const char *pathname, int mode, int flags);

/* ============================================================================
 * FILE PERMISSIONS - Chmod
 * ============================================================================ */

int chmod(const char *pathname, mode_t mode);
int fchmod(int fd, mode_t mode);
int fchmodat(int dirfd, const char *pathname, mode_t mode, int flags);

/* ============================================================================
 * FILE OWNERSHIP - Chown
 * ============================================================================ */

int chown(const char *pathname, uid_t owner, gid_t group);
int fchown(int fd, uid_t owner, gid_t group);
int lchown(const char *pathname, uid_t owner, gid_t group);
int fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags);

/* ============================================================================
 * FILE CREATION MASK - Umask
 * ============================================================================ */

mode_t umask(mode_t mask);

/* ============================================================================
 * STAT/FSSTAT
 * ============================================================================ */

int statfs(const char *path, struct statfs *buf);
int fstatfs(int fd, struct statfs *buf);
int statvfs(const char *path, struct statvfs *buf);
int fstatvfs(int fd, struct statvfs *buf);

/* ============================================================================
 * MOUNT OPERATIONS (restricted on iOS)
 * ============================================================================ */

int mount(const char *source, const char *target, const char *filesystemtype,
          unsigned long mountflags, const void *data);
int umount(const char *target);
int umount2(const char *target, int flags);

/* ============================================================================
 * FILE TRUNCATION
 * ============================================================================ */

int truncate(const char *path, off_t length);
int ftruncate(int fd, off_t length);

/* ============================================================================
 * SYNC - Flush filesystem buffers
 * ============================================================================ */

void sync(void);
int fsync(int fd);
int fdatasync(int fd);
int syncfs(int fd);

/* ============================================================================
 * DIRECTORY READING
 * ============================================================================ */

ssize_t getdents(int fd, void *dirp, size_t count);
ssize_t getdents64(int fd, void *dirp, size_t count);

/* ============================================================================
 * RANDOM
 * ============================================================================ */

ssize_t getrandom(void *buf, size_t buflen, unsigned int flags);
int getentropy(void *buffer, size_t length);

/* ============================================================================
 * PROCESS CREATION
 * ============================================================================ */

pid_t fork(void);
int vfork(void);

/* ============================================================================
 * PROCESS TERMINATION
 * ============================================================================ */

void exit(int status);
void _exit(int status);

/* ============================================================================
 * PROCESS IDENTIFICATION
 * ============================================================================ */

pid_t getpid(void);
pid_t getppid(void);

/* ============================================================================
 * USER/GROUP IDENTITY
 * ============================================================================ */

uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
int setuid(uid_t uid);
int setgid(gid_t gid);

/* ============================================================================
 * PROCESS GROUPS
 * ============================================================================ */

pid_t getpgrp(void);
pid_t getpgid(pid_t pid);
int setpgid(pid_t pid, pid_t pgid);

/* ============================================================================
 * SESSIONS
 * ============================================================================ */

pid_t setsid(void);
pid_t getsid(pid_t pid);

/* ============================================================================
 * RESOURCE LIMITS
 * ============================================================================ */

int getrlimit(int resource, struct rlimit *rlim);
int setrlimit(int resource, const struct rlimit *rlim);
int getrusage(int who, struct rusage *usage);
int prlimit(pid_t pid, int resource, const struct rlimit *new_limit, struct rlimit *old_limit);

/* ============================================================================
 * WAIT
 * ============================================================================ */

pid_t wait(int *wstatus);
pid_t waitpid(pid_t pid, int *wstatus, int options);
pid_t wait4(pid_t pid, int *wstatus, int options, struct rusage *rusage);
pid_t wait3(int *wstatus, int options, struct rusage *rusage);

/* ============================================================================
 * EXEC
 * ============================================================================ */

int execve(const char *pathname, char *const argv[], char *const envp[]);
int execv(const char *pathname, char *const argv[]);
int execvp(const char *file, char *const argv[]);
int fexecve(int fd, char *const argv[], char *const envp[]);

/* ============================================================================
 * NETWORK / SOCKET
 * ============================================================================ */

int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol, int sv[2]);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int accept4(int sockfd, struct sockaddr *addr, socklen_t *addrlen, int flags);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int shutdown(int sockfd, int how);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);
int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

int ixland_init(const ixland_config_t *config);
void ixland_cleanup(void);
const char *ixland_version(void);
int ixland_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* IXLAND_SYSCALLS_H */
