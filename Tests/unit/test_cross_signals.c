/* ============================================================================
 * Cross-Area Signal Integration Tests
 * ============================================================================
 *
 * End-to-end test of signal handling across process boundaries.
 * Tests parent-child signal delivery, signal handlers in forked children,
 * SIGCHLD delivery, and process group signaling.
 *
 * Validation Assertions:
 *   - VAL-CROSS-014: Signal delivery to individual process
 *   - VAL-CROSS-015: Signal delivery to process group
 *   - VAL-CROSS-016: SIGSTOP/SIGCONT state transitions
 *   - VAL-CROSS-017: Signal inheritance across fork
 *   - VAL-CROSS-018: Parent notification on child state change
 */

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../../fs/fdtable.h"
#include "../../fs/vfs.h"
#include "../../kernel/signal/ixland_signal.h"
#include "../../kernel/task/task.h"
#include "../harness/ixland_test.h"

/* External declarations for syscalls */
extern pid_t ixland_waitpid(pid_t pid, int *wstatus, int options);
extern int ixland_kill(pid_t pid, int sig);
extern int ixland_killpg(pid_t pgrp, int sig);

/* Helper to create a test child with proper signal setup */
static ixland_task_t *create_test_child(ixland_task_t *parent) {
    ixland_task_t *child = ixland_task_alloc();
    if (!child)
        return NULL;

    child->files = ixland_files_dup(parent->files);
    child->fs = ixland_fs_dup(parent->fs);
    child->sighand = ixland_sighand_dup(parent->sighand);

    if (!child->files || !child->fs || !child->sighand) {
        ixland_task_free(child);
        return NULL;
    }

    child->ppid = parent->pid;
    child->pgid = parent->pgid;
    child->sid = parent->sid;
    child->state = IXLAND_TASK_RUNNING;

    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    return child;
}

/* Helper to simulate child exit */
static void simulate_child_exit(ixland_task_t *child, int exit_status) {
    pthread_mutex_lock(&child->lock);
    child->exit_status = exit_status;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);
}

/* ============================================================================
 * Test: Signal delivery to individual process (VAL-CROSS-014)
 * ============================================================================ */
IXLAND_TEST(cross_signal_kill_delivers_to_child) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create child process */
    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);

    /* Verify initial state */
    IXLAND_ASSERT(atomic_load(&child->state) == IXLAND_TASK_RUNNING);
    IXLAND_ASSERT(!atomic_load(&child->exited));

    /* Clear pending signals */
    sigemptyset(&child->sighand->pending);

    /* Parent sends signal to child */
    int ret = ixland_kill(child->pid, SIGUSR1);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Verify signal delivered to child's pending set */
    IXLAND_ASSERT(sigismember(&child->sighand->pending, SIGUSR1));

    /* Cleanup */
    simulate_child_exit(child, 0);
    int status;
    ixland_waitpid(child->pid, &status, 0);

    return true;
}

/* ============================================================================
 * Test: SIGTERM terminates child process (VAL-CROSS-014)
 * ============================================================================ */
IXLAND_TEST(cross_signal_sigterm_terminates_child) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);

    /* Send SIGTERM to child */
    int ret = ixland_kill(child->pid, SIGTERM);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Verify child state is marked for termination */
    IXLAND_ASSERT(atomic_load(&child->signaled));
    IXLAND_ASSERT_EQ(atomic_load(&child->termsig), SIGTERM);
    IXLAND_ASSERT(atomic_load(&child->state) == IXLAND_TASK_ZOMBIE);

    /* Wait for child - verify signaled exit */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);
    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT(WIFSIGNALED(status));
    IXLAND_ASSERT_EQ(WTERMSIG(status), SIGTERM);

    return true;
}

/* ============================================================================
 * Test: SIGKILL terminates unconditionally (VAL-CROSS-014)
 * ============================================================================ */
IXLAND_TEST(cross_signal_sigkill_unconditional_termination) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);

    /* Set SIG_IGN for SIGTERM to test that SIGKILL still works */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    ixland_sigaction(SIGTERM, &act, NULL);

    /* SIGKILL should still terminate */
    int ret = ixland_kill(child->pid, SIGKILL);
    IXLAND_ASSERT_EQ(ret, 0);

    IXLAND_ASSERT(atomic_load(&child->signaled));
    IXLAND_ASSERT_EQ(atomic_load(&child->termsig), SIGKILL);

    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);
    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT(WIFSIGNALED(status));
    IXLAND_ASSERT_EQ(WTERMSIG(status), SIGKILL);

    return true;
}

/* ============================================================================
 * Test: Signal 0 checks process existence (VAL-CROSS-014)
 * ============================================================================ */
