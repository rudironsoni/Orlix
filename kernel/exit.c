#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include "task.h"

/* External declaration for init task */
extern ixland_task_t *init_task;

void ixland_exit(int status) {
    ixland_task_t *task = ixland_current_task();
    if (!task) {
        _Exit(status);
    }

    pthread_mutex_lock(&task->lock);

    /* Set exit status */
    task->exit_status = status;
    atomic_store(&task->exited, true);
    atomic_store(&task->state, IXLAND_TASK_ZOMBIE);

    /* Reparent children to init (orphan adoption) */
    if (task->children && init_task && init_task != task) {
        /* Lock init's children list */
        pthread_mutex_lock(&init_task->lock);

        /* Iterate through all children and reparent them */
        ixland_task_t *child = task->children;
        while (child) {
            pthread_mutex_lock(&child->lock);

            /* Update parent pointer and ppid */
            child->parent = init_task;
            child->ppid = init_task->pid;

            pthread_mutex_unlock(&child->lock);
            child = child->next_sibling;
        }

        /* Link entire children list to init's children list */
        /* Find the last child in our list */
        ixland_task_t *last_child = task->children;
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
            pthread_cond_broadcast(&init_task->wait_cond);
        }

        pthread_mutex_unlock(&init_task->lock);
    } else if (task->children) {
        /* No init task available, just update ppid to 1 */
        ixland_task_t *child = task->children;
        while (child) {
            pthread_mutex_lock(&child->lock);
            child->ppid = 1;
            pthread_mutex_unlock(&child->lock);
            child = child->next_sibling;
        }
    }

    /* Wake up any waiters on this task */
    if (task->waiters > 0) {
        pthread_cond_broadcast(&task->wait_cond);
    }

    pthread_mutex_unlock(&task->lock);

    /* Notify vfork parent if this is a vfork child */
    if (task->vfork_parent) {
        __ixland_vfork_exit_notify();
    }

    /* Notify parent via SIGCHLD */
    if (task->parent) {
        /* In real implementation: kill(task->parent->pid, SIGCHLD); */
        pthread_mutex_lock(&task->parent->lock);
        if (task->parent->waiters > 0) {
            pthread_cond_broadcast(&task->parent->wait_cond);
        }
        pthread_mutex_unlock(&task->parent->lock);
    }

    /* Terminate thread but keep task until parent waits */
    pthread_exit(NULL);
}

void ixland__exit(int status) {
    /* Immediate exit without cleanup */
    _Exit(status);
}
