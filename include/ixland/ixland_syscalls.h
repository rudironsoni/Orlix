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
#include <sys/resource.h>
#include <sys/wait.h>
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
