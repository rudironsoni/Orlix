#ifndef IXLAND_KERNEL_TASK_H
#define IXLAND_KERNEL_TASK_H

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

/* Task states - virtual kernel internal states */
#define TASK_RUNNING 0
#define TASK_INTERRUPTIBLE 1
#define TASK_UNINTERRUPTIBLE 2
#define TASK_STOPPED 4
#define TASK_ZOMBIE 8
#define TASK_DEAD 16

/* TTY structure - virtual kernel internal */
struct tty_struct {
    int index;
    pid_t foreground_pgrp;
    atomic_int refs;
};

/* MM structure - virtual kernel internal */
struct mm_struct {
    void *exec_image_base;
    size_t exec_image_size;
    struct address_space *vma_addr_space;
};

/* Exec image types - virtual kernel internal */
enum exec_image_type {
    EXEC_IMAGE_NONE = 0,
    EXEC_IMAGE_NATIVE,
    EXEC_IMAGE_WASI,
    EXEC_IMAGE_SCRIPT,
};

/* Exec image entry - virtual kernel internal */
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

/* Task structure - virtual kernel's internal representation of a Linux task
 * This is PRIVATE internal state, NOT Linux UAPI */
struct task_struct {
    /* Virtual PID/TGID/PGID/SID namespace identity */
    pid_t pid;
    pid_t ppid;
    pid_t tgid;
    pid_t pgid;
    pid_t sid;

    /* Virtual task lifecycle state */
    atomic_int state;
    int exit_status;
    atomic_bool exited;
    atomic_bool signaled;
    atomic_int termsig;
    atomic_bool stopped;
    atomic_int stopsig;
    atomic_bool continued;

    /* Host thread backing for this virtual task */
    pthread_t thread;
    char comm[TASK_COMM_LEN];
    char exe[MAX_PATH];

    /* Resource ownership - pointers to virtual subsystem state */
    struct files_struct *files;
    struct fs_struct *fs;
    struct sighand_struct *sighand;
    struct tty_struct *tty;
    struct mm_struct *mm;
    struct exec_image *exec_image;

    /* Virtual process hierarchy relationships */
    struct task_struct *parent;
    struct task_struct *children;
    struct task_struct *next_sibling;
    struct task_struct *hash_next;

    /* Vfork tracking - virtual kernel bookkeeping */
    struct task_struct *vfork_parent;

    /* Virtual wait queue / sleep state */
    pthread_cond_t wait_cond;
    pthread_mutex_t wait_lock;
    int waiters;

    /* Resource limits - virtual kernel tracked */
    struct rlimit rlimits[RLIMIT_NLIMITS];

    /* Start time - virtual kernel tracked */
    struct timespec start_time;

    /* Reference counting and locking */
    atomic_int refs;
    pthread_mutex_t lock;
};

/* Task global table - virtual PID namespace */
extern pthread_mutex_t task_table_lock;
extern struct task_struct *task_table[TASK_MAX_TASKS];

/* Task allocation - virtual kernel internal */
struct task_struct *alloc_task(void);
void free_task(struct task_struct *task);

/* Current task accessors - virtual kernel runtime */
struct task_struct *get_current(void);
void set_current(struct task_struct *task);

/* Virtual PID namespace management */
pid_t alloc_pid(void);
void free_pid(pid_t pid);
void pid_init(void);

/* Virtual task table management */
int task_init(void);
void task_deinit(void);
struct task_struct *task_lookup(pid_t pid);
int task_hash(pid_t pid);

/* Virtual process identity syscalls (internal helpers) */
pid_t getpid_impl(void);
pid_t getppid_impl(void);
pid_t getpgrp_impl(void);
pid_t getpgid_impl(pid_t pid);
int setpgid_impl(pid_t pid, pid_t pgid);
pid_t getsid_impl(pid_t pid);
pid_t setsid_impl(void);

/* Virtual fork/exec - internal helpers */
pid_t fork_impl(void);
int vfork_impl(void);

/* Virtual exit/wait - internal helpers */
void exit_impl(int status);
pid_t wait_impl(int *wstatus);
pid_t waitpid_impl(pid_t pid, int *wstatus, int options);
pid_t wait4_impl(pid_t pid, int *wstatus, int options, struct rusage *rusage);

/* Virtual vfork notifications */
void vfork_exec_notify(void);
void vfork_exit_notify(void);

#ifdef __cplusplus
}
#endif

#endif /* IXLAND_KERNEL_TASK_H */
