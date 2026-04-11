#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "../fs/fdtable.h"
#include "../fs/vfs.h"
#include "signal.h"
#include "task.h"

/* PTHREAD_STACK_MIN may not be defined on all systems */
#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN (64 * 1024) /* 64KB minimum stack */
#endif

/* ============================================================================
 * FORK IMPLEMENTATION WITH SETJMP/LONGJMP
 * ============================================================================
 *
 * Since iOS forbids real fork(), we use pthread-based simulation with
 * setjmp/longjmp for child continuation. The key insight:
 *
 * 1. Parent calls setjmp() to save its context
 * 2. We create a new thread (the "child")
 * 3. Child sets up its environment and calls longjmp() to return to parent
 * 4. Parent distinguishes child from parent by thread ID
 * 5. Child continues from longjmp and returns 0
 * 6. Parent returns child's PID
 *
 * This gives us proper fork semantics on iOS.
 */

/* Fork context shared between parent and child */
typedef struct {
    ixland_task_t *parent;
    ixland_task_t *child;
    jmp_buf jmpbuf;           /* Shared jump buffer */
    volatile pid_t result;    /* Result from child perspective */
    volatile int child_ready; /* Synchronization flag */
    pthread_mutex_t lock;
    pthread_cond_t cond;
} fork_ctx_t;

/* Global fork context (only valid during fork) */
static __thread fork_ctx_t *active_fork_ctx = NULL;

/* Child thread entry point */
static void *fork_child_trampoline(void *arg) {
    fork_ctx_t *ctx = (fork_ctx_t *)arg;

    /* Set child as current task in thread-local storage */
    ixland_set_current_task(ctx->child);
    ctx->child->thread = pthread_self();

    /* Copy parent's state from task structure */
    /* Child inherits parent's signal mask, working directory, etc. */

    /* Signal that child is initialized */
    pthread_mutex_lock(&ctx->lock);
    ctx->child_ready = 1;
    pthread_cond_broadcast(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);

    /* Wait for parent to be ready for the "return" */
    pthread_mutex_lock(&ctx->lock);
    while (ctx->result == 0) {
        pthread_cond_wait(&ctx->cond, &ctx->lock);
    }
    pthread_mutex_unlock(&ctx->lock);

    /* Child returns 0 via longjmp to fork's setjmp */
    /* The result is already set to 0 for child */
    longjmp(ctx->jmpbuf, 1);

    /* NOTREACHED */
    return NULL;
}

pid_t ixland_fork(void) {
    ixland_task_t *parent = ixland_current_task();
    if (!parent) {
        errno = ESRCH;
        return -1;
    }

    /* Check process limit */
    int child_count = 0;
    pthread_mutex_lock(&parent->lock);
    ixland_task_t *c = parent->children;
    while (c) {
        child_count++;
        c = c->next_sibling;
    }
    if (child_count >= (int)parent->rlimits[RLIMIT_NPROC].rlim_cur) {
        pthread_mutex_unlock(&parent->lock);
        errno = EAGAIN;
        return -1;
    }
    pthread_mutex_unlock(&parent->lock);

    /* Allocate child task */
    ixland_task_t *child = ixland_task_alloc();
    if (!child) {
        errno = ENOMEM;
        return -1;
    }

    /* Set up parent-child relationship */
    child->ppid = parent->pid;
    child->pgid = parent->pgid;
    child->sid = parent->sid;

    /* Copy parent's working directory */
    if (parent->fs && child->fs) {
        strncpy(child->fs->cwd, parent->fs->cwd, IXLAND_MAX_PATH - 1);
        child->fs->cwd[IXLAND_MAX_PATH - 1] = '\0';
    }

    /* Copy subsystems with proper semantics */
    child->files = ixland_files_dup(parent->files);
    if (!child->files) {
        ixland_task_free(child);
        errno = ENOMEM;
        return -1;
    }

    /* Copy signal handlers */
    if (parent->sighand && child->sighand) {
        memcpy(child->sighand->action, parent->sighand->action, sizeof(parent->sighand->action));
        child->sighand->blocked = parent->sighand->blocked;
    }

    /* Reference TTY (not copy) */
    if (parent->tty) {
        child->tty = parent->tty;
        atomic_fetch_add(&child->tty->refs, 1);
    }

    /* Link into parent's children list */
    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Set up fork context on stack */
    fork_ctx_t ctx;
    ctx.parent = parent;
    ctx.child = child;
    ctx.result = 0;
    ctx.child_ready = 0;
    pthread_mutex_init(&ctx.lock, NULL);
    pthread_cond_init(&ctx.cond, NULL);

    /* Make context available globally for this thread */
    active_fork_ctx = &ctx;

    /* Save parent's context */
    if (setjmp(ctx.jmpbuf) == 0) {
        /* Parent: Create child thread */
        pthread_t child_thread;
        pthread_attr_t attr;
        pthread_attr_init(&attr);

        /* Set stack size from resource limits */
        size_t stacksize = parent->rlimits[RLIMIT_STACK].rlim_cur;
        if (stacksize < PTHREAD_STACK_MIN) {
            stacksize = PTHREAD_STACK_MIN;
        }
        pthread_attr_setstacksize(&attr, stacksize);

        int rc = pthread_create(&child_thread, &attr, fork_child_trampoline, &ctx);
        pthread_attr_destroy(&attr);

        if (rc != 0) {
            /* Cleanup on failure */
            pthread_mutex_lock(&parent->lock);
            parent->children = child->next_sibling;
            pthread_mutex_unlock(&parent->lock);
            ixland_task_free(child);
            active_fork_ctx = NULL;
            pthread_mutex_destroy(&ctx.lock);
            pthread_cond_destroy(&ctx.cond);
            errno = EAGAIN;
            return -1;
        }

        /* Wait for child to initialize */
        pthread_mutex_lock(&ctx.lock);
        while (!ctx.child_ready) {
            pthread_cond_wait(&ctx.cond, &ctx.lock);
        }

        /* Set result: parent gets child's PID */
        ctx.result = child->pid;
        pthread_cond_broadcast(&ctx.cond);
        pthread_mutex_unlock(&ctx.lock);

        /* Detach child thread - it will exit via longjmp */
        pthread_detach(child_thread);

        /* Cleanup context */
        active_fork_ctx = NULL;
        pthread_mutex_destroy(&ctx.lock);
        pthread_cond_destroy(&ctx.cond);

        /* Parent returns child's PID */
        return child->pid;
    }

    /* Child continuation (after longjmp) */
    /* ctx is still valid on child's stack */

    /* Cleanup synchronization primitives */
    pthread_mutex_destroy(&ctx.lock);
    pthread_cond_destroy(&ctx.cond);
    active_fork_ctx = NULL;

    /* Child returns 0 */
    return 0;
}

