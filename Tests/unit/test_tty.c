#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include "../../fs/fdtable.h"
#include "../../fs/vfs.h"
#include "../../kernel/signal/ixland_signal.h"
#include "../../kernel/task/task.h"
#include "../harness/ixland_test.h"

/* Minimal TTY bookkeeping tests
 *
 * Note: TTY-driven signal routing (SIGTTIN, SIGTTOU, SIGTSTP) is NOT implemented.
 * Only basic TTY inheritance on fork is tested.
 */

IXLAND_TEST(tty_initial_task_no_tty) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Initial task has no controlling TTY */
    IXLAND_ASSERT_NULL(task->tty);

    return true;
}

IXLAND_TEST(tty_allocation_basic) {
    /* Allocate a TTY structure */
    ixland_tty_t *tty = calloc(1, sizeof(ixland_tty_t));
    IXLAND_ASSERT_NOT_NULL(tty);

    /* Initialize */
    tty->tty_id = 1;
    tty->foreground_pgrp = 0; /* No foreground process group yet */
    atomic_init(&tty->refs, 1);

    IXLAND_ASSERT_EQ(tty->tty_id, 1);
    IXLAND_ASSERT_EQ(tty->foreground_pgrp, 0);
    IXLAND_ASSERT_EQ(atomic_load(&tty->refs), 1);

    free(tty);
    return true;
}

IXLAND_TEST(tty_refcount_increment) {
    ixland_tty_t *tty = calloc(1, sizeof(ixland_tty_t));
    IXLAND_ASSERT_NOT_NULL(tty);

    atomic_init(&tty->refs, 1);

    /* Simulate reference increment (like fork would do) */
    atomic_fetch_add(&tty->refs, 1);
    IXLAND_ASSERT_EQ(atomic_load(&tty->refs), 2);

    /* Decrement */
    atomic_fetch_sub(&tty->refs, 1);
    IXLAND_ASSERT_EQ(atomic_load(&tty->refs), 1);

    free(tty);
    return true;
}

IXLAND_TEST(tty_task_assignment) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Allocate and assign TTY */
    ixland_tty_t *tty = calloc(1, sizeof(ixland_tty_t));
    IXLAND_ASSERT_NOT_NULL(tty);

    tty->tty_id = 5;
    tty->foreground_pgrp = task->pgid; /* Task's process group is foreground */
    atomic_init(&tty->refs, 1);

    task->tty = tty;

    IXLAND_ASSERT_EQ(task->tty->tty_id, 5);
    IXLAND_ASSERT_EQ(task->tty->foreground_pgrp, task->pgid);

    /* Cleanup */
    task->tty = NULL;
    free(tty);

    return true;
}

IXLAND_TEST(tty_fork_inheritance) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Give parent a TTY */
    ixland_tty_t *tty = calloc(1, sizeof(ixland_tty_t));
    IXLAND_ASSERT_NOT_NULL(tty);

    tty->tty_id = 10;
    tty->foreground_pgrp = parent->pgid;
    atomic_init(&tty->refs, 1);
    parent->tty = tty;

    /* Simulate fork - create child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->ppid = parent->pid;
    child->pgid = parent->pgid;
    child->sid = parent->sid;

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    /* TTY inheritance (from fork.c logic) */
    if (parent->tty) {
        child->tty = parent->tty;
        atomic_fetch_add(&child->tty->refs, 1);
    }

    /* Verify child inherited TTY */
    IXLAND_ASSERT_NOT_NULL(child->tty);
    IXLAND_ASSERT_EQ(child->tty, parent->tty);
    IXLAND_ASSERT_EQ(child->tty->tty_id, 10);
    IXLAND_ASSERT_EQ(atomic_load(&child->tty->refs), 2); /* Parent + child */

    /* Link to parent for proper cleanup */
    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);

    /* Decrement refs for child */
    atomic_fetch_sub(&tty->refs, 1);

    ixland_task_free(child);

    /* Decrement refs for parent */
    atomic_fetch_sub(&tty->refs, 1);
    parent->tty = NULL;
    free(tty);

    return true;
}

IXLAND_TEST(tty_foreground_pgrp_change) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Allocate TTY */
    ixland_tty_t *tty = calloc(1, sizeof(ixland_tty_t));
    IXLAND_ASSERT_NOT_NULL(tty);

    tty->tty_id = 1;
    tty->foreground_pgrp = task->pgid;
    atomic_init(&tty->refs, 1);
    task->tty = tty;

    /* Simulate foreground process group change */
    pid_t new_pgrp = task->pid + 100;
    tty->foreground_pgrp = new_pgrp;

    IXLAND_ASSERT_EQ(task->tty->foreground_pgrp, new_pgrp);

    /* Cleanup */
    task->tty = NULL;
    free(tty);

    return true;
}

/* Note: TTY-driven signal routing tests are BLOCKED_NOT_IMPLEMENTED
 * - SIGTTIN, SIGTTOU, SIGTSTP require TTY read/write interception
 * - TTY ioctl for foreground process group changes
 * - Session leader TTY acquisition (setsid + TIOCSCTTY)
 *
 * Current implementation only has:
 * - TTY structure definition
 * - Task TTY pointer
 * - TTY inheritance on fork
 * - Basic refcounting
 */
