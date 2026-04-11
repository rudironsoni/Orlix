#ifndef IXLAND_TASK_H
#define IXLAND_TASK_H

#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "ixland/ixland_types.h"

#define IXLAND_MAX_NAME 256
#define IXLAND_NSIG 64
#define IXLAND_MAX_FD 256
#define IXLAND_MAX_ARGS 256
#define IXLAND_MAX_TASKS 1024

#ifndef RLIMIT_NLIMITS
#define RLIMIT_NLIMITS 16
#endif

typedef _Atomic pid_t atomic_pid_t;

typedef struct ixland_task ixland_task_t;
typedef struct ixland_files ixland_files_t;
typedef struct ixland_fs ixland_fs_t;
typedef struct ixland_sighand ixland_sighand_t;
typedef struct ixland_tty {
    int tty_id;
    pid_t foreground_pgrp;
    atomic_int refs;
} ixland_tty_t;

typedef struct ixland_mm_emu {
    void *exec_image_base;
    size_t exec_image_size;
} ixland_mm_emu_t;

typedef enum {
    IXLAND_IMAGE_NONE = 0,
    IXLAND_IMAGE_NATIVE,
    IXLAND_IMAGE_WASI,
    IXLAND_IMAGE_SCRIPT
} ixland_image_type_t;

typedef int (*ixland_native_entry_t)(struct ixland_task *task, int argc, char **argv, char **envp);

typedef struct ixland_exec_image {
    ixland_image_type_t type;
    char path[IXLAND_MAX_PATH];
    char interpreter[IXLAND_MAX_PATH];

    union {
        struct {
            ixland_native_entry_t entry;
        } native;
        struct {
            void *module;
            void *instance;
        } wasi;
        struct {
            char *interp_argv[IXLAND_MAX_ARGS];
            int interp_argc;
        } script;
    } u;
} ixland_exec_image_t;

typedef enum {
    IXLAND_TASK_RUNNING = 0,
    IXLAND_TASK_INTERRUPTIBLE,
    IXLAND_TASK_UNINTERRUPTIBLE,
    IXLAND_TASK_STOPPED,
    IXLAND_TASK_ZOMBIE,
    IXLAND_TASK_DEAD
} ixland_task_state_t;

struct ixland_task {
    pid_t pid;
    pid_t ppid;
    pid_t tgid;
    pid_t pgid;
    pid_t sid;

    atomic_int state;
    int exit_status;
    atomic_bool exited;
    atomic_bool signaled;
    atomic_int termsig;
    atomic_bool stopped;
    atomic_int stopsig;
    atomic_bool continued;

    pthread_t thread;
    char comm[IXLAND_MAX_NAME];
    char exe[IXLAND_MAX_PATH];

    ixland_files_t *files;
    ixland_fs_t *fs;
    ixland_sighand_t *sighand;
    ixland_tty_t *tty;
    ixland_mm_emu_t *mm;
    ixland_exec_image_t *exec_image;

    struct ixland_task *parent;
    struct ixland_task *children;
    struct ixland_task *next_sibling;
    struct ixland_task *hash_next;

    /* Vfork tracking - set when created by vfork() */
    struct ixland_task *vfork_parent;

    pthread_cond_t wait_cond;
    pthread_mutex_t wait_lock;
    int waiters;

    struct rlimit rlimits[RLIMIT_NLIMITS];

    struct timespec start_time;

    atomic_int refs;
    pthread_mutex_t lock;
};

/* Task table - exposed for tests */
extern pthread_mutex_t task_table_lock;
extern ixland_task_t *task_table[IXLAND_MAX_TASKS];

ixland_task_t *ixland_task_alloc(void);
void ixland_task_free(ixland_task_t *task);
ixland_task_t *ixland_current_task(void);
void ixland_set_current_task(ixland_task_t *task);

pid_t ixland_alloc_pid(void);
void ixland_free_pid(pid_t pid);
void ixland_pid_init(void);

int ixland_task_init(void);
void ixland_task_deinit(void);

ixland_task_t *ixland_task_lookup(pid_t pid);
int task_hash(pid_t pid);

/* Process identity functions */
pid_t ixland_getpid(void);
pid_t ixland_getppid(void);
pid_t ixland_getpgrp(void);
pid_t ixland_getpgid(pid_t pid);
int ixland_setpgid(pid_t pid, pid_t pgid);
pid_t ixland_getsid(pid_t pid);
pid_t ixland_setsid(void);

/* Fork functions */
pid_t ixland_fork(void);
int ixland_vfork(void);

/* Exit/wait functions */
void ixland_exit(int status);
void ixland__exit(int status);
pid_t ixland_wait(int *wstatus);
pid_t ixland_waitpid(pid_t pid, int *wstatus, int options);
pid_t ixland_wait3(int *wstatus, int options, struct rusage *rusage);
pid_t ixland_wait4(pid_t pid, int *wstatus, int options, struct rusage *rusage);

/* Vfork notification helpers */
void __ixland_vfork_exec_notify(void);
void __ixland_vfork_exit_notify(void);

#ifdef __cplusplus
}
#endif

#endif