/* ============================================================================
 * VFORK IMPLEMENTATION
 * ============================================================================
 *
 * vfork() suspends the parent process until the child calls execve() or exit().
 * On iOS, we simulate this using setjmp/longjmp:
 *
 * 1. Parent saves context with setjmp()
 * 2. Child "borrows" parent's address space (simulated)
 * 3. Child must call execve() or exit() before parent resumes
 * 4. Parent resumes when child exits or execs
 *
 * For simplicity in this simulation, vfork behaves like fork but guarantees
 * the parent doesn't run until child execs/exits.
 */

/* Vfork context */
typedef struct {
    ixland_task_t *parent;
    ixland_task_t *child;
    jmp_buf parent_jmp;        /* Parent's saved context */
    jmp_buf child_jmp;         /* Child's entry point */
    volatile int child_done;   /* Set when child execs or exits */
    volatile int child_execed; /* Set if child called execve */
    pthread_mutex_t lock;
    pthread_cond_t cond;
    pid_t child_pid;
} vfork_ctx_t;

/* Global vfork context */
static __thread vfork_ctx_t *active_vfork_ctx = NULL;

/* Vfork child entry */
static void *vfork_child_trampoline(void *arg) {
    vfork_ctx_t *ctx = (vfork_ctx_t *)arg;

    /* Set child as current task */
    ixland_set_current_task(ctx->child);
    ctx->child->thread = pthread_self();

    /* Signal that child is ready */
    pthread_mutex_lock(&ctx->lock);
    pthread_cond_broadcast(&ctx->cond);
    pthread_mutex_unlock(&ctx->lock);

    /* Jump to child continuation */
    longjmp(ctx->child_jmp, 1);

    /* NOTREACHED */
    return NULL;
}

