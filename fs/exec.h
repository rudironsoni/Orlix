/* iXland - File Execution Header
 *
 * Canonical owner for exec syscalls
 */

#ifndef IXLAND_EXEC_H
#define IXLAND_EXEC_H

#include <stdbool.h>

#include "../kernel/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Exec flags */
#define EXEC_CLOEXEC 0x01

/* Image type detection */
enum exec_image_type exec_classify(const char *path);

/* Exec implementations */
int ixland_execve(const char *pathname, char *const argv[], char *const envp[]);
int ixland_execv(const char *pathname, char *const argv[]);
int ixland_execvp(const char *file, char *const argv[]);
int ixland_fexecve(int fd, char *const argv[], char *const envp[]);
int exec_native(struct task_struct *task, const char *path, int argc, char **argv, char **envp);
int exec_wasi(struct task_struct *task, const char *path, int argc, char **argv, char **envp);
int exec_script(struct task_struct *task, const char *path, int argc, char **argv, char **envp);

/* Close FD_CLOEXEC descriptors */
int exec_close_cloexec(struct task_struct *task);

/* Reset signal handlers on exec */
void exec_reset_signals(struct sighand_struct *sighand);

#ifdef __cplusplus
}
#endif

#endif