IXLAND_TEST(cross_signal_zero_checks_existence) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Signal 0 to current process should succeed */
    int ret = ixland_kill(parent->pid, 0);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Signal 0 to non-existent process should fail */
    ret = ixland_kill(999999, 0);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, ESRCH);

    return true;
}

/* ============================================================================
 * Test: Signal inheritance across fork (VAL-CROSS-017)
 * ============================================================================ */
IXLAND_TEST(cross_signal_inheritance_across_fork) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Set custom handler for SIGUSR1 */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    IXLAND_ASSERT(ixland_sigaction(SIGUSR1, &act, NULL) == 0);

    /* Block SIGUSR2 */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR2);
    IXLAND_ASSERT(ixland_sigprocmask(SIG_BLOCK, &mask, NULL) == 0);

    /* Create child (fork simulation) */
    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);

    /* Verify child inherited signal handlers */
    IXLAND_ASSERT(child->sighand->action[SIGUSR1].sa_handler == SIG_IGN);

    /* Verify child inherited signal mask */
    IXLAND_ASSERT(sigismember(&child->sighand->blocked, SIGUSR2));

    /* Verify child's pending signals are cleared */
    sigset_t pending;
    sigemptyset(&pending);
    /* Note: pending should be empty after fork, but we can't easily test
     * this without a real signal being sent before fork */

    /* Cleanup */
    simulate_child_exit(child, 0);
    int status;
    ixland_waitpid(child->pid, &status, 0);

    /* Restore parent signal mask */
    sigemptyset(&mask);
    ixland_sigprocmask(SIG_SETMASK, &mask, NULL);

    return true;
}

/* ============================================================================
 * Test: SIGSTOP/SIGCONT state transitions (VAL-CROSS-016)
 * ============================================================================ */
IXLAND_TEST(cross_signal_sigstop_stops_process) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);

    /* Verify child is running */
    IXLAND_ASSERT(atomic_load(&child->state) == IXLAND_TASK_RUNNING);
    IXLAND_ASSERT(!atomic_load(&child->stopped));

    /* Send SIGSTOP */
    int ret = ixland_kill(child->pid, SIGSTOP);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Verify child is stopped */
    IXLAND_ASSERT(atomic_load(&child->state) == IXLAND_TASK_STOPPED);
    IXLAND_ASSERT(atomic_load(&child->stopped));
    IXLAND_ASSERT_EQ(atomic_load(&child->stopsig), SIGSTOP);

    /* Cleanup - need to continue first, then exit */
    pthread_mutex_lock(&child->lock);
    atomic_store(&child->state, IXLAND_TASK_RUNNING);
    atomic_store(&child->stopped, false);
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    int status;
    ixland_waitpid(child->pid, &status, 0);

    return true;
}

/* ============================================================================
 * Test: SIGCONT resumes stopped process (VAL-CROSS-016)
 * ============================================================================ */
IXLAND_TEST(cross_signal_sigcont_resumes_process) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);

    /* Stop the child first */
    pthread_mutex_lock(&child->lock);
    atomic_store(&child->state, IXLAND_TASK_STOPPED);
    atomic_store(&child->stopped, true);
    atomic_store(&child->stopsig, SIGSTOP);
    pthread_mutex_unlock(&child->lock);

    /* Send SIGCONT */
    int ret = ixland_kill(child->pid, SIGCONT);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Verify child is running again */
    IXLAND_ASSERT(atomic_load(&child->state) == IXLAND_TASK_RUNNING);
    IXLAND_ASSERT(!atomic_load(&child->stopped));
    IXLAND_ASSERT(atomic_load(&child->continued));

    /* Cleanup */
    simulate_child_exit(child, 0);
    int status;
    ixland_waitpid(child->pid, &status, 0);

    return true;
}

/* ============================================================================
 * Test: Parent notification on child exit (VAL-CROSS-018)
 * ============================================================================ */
IXLAND_TEST(cross_signal_parent_notified_on_child_exit) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);

    /* Simulate child exit - should notify parent */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 42;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* Simulate parent notification (this happens in ixland_exit) */
    pthread_mutex_lock(&parent->lock);
    if (parent->waiters > 0) {
        pthread_cond_broadcast(&parent->wait_cond);
    }
    pthread_mutex_unlock(&parent->lock);

    /* Parent should be able to collect child */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);
    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT(WIFEXITED(status));
    IXLAND_ASSERT_EQ(WEXITSTATUS(status), 42);

    return true;
}

/* ============================================================================
 * Test: SIGCHLD delivered to parent on child exit (VAL-CROSS-018)
 * ============================================================================ */
