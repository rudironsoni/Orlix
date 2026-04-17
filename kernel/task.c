/* IXLandSystem/kernel/task.c
 * Virtual task/process subsystem implementation
 */
#include "task.h"

#include <errno.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../fs/fdtable.h"
#include "../fs/vfs.h"
#include "signal.h"

static __thread struct task_struct *current_task = NULL;
struct task_struct *init_task = NULL;

/* Task table - accessible to signal.c for killpg */
pthread_mutex_t task_table_lock = PTHREAD_MUTEX_INITIALIZER;
struct task_struct *task_table[TASK_MAX_TASKS] = {NULL};

int task_hash(int32_t pid) {
    return (int)(pid % TASK_MAX_TASKS);
}

struct task_struct *get_current(void) {
    return current_task;
}

void set_current(struct task_struct *task) {
    current_task = task;
}

struct task_struct *alloc_task(void) {
    struct task_struct *task = calloc(1, sizeof(struct task_struct));
    if (!task)
        return NULL;

    task->pid = alloc_pid();
    task->tgid = task->pid;
    /* A new task starts without pgid/sid; fork_impl will inherit from parent */
    task->pgid = 0;
    task->sid = 0;
    task->vfork_parent = NULL;

    atomic_init(&task->state, TASK_RUNNING);
    atomic_init(&task->refs, 1);
    atomic_init(&task->exited, false);
    atomic_init(&task->signaled, false);

    pthread_mutex_init(&task->lock, NULL);
    pthread_cond_init(&task->wait_cond, NULL);
    pthread_mutex_init(&task->wait_lock, NULL);

    /* Store start time as nanoseconds instead of struct timespec */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    task->start_time_ns = (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;

    int idx = task_hash(task->pid);
    pthread_mutex_lock(&task_table_lock);
    task->hash_next = task_table[idx];
    task_table[idx] = task;
    pthread_mutex_unlock(&task_table_lock);

    return task;
}

void free_task(struct task_struct *task) {
    if (!task)
        return;

    if (atomic_fetch_sub(&task->refs, 1) > 1)
        return;

    int idx = task_hash(task->pid);
    pthread_mutex_lock(&task_table_lock);
    struct task_struct **pp = &task_table[idx];
    while (*pp && *pp != task) {
        pp = &(*pp)->hash_next;
    }
    if (*pp) {
        *pp = task->hash_next;
    }
    pthread_mutex_unlock(&task_table_lock);

    if (task->files)
        free_files(task->files);
    if (task->fs)
        free(task->fs);
    if (task->signal)
        free_signal_struct(task->signal);
    if (task->tty)
        atomic_fetch_sub(&task->tty->refs, 1);
    if (task->mm)
        free(task->mm);
    if (task->exec_image)
        free(task->exec_image);

    pthread_cond_destroy(&task->wait_cond);
    pthread_mutex_destroy(&task->wait_lock);
    pthread_mutex_destroy(&task->lock);

    free_pid(task->pid);
    free(task);
}

struct task_struct *task_lookup(int32_t pid) {
    if (pid <= 0)
        return NULL;

    int idx = task_hash(pid);
    pthread_mutex_lock(&task_table_lock);
    struct task_struct *task = task_table[idx];
    while (task && task->pid != pid) {
        task = task->hash_next;
    }
    if (task) {
        atomic_fetch_add(&task->refs, 1);
    }
    pthread_mutex_unlock(&task_table_lock);
    return task;
}

static void task_init_once(void) {
    /* Initialize PID allocator first */
    pid_init();

  init_task = alloc_task();
  if (!init_task)
    return;

  init_task->ppid = init_task->pid;
  /* init_task is session and process group leader */
  init_task->pgid = init_task->pid;
  init_task->sid = init_task->pid;
  strncpy(init_task->comm, "init", sizeof(init_task->comm));

    init_task->files = alloc_files(NR_OPEN_DEFAULT);
    if (!init_task->files) {
        free_task(init_task);
        init_task = NULL;
        return;
    }

    init_task->fs = alloc_fs_struct();
    if (!init_task->fs) {
        free_task(init_task);
        init_task = NULL;
        return;
    }
    /* Initialize virtual root and pwd for init task */
    fs_init_root(init_task->fs, "/");
    fs_init_pwd(init_task->fs, "/");

    init_task->signal = alloc_signal_struct();
    if (!init_task->signal) {
        free_task(init_task);
        init_task = NULL;
        return;
    }

    current_task = init_task;
}

int task_init(void) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;

    pthread_once(&once, task_init_once);

    return init_task ? 0 : -1;
}

void task_deinit(void) {
    if (init_task) {
        free_task(init_task);
        init_task = NULL;
    }
}

/* ============================================================================
 * PID/IDENTITY FUNCTIONS
 * ============================================================================ */

int32_t getpid_impl(void) {
    struct task_struct *task = get_current();
    if (!task) {
        /* Try to initialize if not already done */
        if (task_init() == 0) {
            task = get_current();
        }
    }
    return task ? task->pid : 0;
}