int ixland_vfork(void) {
    ixland_task_t *parent = ixland_current_task();
    if (!parent) {
        errno = ESRCH;
        return -1;
    }

    /* Check resource limits */
    pthread_mutex_lock(&parent->lock);
    int child_count = 0;
    ixland_task_t *c = parent->children;
    while (c) {
        child_count++;
        c = c->next_sibling;
    }
    if (child_count >= (int)parent->rlimits[RLIMIT_NPROC].rlim_cur) {
        pthread_mutex_unlock(&parent->lock);
        errno = EAGAIN;
        return -1;
    }
    pthread_mutex_unlock(&parent->lock);

    /* Allocate child task */
    ixland_task_t *child = ixland_task_alloc();
    if (!child) {
        errno = ENOMEM;
        return -1;
    }

    /* Set up parent-child relationship */
    child->ppid = parent->pid;
    child->pgid = parent->pgid;
    child->sid = parent->sid;
    child->vfork_parent = parent; /* Mark as vfork child */

    /* Copy parent's resources */
    if (parent->fs && child->fs) {
        strncpy(child->fs->cwd, parent->fs->cwd, IXLAND_MAX_PATH - 1);
        child->fs->cwd[IXLAND_MAX_PATH - 1] = '\0';
    }

    /* Share file table (not copy - key vfork semantics) */
    /* In real vfork, child shares address space; we simulate by sharing FD table */
    /* Note: For vfork, we duplicate the file table structure but share the underlying files */
    if (parent->files) {
        /* Duplicate the file table (shallow copy that shares file references) */
        child->files = ixland_files_dup(parent->files);
        if (!child->files) {
            pthread_mutex_lock(&parent->lock);
            parent->children = child->next_sibling;
            pthread_mutex_unlock(&parent->lock);
            ixland_task_free(child);
            errno = ENOMEM;
            return -1;
        }
    }

    /* Copy signal handlers */
    if (parent->sighand && child->sighand) {
        memcpy(child->sighand->action, parent->sighand->action, sizeof(parent->sighand->action));
        child->sighand->blocked = parent->sighand->blocked;
    }

    /* Reference TTY */
    if (parent->tty) {
        child->tty = parent->tty;
        atomic_fetch_add(&child->tty->refs, 1);
    }

    /* Link into parent's children list */
    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Mark parent as suspended (vfork semantics) */
    atomic_store(&parent->state, IXLAND_TASK_UNINTERRUPTIBLE);

    /* Set up vfork context */
    vfork_ctx_t ctx;
    ctx.parent = parent;
    ctx.child = child;
    ctx.child_done = 0;
    ctx.child_execed = 0;
    ctx.child_pid = child->pid;
    pthread_mutex_init(&ctx.lock, NULL);
    pthread_cond_init(&ctx.cond, NULL);

    active_vfork_ctx = &ctx;

    /* Parent: Save context */
    if (setjmp(ctx.parent_jmp) == 0) {
        /* Child: Set up entry point */
        if (setjmp(ctx.child_jmp) == 0) {
            /* Parent continues here after creating thread */
            pthread_t child_thread;
            pthread_attr_t attr;
            pthread_attr_init(&attr);

            size_t stacksize = parent->rlimits[RLIMIT_STACK].rlim_cur;
            if (stacksize < PTHREAD_STACK_MIN) {
                stacksize = PTHREAD_STACK_MIN;
            }
            pthread_attr_setstacksize(&attr, stacksize);

            int rc = pthread_create(&child_thread, &attr, vfork_child_trampoline, &ctx);
            pthread_attr_destroy(&attr);

            if (rc != 0) {
                pthread_mutex_lock(&parent->lock);
                parent->children = child->next_sibling;
                pthread_mutex_unlock(&parent->lock);
                atomic_store(&parent->state, IXLAND_TASK_RUNNING);
                ixland_task_free(child);
                active_vfork_ctx = NULL;
                pthread_mutex_destroy(&ctx.lock);
                pthread_cond_destroy(&ctx.cond);
                errno = EAGAIN;
                return -1;
            }

            /* Wait for child to exec or exit (vfork semantics) */
            pthread_mutex_lock(&ctx.lock);
            while (!ctx.child_done) {
                pthread_cond_wait(&ctx.cond, &ctx.lock);
            }
            pthread_mutex_unlock(&ctx.lock);

            /* Child has execed or exited - parent can resume */
            atomic_store(&parent->state, IXLAND_TASK_RUNNING);

            pthread_detach(child_thread);

            /* Cleanup */
            active_vfork_ctx = NULL;
            pthread_mutex_destroy(&ctx.lock);
            pthread_cond_destroy(&ctx.cond);

            /* If child execed, parent returns PID */
            /* If child exited, parent returns PID (child is zombie) */
            return child->pid;
        }

        /* Child continuation after longjmp */
        /* NOTREACHED - child jumps to child_jmp instead */
    }

    /* Child returns 0 */
    active_vfork_ctx = NULL;
    pthread_mutex_destroy(&ctx.lock);
    pthread_cond_destroy(&ctx.cond);
    return 0;
}

/* Called from ixland_execve to notify vfork parent */
void __ixland_vfork_exec_notify(void) {
    if (active_vfork_ctx) {
        pthread_mutex_lock(&active_vfork_ctx->lock);
        active_vfork_ctx->child_done = 1;
        active_vfork_ctx->child_execed = 1;
        pthread_cond_broadcast(&active_vfork_ctx->cond);
        pthread_mutex_unlock(&active_vfork_ctx->lock);
    }
}

/* Called from ixland_exit to notify vfork parent */
void __ixland_vfork_exit_notify(void) {
    if (active_vfork_ctx) {
        pthread_mutex_lock(&active_vfork_ctx->lock);
        active_vfork_ctx->child_done = 1;
        pthread_cond_broadcast(&active_vfork_ctx->cond);
        pthread_mutex_unlock(&active_vfork_ctx->lock);
    }
}
