/* iXland - File Execution
 *
 * Canonical owner for exec syscalls:
 * - execve(), execv(), execvp(), execvpe()
 * - execle(), execl(), execlp()
 * - fexecve()
 *
 * Linux-shaped canonical owner - iOS mediation as implementation detail
 */

#include "exec.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* environ is not available on iOS; use _NSGetEnviron() */
#include <crt_externs.h>
#define environ (*_NSGetEnviron())

#include "../kernel/signal.h"
#include "../kernel/task.h"
#include "../runtime/native/registry.h"
#include "fdtable.h"
#include "vfs.h"

/* Deep copy argv array */
static char **ixland_exec_copy_argv(char *const argv[]) {
    if (!argv) {
        return NULL;
    }

    int argc = 0;
    while (argv[argc]) {
        argc++;
    }

    char **copy = calloc(argc + 1, sizeof(char *));
    if (!copy) {
        return NULL;
    }

    for (int i = 0; i < argc; i++) {
        copy[i] = strdup(argv[i]);
        if (!copy[i]) {
            for (int j = 0; j < i; j++) {
                free(copy[j]);
            }
            free(copy);
            return NULL;
        }
    }

    return copy;
}

/* Deep copy envp array */
static char **ixland_exec_copy_envp(char *const envp[]) {
    if (!envp) {
        return NULL;
    }

    int envc = 0;
    while (envp[envc]) {
        envc++;
    }

    char **copy = calloc(envc + 1, sizeof(char *));
    if (!copy) {
        return NULL;
    }

    for (int i = 0; i < envc; i++) {
        copy[i] = strdup(envp[i]);
        if (!copy[i]) {
            for (int j = 0; j < i; j++) {
                free(copy[j]);
            }
            free(copy);
            return NULL;
        }
    }

    return copy;
}

/* Free copied argv */
static void ixland_exec_free_argv(char **argv) {
    if (!argv) {
        return;
    }

    for (int i = 0; argv[i]; i++) {
        free(argv[i]);
    }
    free(argv);
}

/* Internal: Ensure task has an exec_image allocated */
static int ixland_exec_image_ensure(ixland_task_t *task) {
    if (!task) {
        errno = EINVAL;
        return -1;
    }

    if (task->exec_image) {
        return 0;
    }

    task->exec_image = calloc(1, sizeof(ixland_exec_image_t));
    if (!task->exec_image) {
        errno = ENOMEM;
        return -1;
    }

    return 0;
}

ixland_image_type_t ixland_exec_classify(const char *path) {
    if (ixland_native_lookup(path)) {
        return IXLAND_IMAGE_NATIVE;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return IXLAND_IMAGE_NONE;
    }

    unsigned char magic[4];
    ssize_t n = read(fd, magic, 4);
    close(fd);

    if (n < 2) {
        return IXLAND_IMAGE_NONE;
    }

    if (n >= 4 && magic[0] == 0x00 && magic[1] == 0x61 && magic[2] == 0x73 && magic[3] == 0x6d) {
        return IXLAND_IMAGE_WASI;
    }

    if (magic[0] == '#' && magic[1] == '!') {
        return IXLAND_IMAGE_SCRIPT;
    }

    return IXLAND_IMAGE_NONE;
}

int ixland_exec_close_cloexec(ixland_task_t *task) {
    if (!task || !task->files) {
        errno = EINVAL;
        return -1;
    }

    pthread_mutex_lock(&task->files->lock);
    for (size_t i = 0; i < task->files->max_fds; i++) {
        if (task->files->fd[i] && (task->files->fd[i]->flags & FD_CLOEXEC)) {
            ixland_file_t *file = task->files->fd[i];
            task->files->fd[i] = NULL;
            ixland_file_free(file);
        }
    }
    pthread_mutex_unlock(&task->files->lock);

    return 0;
}

void ixland_exec_reset_signals(ixland_sighand_t *sighand) {
    if (!sighand) {
        return;
    }

    for (int i = 0; i < IXLAND_NSIG; i++) {
        if (sighand->action[i].sa_handler != SIG_IGN) {
            sighand->action[i].sa_handler = SIG_DFL;
        }
    }

    sigemptyset(&sighand->blocked);
    sigemptyset(&sighand->pending);
}

