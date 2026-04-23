#include <errno.h>
#include <stdlib.h>

#include "task.h"

/* External declaration for init task */
extern struct task_struct *init_task;

void exit_impl(int status) {
    struct task_struct *task = get_current();
    if (!task) {
        _Exit(status);
    }

    ix_mutex_lock_impl(&task->lock);

    /* Set exit status */
    task->exit_status = status;
    atomic_store(&task->exited, true);
    atomic_store(&task->state, TASK_ZOMBIE);

    /* Reparent children to init (orphan adoption) */
    if (task->children && init_task && init_task != task) {
        /* Lock init's children list */
        ix_mutex_lock_impl(&init_task->lock);

        /* Iterate through all children and reparent them */
        struct task_struct *child = task->children;
        while (child) {
            ix_mutex_lock_impl(&child->lock);

            /* Update parent pointer and ppid */
            child->parent = init_task;
            child->ppid = init_task->pid;

            ix_mutex_unlock_impl(&child->lock);
            child = child->next_sibling;
        }

        /* Link entire children list to init's children list */
        /* Find the last child in our list */
        struct task_struct *last_child = task->children;
        while (last_child->next_sibling) {
            last_child = last_child->next_sibling;
        }

        /* Prepend our children list to init's children list */
        last_child->next_sibling = init_task->children;
        init_task->children = task->children;

        /* Clear our children list */
        task->children = NULL;

        /* Wake up init if it's waiting for children */
        if (init_task->waiters > 0) {
            ix_cond_broadcast_impl(&init_task->wait_cond);
        }

        ix_mutex_unlock_impl(&init_task->lock);
    } else if (task->children) {
        /* No init task available, just update ppid to 1 */
        struct task_struct *child = task->children;
        while (child) {
            ix_mutex_lock_impl(&child->lock);
            child->ppid = 1;
            ix_mutex_unlock_impl(&child->lock);
            child = child->next_sibling;
        }
    }

    /* Wake up any waiters on this task */
    if (task->waiters > 0) {
        ix_cond_broadcast_impl(&task->wait_cond);
    }

    ix_mutex_unlock_impl(&task->lock);

    /* Notify vfork parent if this is a vfork child */
    if (task->vfork_parent) {
        vfork_exit_notify();
    }

    /* Notify parent via SIGCHLD */
    if (task->parent) {
        /* In real implementation: kill(task->parent->pid, SIGCHLD); */
        ix_mutex_lock_impl(&task->parent->lock);
        if (task->parent->waiters > 0) {
            ix_cond_broadcast_impl(&task->parent->wait_cond);
        }
        ix_mutex_unlock_impl(&task->parent->lock);
    }

    /* Terminate thread but keep task until parent waits */
}

__attribute__((visibility("default"), __noreturn__)) void exit(int status) {
    exit_impl(status);
    ix_thread_exit_impl(NULL);
    _Exit(status);
}

__attribute__((visibility("default"))) void _exit(int status) {
    /* Immediate exit without cleanup */
    _Exit(status);
}