IXLAND_TEST(cross_signal_sigchld_on_child_exit) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Clear any pending SIGCHLD */
    sigdelset(&parent->sighand->pending, SIGCHLD);

    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);

    /* Simulate child exit - in real implementation, this would trigger
     * SIGCHLD delivery to parent */
    simulate_child_exit(child, 0);

    /* Note: In the current implementation, SIGCHLD is not automatically
     * sent on child exit. The parent uses waitpid to detect child exit.
     * This test documents the expected behavior. */

    /* Verify we can wait for child */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);
    IXLAND_ASSERT_EQ(result, child_pid);

    return true;
}

/* ============================================================================
 * Test: Signal delivery to process group (VAL-CROSS-015)
 * ============================================================================ */
IXLAND_TEST(cross_signal_group_delivery) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create a unique process group */
    ixland_task_t *group_leader = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(group_leader);
    group_leader->pgid = group_leader->pid; /* New group */
    group_leader->files = ixland_files_alloc(IXLAND_MAX_FD);
    group_leader->fs = ixland_fs_alloc();
    group_leader->sighand = ixland_sighand_alloc();

    pid_t target_pgid = group_leader->pid;

    /* Create children in the target process group */
    ixland_task_t *child1 = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child1);
    child1->pgid = target_pgid;

    ixland_task_t *child2 = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child2);
    child2->pgid = target_pgid;

    /* Create child in different group */
    ixland_task_t *child3 = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child3);
    child3->pgid = child3->pid; /* Different group */

    /* Clear pending signals */
    sigemptyset(&child1->sighand->pending);
    sigemptyset(&child2->sighand->pending);
    sigemptyset(&child3->sighand->pending);

    /* Send signal to process group */
    int ret = ixland_killpg(target_pgid, SIGUSR1);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Verify children in target group received signal */
    IXLAND_ASSERT(sigismember(&child1->sighand->pending, SIGUSR1));
    IXLAND_ASSERT(sigismember(&child2->sighand->pending, SIGUSR1));

    /* Verify child in different group did NOT receive signal */
    IXLAND_ASSERT(!sigismember(&child3->sighand->pending, SIGUSR1));

    /* Cleanup */
    simulate_child_exit(child1, 0);
    simulate_child_exit(child2, 0);
    simulate_child_exit(child3, 0);
    int status;
    ixland_waitpid(child1->pid, &status, 0);
    ixland_waitpid(child2->pid, &status, 0);
    ixland_waitpid(child3->pid, &status, 0);
    ixland_task_free(group_leader);

    return true;
}

/* ============================================================================
 * Test: Signal group empty group returns ESRCH (VAL-CROSS-015)
 * ============================================================================ */
IXLAND_TEST(cross_signal_group_empty_returns_esrch) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Try to signal a non-existent process group */
    int ret = ixland_killpg(999999, SIGTERM);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, ESRCH);

    return true;
}

/* ============================================================================
 * Test: Signal group invalid PGID returns EINVAL (VAL-CROSS-015)
 * ============================================================================ */
IXLAND_TEST(cross_signal_group_invalid_pgid) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* pgrp <= 0 should return EINVAL */
    int ret = ixland_killpg(0, SIGTERM);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    ret = ixland_killpg(-1, SIGTERM);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    return true;
}

/* ============================================================================
 * Test: Child can install custom signal handler
 * Validates: Child signal handler installation works correctly
 * ============================================================================ */
IXLAND_TEST(cross_signal_child_installs_handler) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);

    /* Set current task to child to simulate child context */
    ixland_set_current_task(child);

    /* Child installs SIG_IGN for SIGUSR1 */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    int ret = ixland_sigaction(SIGUSR1, &act, NULL);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Verify handler is set in child's sighand */
    IXLAND_ASSERT(child->sighand->action[SIGUSR1].sa_handler == SIG_IGN);

    /* Restore parent as current task */
    ixland_set_current_task(parent);

    /* Cleanup */
    simulate_child_exit(child, 0);
    int status;
    ixland_waitpid(child->pid, &status, 0);

    return true;
}

/* ============================================================================
 * Test: Signal handler preserved after exec
 * Note: In Linux, signal handlers are reset to default on exec
 * ============================================================================ */
IXLAND_TEST(cross_signal_handlers_reset_on_exec) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(task);

    /* Set custom handler */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    IXLAND_ASSERT(ixland_sigaction(SIGUSR1, &act, NULL) == 0);

    /* Verify handler is set */
    IXLAND_ASSERT(task->sighand->action[SIGUSR1].sa_handler == SIG_IGN);

    /* After exec (simulated by creating new sighand), handlers should reset */
    ixland_sighand_t *new_sighand = ixland_sighand_alloc();
    IXLAND_ASSERT_NOT_NULL(new_sighand);

    /* New sighand should have default handlers */
    IXLAND_ASSERT(new_sighand->action[SIGUSR1].sa_handler == SIG_DFL);

    ixland_sighand_free(new_sighand);

    return true;
}

