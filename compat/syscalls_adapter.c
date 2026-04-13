/*
 * IXLandSystem Syscall Adapter
 *
 * Bridges public package seam to internal owners.
 * These functions are the ONLY entry points IXLandLibC may call.
 */

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../include/ixland/ixland_types.h"

/* ============================================================================
 * Internal owner declarations (from kernel/)
 * ============================================================================ */
extern pid_t do_fork(void);
extern int do_vfork(void);
extern void do_exit(int status);
extern void do_exit_group(int status);
extern pid_t do_getpid(void);
extern pid_t do_getppid(void);
extern pid_t do_getpgrp(void);
extern pid_t do_getpgid(pid_t pid);
extern int do_setpgid(pid_t pid, pid_t pgid);
extern pid_t do_setsid(void);
extern pid_t do_getsid(pid_t pid);
extern pid_t do_wait(int *wstatus);
extern pid_t do_waitpid(pid_t pid, int *wstatus, int options);
extern pid_t do_wait4(pid_t pid, int *wstatus, int options, struct rusage *rusage);

/* From fs/exec.c */
extern int ixland_execve_internal(const char *pathname, char *const argv[], char *const envp[]);
extern int ixland_execv_internal(const char *pathname, char *const argv[]);
extern int ixland_execvp_internal(const char *file, char *const argv[]);
extern int ixland_fexecve_internal(int fd, char *const argv[], char *const envp[]);

/* From kernel/init.c */
extern int ixland_init_internal(const ixland_config_t *config);
extern void ixland_cleanup_internal(void);
extern const char *ixland_version_internal(void);
extern int ixland_is_initialized_internal(void);

/* ============================================================================
 * PROCESS CREATION
 * ============================================================================ */

pid_t ixland_fork(void) {
    return do_fork();
}

int ixland_vfork(void) {
    return do_vfork();
}

/* ============================================================================
 * PROCESS TERMINATION
 * ============================================================================ */

void ixland_exit(int status) {
    do_exit(status);
}

void ixland__exit(int status) __attribute__((noreturn));
void ixland__exit(int status) {
    do_exit_group(status);
    __builtin_unreachable();
}

/* ============================================================================
 * PROCESS IDENTIFICATION
 * ============================================================================ */

pid_t ixland_getpid(void) {
    return do_getpid();
}

pid_t ixland_getppid(void) {
    return do_getppid();
}

/* ============================================================================
 * PROCESS GROUPS
 * ============================================================================ */

pid_t ixland_getpgrp(void) {
    return do_getpgrp();
}

pid_t ixland_getpgid(pid_t pid) {
    return do_getpgid(pid);
}

int ixland_setpgid(pid_t pid, pid_t pgid) {
    return do_setpgid(pid, pgid);
}

/* ============================================================================
 * SESSIONS
 * ============================================================================ */

pid_t ixland_setsid(void) {
    return do_setsid();
}

pid_t ixland_getsid(pid_t pid) {
    return do_getsid(pid);
}

/* ============================================================================
 * WAIT
 * ============================================================================ */

pid_t ixland_wait(int *wstatus) {
    return do_wait(wstatus);
}

pid_t ixland_waitpid(pid_t pid, int *wstatus, int options) {
    return do_waitpid(pid, wstatus, options);
}

pid_t ixland_wait4(pid_t pid, int *wstatus, int options, struct rusage *rusage) {
    return do_wait4(pid, wstatus, options, rusage);
}

pid_t ixland_wait3(int *wstatus, int options, struct rusage *rusage) {
    return do_wait4(-1, wstatus, options, rusage);
}

/* ============================================================================
 * EXEC
 * ============================================================================ */

int ixland_execve(const char *pathname, char *const argv[], char *const envp[]) {
    return ixland_execve_internal(pathname, argv, envp);
}

int ixland_execv(const char *pathname, char *const argv[]) {
    return ixland_execv_internal(pathname, argv);
}

int ixland_execvp(const char *file, char *const argv[]) {
    return ixland_execvp_internal(file, argv);
}

int ixland_fexecve(int fd, char *const argv[], char *const envp[]) {
    return ixland_fexecve_internal(fd, argv, envp);
}

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

int ixland_init(const ixland_config_t *config) {
    return ixland_init_internal(config);
}

void ixland_cleanup(void) {
    ixland_cleanup_internal();
}

const char *ixland_version(void) {
    return ixland_version_internal();
}

int ixland_is_initialized(void) {
    return ixland_is_initialized_internal();
}
