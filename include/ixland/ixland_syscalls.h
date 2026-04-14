/*
 * IXLandSystem Public Syscall Interface
 *
 * Public package seam between IXLandSystem and IXLandLibC.
 * These are the ONLY functions IXLandLibC SHALL call from IXLandSystem.
 * All names are product-prefixed to avoid symbol collisions.
 * These functions delegate to internal IXLandSystem owners.
 */

#ifndef IXLAND_SYSCALLS_H
#define IXLAND_SYSCALLS_H

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/wait.h>

#ifdef __cplusplus
extern "C" {
#endif

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