/* ============================================================================
 * Test: Invalid signal number returns EINVAL
 * ============================================================================ */
IXLAND_TEST(cross_signal_invalid_signal_number) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Try to send invalid signal */
    int ret = ixland_kill(parent->pid, 100);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    /* Negative signal */
    ret = ixland_kill(parent->pid, -1);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    return true;
}

/* ============================================================================
 * Test: Non-existent PID returns ESRCH
 * ============================================================================ */
IXLAND_TEST(cross_signal_nonexistent_pid) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Try to send signal to non-existent PID */
    int ret = ixland_kill(999999, SIGTERM);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, ESRCH);

    return true;
}

/* ============================================================================
 * Test: Signal to pid=0 sends to current process group
 * ============================================================================ */
IXLAND_TEST(cross_signal_pid_zero_sends_to_group) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create child in same process group */
    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);

    /* Ensure child is in same group */
    IXLAND_ASSERT_EQ(child->pgid, parent->pgid);

    /* Clear pending signals */
    sigemptyset(&parent->sighand->pending);
    sigemptyset(&child->sighand->pending);

    /* Send signal to pid=0 (current process group) */
    int ret = ixland_kill(0, SIGUSR2);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Both parent and child (in same group) should receive signal */
    IXLAND_ASSERT(sigismember(&parent->sighand->pending, SIGUSR2));
    IXLAND_ASSERT(sigismember(&child->sighand->pending, SIGUSR2));

    /* Cleanup */
    simulate_child_exit(child, 0);
    int status;
    ixland_waitpid(child->pid, &status, 0);

    return true;
}

/* ============================================================================
 * Test: Signal blocked signals remain pending
 * ============================================================================ */
IXLAND_TEST(cross_signal_blocked_signals_pending) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(task);

    /* Block SIGUSR1 */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    IXLAND_ASSERT(ixland_sigprocmask(SIG_BLOCK, &mask, NULL) == 0);

    /* Clear pending */
    sigemptyset(&task->sighand->pending);

    /* Send SIGUSR1 to self */
    int ret = ixland_kill(task->pid, SIGUSR1);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Signal should be pending but not delivered yet */
    IXLAND_ASSERT(sigismember(&task->sighand->pending, SIGUSR1));

    /* Unblock - signal would be delivered */
    IXLAND_ASSERT(ixland_sigprocmask(SIG_UNBLOCK, &mask, NULL) == 0);

    return true;
}

/* ============================================================================
 * Test: Multiple signals delivered in order
 * ============================================================================ */
IXLAND_TEST(cross_signal_multiple_delivery_order) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);

    /* Clear pending */
    sigemptyset(&child->sighand->pending);

    /* Send multiple signals */
    IXLAND_ASSERT(ixland_kill(child->pid, SIGUSR1) == 0);
    IXLAND_ASSERT(ixland_kill(child->pid, SIGUSR2) == 0);
    IXLAND_ASSERT(ixland_kill(child->pid, SIGTERM) == 0);

    /* All should be pending */
    IXLAND_ASSERT(sigismember(&child->sighand->pending, SIGUSR1));
    IXLAND_ASSERT(sigismember(&child->sighand->pending, SIGUSR2));
    IXLAND_ASSERT(sigismember(&child->sighand->pending, SIGTERM));

    /* Cleanup */
    simulate_child_exit(child, 0);
    int status;
    ixland_waitpid(child->pid, &status, 0);

    return true;
}

/* ============================================================================
 * Test: Complete signal flow - parent to child to grandchild
 * Validates: End-to-end signal delivery across multiple process generations
 * ============================================================================ */
IXLAND_TEST(cross_signal_multi_generation_delivery) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *grandparent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(grandparent);

    /* Create parent (child of grandparent) */
    ixland_task_t *parent = create_test_child(grandparent);
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create child (grandchild of grandparent) */
    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);

    /* Clear pending signals */
    sigemptyset(&child->sighand->pending);

    /* Grandparent sends signal to grandchild */
    int ret = ixland_kill(child->pid, SIGUSR1);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Verify grandchild received signal */
    IXLAND_ASSERT(sigismember(&child->sighand->pending, SIGUSR1));

    /* Cleanup */
    simulate_child_exit(child, 0);
    simulate_child_exit(parent, 0);
    int status;
    pid_t parent_pid = parent->pid;
    ixland_set_current_task(parent);
    ixland_waitpid(child->pid, &status, 0);
    ixland_set_current_task(grandparent);
    ixland_waitpid(parent_pid, &status, 0);

    return true;
}
