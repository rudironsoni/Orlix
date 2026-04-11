#include <stdlib.h>
#include <unistd.h>

#include "../../kernel/signal/ixland_signal.h"
#include "../../kernel/task/task.h"
#include "../harness/ixland_test.h"

IXLAND_TEST(task_init_state_after_alloc) {
    /* Initialize task system */
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(task);

    /* Verify initial state */
    IXLAND_ASSERT(task->state == IXLAND_TASK_RUNNING);
    IXLAND_ASSERT(task->pid > 0);
    IXLAND_ASSERT(task->ppid == task->pid); /* init task is its own parent */
    IXLAND_ASSERT(task->tgid == task->pid);
    IXLAND_ASSERT(task->pgid == task->pid);
    IXLAND_ASSERT(task->sid == task->pid);
    IXLAND_ASSERT(atomic_load(&task->refs) >= 1);

    /* Verify resources allocated */
    IXLAND_ASSERT_NOT_NULL(task->files);
    IXLAND_ASSERT_NOT_NULL(task->fs);
    IXLAND_ASSERT_NOT_NULL(task->sighand);

    return true;
}

IXLAND_TEST(task_pid_invariants) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* PIDs should be positive */
    IXLAND_ASSERT(task->pid > 0);
    IXLAND_ASSERT(task->ppid > 0);
    IXLAND_ASSERT(task->tgid > 0);
    IXLAND_ASSERT(task->pgid > 0);
    IXLAND_ASSERT(task->sid > 0);

    /* For init-like task, all IDs equal its PID */
    IXLAND_ASSERT_EQ(task->pid, task->ppid);
    IXLAND_ASSERT_EQ(task->pid, task->tgid);
    IXLAND_ASSERT_EQ(task->pid, task->pgid);
    IXLAND_ASSERT_EQ(task->pid, task->sid);

    return true;
}

IXLAND_TEST(task_refs_initial) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Initial refcount should be at least 1 */
    int refs = atomic_load(&task->refs);
    IXLAND_ASSERT(refs >= 1);

    return true;
}

IXLAND_TEST(task_lookup_returns_same_task) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    pid_t pid = task->pid;

    /* Lookup should return the same task */
    ixland_task_t *found = ixland_task_lookup(pid);
    IXLAND_ASSERT_NOT_NULL(found);
    IXLAND_ASSERT_EQ(found, task);

    /* Refcount should have incremented */
    IXLAND_ASSERT(atomic_load(&task->refs) >= 2);

    /* Free the lookup reference */
    ixland_task_free(found);

    return true;
}

IXLAND_TEST(task_lookup_invalid_pid) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Lookup invalid PID should return NULL */
    ixland_task_t *found = ixland_task_lookup(-1);
    IXLAND_ASSERT_NULL(found);

    found = ixland_task_lookup(999999);
    IXLAND_ASSERT_NULL(found);

    return true;
}

IXLAND_TEST(task_refcount_increments) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    int initial_refs = atomic_load(&task->refs);

    /* Each lookup increments refcount */
    ixland_task_t *found1 = ixland_task_lookup(task->pid);
    ixland_task_t *found2 = ixland_task_lookup(task->pid);

    IXLAND_ASSERT(atomic_load(&task->refs) == initial_refs + 2);

    ixland_task_free(found1);
    ixland_task_free(found2);

    IXLAND_ASSERT(atomic_load(&task->refs) == initial_refs);

    return true;
}

IXLAND_TEST(task_exit_status_flag) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Initially not exited */
    IXLAND_ASSERT(!atomic_load(&task->exited));
    IXLAND_ASSERT(!atomic_load(&task->signaled));

    /* Simulate exit */
    task->exit_status = 42;
    atomic_store(&task->exited, true);

    IXLAND_ASSERT_EQ(task->exit_status, 42);
    IXLAND_ASSERT(atomic_load(&task->exited));
    IXLAND_ASSERT(!atomic_load(&task->signaled));

    return true;
}

IXLAND_TEST(task_exit_signaled_flag) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Save original state to restore after test */
    bool orig_signaled = atomic_load(&task->signaled);
    int orig_termsig = atomic_load(&task->termsig);
    bool orig_exited = atomic_load(&task->exited);

    /* Simulate signaled exit */
    atomic_store(&task->signaled, true);
    atomic_store(&task->termsig, SIGTERM);
    atomic_store(&task->exited, true);

    IXLAND_ASSERT(atomic_load(&task->signaled));
    IXLAND_ASSERT_EQ(atomic_load(&task->termsig), SIGTERM);
    IXLAND_ASSERT(atomic_load(&task->exited));

    /* Restore original state to prevent test contamination */
    atomic_store(&task->signaled, orig_signaled);
    atomic_store(&task->termsig, orig_termsig);
    atomic_store(&task->exited, orig_exited);

    return true;
}

IXLAND_TEST(task_alloc_basic_state) {
    /* Allocate a fresh task without going through init */
    ixland_task_t *task = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(task);

    /* Verify ixland_task_alloc sets up minimal state */
    IXLAND_ASSERT(task->pid > 0);                      /* PID is allocated */
    IXLAND_ASSERT(task->state == IXLAND_TASK_RUNNING); /* State is set */
    IXLAND_ASSERT(atomic_load(&task->refs) == 1);      /* Refcount initialized */
    IXLAND_ASSERT_NULL(task->files);                   /* Resources not yet allocated */
    IXLAND_ASSERT_NULL(task->fs);
    IXLAND_ASSERT_NULL(task->sighand);

    /* Cleanup */
    ixland_task_free(task);

    return true;
}
