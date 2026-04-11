/* iXland - File Execution Header
 *
 * Canonical owner for exec syscalls
 */

#ifndef IXLAND_EXEC_H
#define IXLAND_EXEC_H

#include <signal.h>
#include <stdbool.h>

#include "../kernel/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exec flags */
#define IXLAND_EXEC_CLOEXEC 0x01

/* Image type detection */
ixland_image_type_t ixland_exec_classify(const char *path);

/* Exec implementations */
int ixland_execve(const char *pathname, char *const argv[], char *const envp[]);
int ixland_execv(const char *pathname, char *const argv[]);
int ixland_execvp(const char *file, char *const argv[]);
int ixland_fexecve(int fd, char *const argv[], char *const envp[]);
int ixland_exec_native(ixland_task_t *task, const char *path, int argc, char **argv, char **envp);
int ixland_exec_wasi(ixland_task_t *task, const char *path, int argc, char **argv, char **envp);
int ixland_exec_script(ixland_task_t *task, const char *path, int argc, char **argv, char **envp);

/* Close FD_CLOEXEC descriptors */
int ixland_exec_close_cloexec(ixland_task_t *task);

/* Reset signal handlers on exec */
void ixland_exec_reset_signals(ixland_sighand_t *sighand);

#ifdef __cplusplus
}
#endif

#endif