int32_t getppid_impl(void) {
    struct task_struct *task = get_current();
    if (!task) {
        /* Try to initialize if not already done */
        if (task_init() == 0) {
            task = get_current();
        }
    }
    return task ? task->ppid : 0;
}

/* ============================================================================
 * SESSION AND PROCESS GROUP FUNCTIONS
 * ============================================================================ */

int32_t getpgrp_impl(void) {
    struct task_struct *task = get_current();
    if (!task) {
        errno = ESRCH;
        return -1;
    }
    return task->pgid;
}

int32_t getpgid_impl(int32_t pid) {
    if (pid == 0) {
        return getpgrp_impl();
    }

    struct task_struct *task = task_lookup(pid);
    if (!task) {
        errno = ESRCH;
        return -1;
    }

    int32_t pgid = task->pgid;
    free_task(task);
    return pgid;
}

int setpgid_impl(int32_t pid, int32_t pgid) {
    struct task_struct *current = get_current();
    if (!current) {
        errno = ESRCH;
        return -1;
    }

    if (pid == 0) {
        pid = current->pid;
    }

    /* Linux: reject negative pgid */
    if (pgid < 0 && pgid != 0) {
        errno = EINVAL;
        return -1;
    }

    if (pgid == 0) {
        pgid = pid;
    }

    struct task_struct *target = task_lookup(pid);
    if (!target) {
        errno = ESRCH;
        return -1;
    }

    pthread_mutex_lock(&target->lock);

    /* Linux: check permissions: caller must be target or target's parent */
    if (target->ppid != current->pid && target->pid != current->pid) {
        pthread_mutex_unlock(&target->lock);
        free_task(target);
        errno = EPERM;
        return -1;
    }

    /* Linux: session match - can't move to different session */
    if (target->sid != current->sid) {
        pthread_mutex_unlock(&target->lock);
        free_task(target);
        errno = EPERM;
        return -1;
    }

    /* Linux: cannot change PGID of a session leader */
    if (target->pid == target->sid) {
        pthread_mutex_unlock(&target->lock);
        free_task(target);
        errno = EPERM;
        return -1;
    }

    /* Linux: if joining existing group, group must exist in same session */
    if (pgid != pid) {
        /* Check if target group exists */
        int found_group = 0;
        for (int i = 0; i < TASK_MAX_TASKS; i++) {
            struct task_struct *t = task_table[i];
            while (t) {
                if (t->pgid == pgid && t->sid == target->sid) {
                    found_group = 1;
                    break;
                }
                t = t->hash_next;
            }
            if (found_group) break;
        }
        if (!found_group) {
            pthread_mutex_unlock(&target->lock);
            free_task(target);
            errno = EPERM;
            return -1;
        }
    }

    /* Linux: if child already execve'd, reject with EACCES */
    if (target->pid != current->pid && atomic_load(&target->execed)) {
        pthread_mutex_unlock(&target->lock);
        free_task(target);
        errno = EACCES;
        return -1;
    }

    target->pgid = pgid;
    pthread_mutex_unlock(&target->lock);
    free_task(target);

    return 0;
}

int32_t getsid_impl(int32_t pid) {
    if (pid == 0) {
        struct task_struct *task = get_current();
        if (!task) {
            errno = ESRCH;
            return -1;
        }
        return task->sid;
    }

    struct task_struct *task = task_lookup(pid);
    if (!task) {
        errno = ESRCH;
        return -1;
    }

    int32_t sid = task->sid;
    free_task(task);
    return sid;
}

int32_t setsid_impl(void) {
    struct task_struct *task = get_current();
    if (!task) {
        errno = ESRCH;
        return -1;
    }

    pthread_mutex_lock(&task->lock);

    /* Check if already process group leader */
    if (task->pgid == task->pid) {
        pthread_mutex_unlock(&task->lock);
        errno = EPERM;
        return -1;
    }

    /* Create new session and process group */
    task->sid = task->pid;
    task->pgid = task->pid;

    pthread_mutex_unlock(&task->lock);

    return task->pid;
}

/* ============================================================================
 * PUBLIC CANONICAL WRAPPERS
 * ============================================================================
 * These wrappers convert between POSIX/Linux public types and
 * IXLandSystem's internal representation.
 */

#include <sys/types.h>

__attribute__((visibility("default"))) pid_t getpid(void) {
    return (pid_t)getpid_impl();
}

__attribute__((visibility("default"))) pid_t getppid(void) {
    return (pid_t)getppid_impl();
}

__attribute__((visibility("default"))) pid_t getpgrp(void) {
    return (pid_t)getpgrp_impl();
}

__attribute__((visibility("default"))) pid_t getpgid(pid_t pid) {
    return (pid_t)getpgid_impl((int32_t)pid);
}

__attribute__((visibility("default"))) int setpgid(pid_t pid, pid_t pgid) {
    return setpgid_impl((int32_t)pid, (int32_t)pgid);
}

__attribute__((visibility("default"))) pid_t setsid(void) {
    return (pid_t)setsid_impl();
}

__attribute__((visibility("default"))) pid_t getsid(pid_t pid) {
    return (pid_t)getsid_impl((int32_t)pid);
}