int ixland_execve(const char *pathname, char *const argv[], char *const envp[]) {
    if (!pathname) {
        errno = EFAULT;
        return -1;
    }

    if (pathname[0] == '\0') {
        errno = ENOENT;
        return -1;
    }

    ixland_task_t *task = ixland_current_task();
    if (!task) {
        errno = ESRCH;
        return -1;
    }

    ixland_image_type_t type;
    if (ixland_native_lookup(pathname)) {
        type = IXLAND_IMAGE_NATIVE;
    } else {
        if (access(pathname, X_OK) < 0) {
            return -1;
        }

        type = ixland_exec_classify(pathname);
        if (type == IXLAND_IMAGE_NONE) {
            errno = ENOENT;
            return -1;
        }
    }

    char **argv_copy = ixland_exec_copy_argv(argv);
    char **envp_copy = ixland_exec_copy_envp(envp);

    if (argv && !argv_copy) {
        errno = ENOMEM;
        return -1;
    }
    if (envp && !envp_copy) {
        ixland_exec_free_argv(argv_copy);
        errno = ENOMEM;
        return -1;
    }

    if (ixland_exec_image_ensure(task) < 0) {
        ixland_exec_free_argv(argv_copy);
        ixland_exec_free_argv(envp_copy);
        return -1;
    }

    ixland_exec_close_cloexec(task);

    if (task->sighand) {
        ixland_exec_reset_signals(task->sighand);
    }

    if (argv_copy && argv_copy[0]) {
        strncpy(task->comm, argv_copy[0], IXLAND_MAX_NAME - 1);
        task->comm[IXLAND_MAX_NAME - 1] = '\0';
    } else {
        const char *basename = strrchr(pathname, '/');
        if (basename) {
            basename++;
        } else {
            basename = pathname;
        }
        strncpy(task->comm, basename, IXLAND_MAX_NAME - 1);
        task->comm[IXLAND_MAX_NAME - 1] = '\0';
    }

    strncpy(task->exe, pathname, IXLAND_MAX_PATH - 1);
    task->exe[IXLAND_MAX_PATH - 1] = '\0';

    int argc = 0;
    if (argv_copy) {
        while (argv_copy[argc]) {
            argc++;
        }
    }

    if (task->vfork_parent) {
        __ixland_vfork_exec_notify();
    }

    int ret;
    switch (type) {
    case IXLAND_IMAGE_NATIVE:
        ret = ixland_exec_native(task, pathname, argc, argv_copy, envp_copy);
        break;
    case IXLAND_IMAGE_WASI:
        ret = ixland_exec_wasi(task, pathname, argc, argv_copy, envp_copy);
        break;
    case IXLAND_IMAGE_SCRIPT:
        ret = ixland_exec_script(task, pathname, argc, argv_copy, envp_copy);
        break;
    default:
        errno = ENOEXEC;
        ret = -1;
    }

    ixland_exec_free_argv(argv_copy);
    ixland_exec_free_argv(envp_copy);

    return ret;
}

int ixland_execv(const char *pathname, char *const argv[]) {
    return ixland_execve(pathname, argv, environ);
}

int ixland_execvp(const char *file, char *const argv[]) {
    if (strchr(file, '/') != NULL) {
        return ixland_execv(file, argv);
    }

    const char *path_env = getenv("PATH");
    if (path_env == NULL) {
        path_env = "/usr/bin:/bin";
    }

    char *path_copy = strdup(path_env);
    if (path_copy == NULL) {
        return -1;
    }

    char *saveptr = NULL;
    char *dir = strtok_r(path_copy, ":", &saveptr);

    while (dir != NULL) {
        char fullpath[IXLAND_MAX_PATH];
        int len = snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, file);

        if (len > 0 && (size_t)len < sizeof(fullpath)) {
            struct stat st;
            if (stat(fullpath, &st) == 0 && S_ISREG(st.st_mode) && (access(fullpath, X_OK) == 0)) {
                int result = ixland_execv(fullpath, argv);
                free(path_copy);
                return result;
            }
        }

        dir = strtok_r(NULL, ":", &saveptr);
    }

    free(path_copy);
    errno = ENOENT;
    return -1;
}

int ixland_fexecve(int fd, char *const argv[], char *const envp[]) {
    (void)fd;
    (void)argv;
    (void)envp;
    errno = ENOSYS;
    return -1;
}

int ixland_exec_native(ixland_task_t *task, const char *path, int argc, char **argv, char **envp) {
    ixland_native_entry_t entry = ixland_native_lookup(path);
    if (!entry) {
        errno = ENOENT;
        return -1;
    }

    strncpy(task->exec_image->path, path, sizeof(task->exec_image->path) - 1);
    task->exec_image->path[sizeof(task->exec_image->path) - 1] = '\0';
    task->exec_image->type = IXLAND_IMAGE_NATIVE;

    return entry(task, argc, argv, envp);
}

int ixland_exec_wasi(ixland_task_t *task, const char *path, int argc, char **argv, char **envp) {
    (void)task;
    (void)path;
    (void)argc;
    (void)argv;
    (void)envp;
    errno = ENOEXEC;
    return -1;
}

int ixland_exec_script(ixland_task_t *task, const char *path, int argc, char **argv, char **envp) {
    (void)task;
    (void)path;
    (void)argc;
    (void)argv;
    (void)envp;
    errno = ENOEXEC;
    return -1;
}
