#ifndef TASK_H
#define TASK_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <time.h>

#include "../fs/fdtable.h"
#include "../fs/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TASK_COMM_LEN 16
#define TASK_MAX_ARGS 256
#define TASK_MAX_TASKS 1024

#ifndef RLIMIT_NLIMITS
#define RLIMIT_NLIMITS 16
#endif

/* Forward declarations */
struct task_struct;
struct tty_struct;
struct mm_struct;
struct exec_image;
struct sighand_struct;

/* Task states - Linux-style */
#define TASK_RUNNING 0
#define TASK_INTERRUPTIBLE 1
#define TASK_UNINTERRUPTIBLE 2
#define TASK_STOPPED 4
#define TASK_ZOMBIE 8
#define TASK_DEAD 16

/* TTY structure - canonical owner */
struct tty_struct {
    int index;
    pid_t foreground_pgrp;
    atomic_int refs;
};

/* MM structure - canonical owner */
struct mm_struct {
    void *exec_image_base;
    size_t exec_image_size;
    struct address_space *vma_addr_space;
};

/* Exec image types */
enum exec_image_type {
    EXEC_IMAGE_NONE = 0,
    EXEC_IMAGE_NATIVE,
    EXEC_IMAGE_WASI,
    EXEC_IMAGE_SCRIPT,
};

/* Exec image entry */
typedef int (*native_entry_t)(struct task_struct *task, int argc, char **argv, char **envp);

struct exec_image {
    enum exec_image_type type;
    char path[MAX_PATH];
    char interpreter[MAX_PATH];

    union {
        struct {
            native_entry_t entry;
        } native;
        struct {
            void *module;
            void *instance;
        } wasi;
        struct {
            char *interp_argv[TASK_MAX_ARGS];
            int interp_argc;
        } script;
    } u;
};

/* Task structure - Linux-style task_struct */
struct task_struct {
    /* PIDs */
    pid_t pid;
    pid_t ppid;
    pid_t tgid;
    pid_t pgid;
    pid_t sid;

    /* State */
    atomic_int state;
    int exit_status;
    atomic_bool exited;
    atomic_bool signaled;
    atomic_int termsig;
    atomic_bool stopped;
    atomic_int stopsig;
    atomic_bool continued;

    /* Thread and identity */
    pthread_t thread;
    char comm[TASK_COMM_LEN];
    char exe[MAX_PATH];

    /* Subsystem pointers */
    struct files_struct *files;
    struct fs_struct *fs;
    struct sighand_struct *sighand;
    struct tty_struct *tty;
    struct mm_struct *mm;
    struct exec_image *exec_image;

    /* Process hierarchy */
    struct task_struct *parent;
    struct task_struct *children;
    struct task_struct *next_sibling;
    struct task_struct *hash_next;

    /* Vfork tracking */
    struct task_struct *vfork_parent;

    /* Waiting */
    pthread_cond_t wait_cond;
    pthread_mutex_t wait_lock;
    int waiters;

    /* Resources */
    struct rlimit rlimits[RLIMIT_NLIMITS];

    /* Start time */
    struct timespec start_time;

    /* Reference counting and locking */
    atomic_int refs;
    pthread_mutex_t lock;
};

/* Task global table */
extern pthread_mutex_t task_table_lock;
extern struct task_struct *task_table[TASK_MAX_TASKS];

/* Task allocation */
struct task_struct *alloc_task(void);
void free_task(struct task_struct *task);

/* Current task accessors */
struct task_struct *get_current(void);
void set_current(struct task_struct *task);

/* PID allocation */
pid_t alloc_pid(void);
void free_pid(pid_t pid);
void pid_init(void);

/* Task table management */
int task_init(void);
void task_deinit(void);
struct task_struct *task_lookup(pid_t pid);
int task_hash(pid_t pid);

/* Process identity */
pid_t do_getpid(void);
pid_t do_getppid(void);
pid_t do_getpgrp(void);
pid_t do_getpgid(pid_t pid);
int do_setpgid(pid_t pid, pid_t pgid);
pid_t do_getsid(pid_t pid);
pid_t do_setsid(void);

/* Fork/exec */
pid_t do_fork(void);
int do_vfork(void);

/* Exit/wait */
void do_exit(int status);
void do_exit_group(int status);
pid_t do_wait(int *wstatus);
pid_t do_waitpid(pid_t pid, int *wstatus, int options);
pid_t do_wait4(pid_t pid, int *wstatus, int options, struct rusage *rusage);

/* Vfork notifications */
void vfork_exec_notify(void);
void vfork_exit_notify(void);

#ifdef __cplusplus
}
#endif

#endif /* TASK_H */
