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
extern pid_t fork_impl(void);
extern int vfork_impl(void);
extern void exit_impl(int status);
extern void exit_group_impl(int status);
extern pid_t getpid_impl(void);
extern pid_t getppid_impl(void);
extern pid_t getpgrp_impl(void);
extern pid_t getpgid_impl(pid_t pid);
extern int setpgid_impl(pid_t pid, pid_t pgid);
extern pid_t setsid_impl(void);
extern pid_t getsid_impl(pid_t pid);
extern pid_t wait_impl(int *wstatus);
extern pid_t waitpid_impl(pid_t pid, int *wstatus, int options);
extern pid_t wait4_impl(pid_t pid, int *wstatus, int options, struct rusage *rusage);

/* From fs/exec.c - execve, execv, execvp, fexecve are canonical and exported directly */

/* From kernel/init.c */
extern int ixland_init_internal(const ixland_config_t *config);
extern void ixland_cleanup_internal(void);
extern const char *ixland_version_internal(void);
extern int ixland_is_initialized_internal(void);

/* ============================================================================
 * PROCESS CREATION
 * ============================================================================ */

__attribute__((visibility("default"))) pid_t fork(void) {
    return fork_impl();
}

__attribute__((visibility("default"))) int vfork(void) {
    return vfork_impl();
}

/* ============================================================================
 * PROCESS TERMINATION
 * ============================================================================ */

__attribute__((visibility("default"))) void exit(int status) {
    exit_impl(status);
}

__attribute__((visibility("default"))) void _exit(int status) {
    exit_group_impl(status);
}

/* ============================================================================
 * PROCESS IDENTIFICATION
 * ============================================================================ */

__attribute__((visibility("default"))) pid_t getpid(void) {
    return getpid_impl();
}

__attribute__((visibility("default"))) pid_t getppid(void) {
    return getppid_impl();
}

/* ============================================================================
 * PROCESS GROUPS
 * ============================================================================ */

__attribute__((visibility("default"))) pid_t getpgrp(void) {
    return getpgrp_impl();
}

__attribute__((visibility("default"))) pid_t getpgid(pid_t pid) {
    return getpgid_impl(pid);
}

__attribute__((visibility("default"))) int setpgid(pid_t pid, pid_t pgid) {
    return setpgid_impl(pid, pgid);
}

/* ============================================================================
 * SESSIONS
 * ============================================================================ */

__attribute__((visibility("default"))) pid_t setsid(void) {
    return setsid_impl();
}

__attribute__((visibility("default"))) pid_t getsid(pid_t pid) {
    return getsid_impl(pid);
}

/* ============================================================================
 * WAIT
 * ============================================================================ */

__attribute__((visibility("default"))) pid_t wait(int *wstatus) {
    return wait_impl(wstatus);
}

__attribute__((visibility("default"))) pid_t waitpid(pid_t pid, int *wstatus, int options) {
    return waitpid_impl(pid, wstatus, options);
}

__attribute__((visibility("default"))) pid_t wait4(pid_t pid, int *wstatus, int options, struct rusage *rusage) {
    return wait4_impl(pid, wstatus, options, rusage);
}

__attribute__((visibility("default"))) pid_t wait3(int *wstatus, int options, struct rusage *rusage) {
    return wait4_impl(-1, wstatus, options, rusage);
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
