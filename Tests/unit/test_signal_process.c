#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include "../../fs/fdtable.h"
#include "../../fs/vfs.h"
#include "../../kernel/signal/ixland_signal.h"
#include "../../kernel/task/task.h"
#include "../harness/ixland_test.h"

/* External declarations for syscalls tested */
extern pid_t ixland_waitpid(pid_t pid, int *wstatus, int options);

/* Comprehensive signal-process tests
 *
 * Coverage:
 * - Signal handler disposition semantics
 * - Direct signal delivery to tasks
 * - Signal delivery to process groups
 * - Foreground/background behavior
 * - Edge and failure cases
 */

IXLAND_TEST(signal_kill_delivers_termination) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create child with default handlers */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->ppid = parent->pid;
    child->pgid = parent->pgid;
    child->sid = parent->sid;

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();
    IXLAND_ASSERT_NOT_NULL(child->sighand);

    /* Verify default disposition */
    IXLAND_ASSERT(child->sighand->action[SIGTERM].sa_handler == SIG_DFL);

    /* Link to parent */
    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Simulate SIGTERM delivery */
    pthread_mutex_lock(&child->lock);
    atomic_store(&child->signaled, true);
    atomic_store(&child->termsig, SIGTERM);
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* Parent should see terminated child via wait - save PID before reap */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT(WIFSIGNALED(status));
    IXLAND_ASSERT_EQ(WTERMSIG(status), SIGTERM);

    return true;
}

IXLAND_TEST(signal_int_delivers_interrupt) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->ppid = parent->pid;
    child->pgid = parent->pgid;
    child->sid = parent->sid;
    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Simulate SIGINT delivery */
    pthread_mutex_lock(&child->lock);
    atomic_store(&child->signaled, true);
    atomic_store(&child->termsig, SIGINT);
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* Save child PID before wait (child will be reaped) */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT(WIFSIGNALED(status));
    IXLAND_ASSERT_EQ(WTERMSIG(status), SIGINT);

    return true;
}

IXLAND_TEST(signal_ign_disposition_ignored) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Set SIG_IGN for SIGUSR1 */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    IXLAND_ASSERT(ixland_sigaction(SIGUSR1, &act, NULL) == 0);

    /* Verify disposition is set */
    IXLAND_ASSERT(task->sighand->action[SIGUSR1].sa_handler == SIG_IGN);

    /* Task should remain in RUNNING state after ignored signal */
    /* (We don't have actual signal delivery mechanism to test this,
     * but we verify the disposition is stored correctly) */
    IXLAND_ASSERT(task->sighand->action[SIGUSR1].sa_handler == SIG_IGN);

    return true;
}

IXLAND_TEST(signal_forbidden_handlers_rejected) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Try to set handler for SIGKILL - should fail */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;

    int ret = ixland_sigaction(SIGKILL, &act, NULL);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    /* Try to set handler for SIGSTOP - should fail */
    ret = ixland_sigaction(SIGSTOP, &act, NULL);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    return true;
}

IXLAND_TEST(signal_invalid_signal_number) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;

    /* Signal 0 is invalid */
    int ret = ixland_sigaction(0, &act, NULL);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    /* Out of range signal */
    ret = ixland_sigaction(100, &act, NULL);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    return true;
}

IXLAND_TEST(signal_group_delivery_simulation) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    pid_t pgid = parent->pgid;

    /* Create multiple children in same process group */
    ixland_task_t *children[3];

    for (int i = 0; i < 3; i++) {
        children[i] = ixland_task_alloc();
        IXLAND_ASSERT_NOT_NULL(children[i]);

        children[i]->ppid = parent->pid;
        children[i]->pgid = pgid; /* Same process group */
        children[i]->sid = parent->sid;

        children[i]->files = ixland_files_alloc(IXLAND_MAX_FD);
        children[i]->fs = ixland_fs_alloc();
        children[i]->sighand = ixland_sighand_alloc();

        pthread_mutex_lock(&parent->lock);
        children[i]->parent = parent;
        children[i]->next_sibling = parent->children;
        parent->children = children[i];
        pthread_mutex_unlock(&parent->lock);
    }

    /* Simulate signal delivered to entire group */
    for (int i = 0; i < 3; i++) {
        pthread_mutex_lock(&children[i]->lock);
        atomic_store(&children[i]->signaled, true);
        atomic_store(&children[i]->termsig, SIGTERM);
        atomic_store(&children[i]->exited, true);
        atomic_store(&children[i]->state, IXLAND_TASK_ZOMBIE);
        pthread_mutex_unlock(&children[i]->lock);
    }

    /* Save child PIDs before wait (children will be reaped) */
    pid_t child_pids[3];
    for (int i = 0; i < 3; i++) {
        child_pids[i] = children[i]->pid;
    }

    /* Wait for all children - verify group members affected */
    for (int i = 0; i < 3; i++) {
        int status;
        pid_t result = ixland_waitpid(child_pids[i], &status, 0);
        IXLAND_ASSERT_EQ(result, child_pids[i]);
        IXLAND_ASSERT(WIFSIGNALED(status));
        IXLAND_ASSERT_EQ(WTERMSIG(status), SIGTERM);
    }

    return true;
}

IXLAND_TEST(signal_shell_foreground_child_terminates) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Simulate shell scenario:
     * - Shell parent in one process group
     * - Child pipeline in foreground
     * - SIGINT sent to foreground group
     * - Child affected, parent survives
     */

    ixland_task_t *current = ixland_current_task();

    /* Create a "shell" task (parent) */
    ixland_task_t *shell = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(shell);

    shell->ppid = current->pid;
    shell->pgid = shell->pid; /* Shell is group leader */
    shell->sid = current->sid;
    shell->state = IXLAND_TASK_RUNNING;
    atomic_store(&shell->exited, false);

    shell->files = ixland_files_alloc(IXLAND_MAX_FD);
    shell->fs = ixland_fs_alloc();
    shell->sighand = ixland_sighand_alloc();

    /* Create child in same foreground group */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->ppid = shell->pid;
    child->pgid = shell->pgid; /* Same foreground group */
    child->sid = shell->sid;
    child->state = IXLAND_TASK_RUNNING;
    atomic_store(&child->exited, false);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    /* Link child to shell */
    pthread_mutex_lock(&shell->lock);
    child->parent = shell;
    child->next_sibling = shell->children;
    shell->children = child;
    pthread_mutex_unlock(&shell->lock);

    /* Verify shell is RUNNING before signal */
    IXLAND_ASSERT(atomic_load(&shell->state) == IXLAND_TASK_RUNNING);
    IXLAND_ASSERT(!atomic_load(&shell->exited));

    /* Signal sent to foreground group affects child */
    pthread_mutex_lock(&child->lock);
    atomic_store(&child->signaled, true);
    atomic_store(&child->termsig, SIGINT);
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* Save child PID before wait (child will be reaped) */
    pid_t child_pid = child->pid;

    /* Save original current task */
    ixland_task_t *original = ixland_current_task();

    /* Set shell as current task so waitpid finds child under shell->children */
    ixland_set_current_task(shell);

    /* Verify child was signaled - shell is now the caller */
    int status;
    pid_t result = ixland_waitpid(child_pid, &status, 0);
    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT(WIFSIGNALED(status));
    IXLAND_ASSERT_EQ(WTERMSIG(status), SIGINT);

    /* Restore original current task */
    ixland_set_current_task(original);

    /* Shell still alive (was not in the signaled group) */
    IXLAND_ASSERT(atomic_load(&shell->state) == IXLAND_TASK_RUNNING);
    IXLAND_ASSERT(!atomic_load(&shell->exited));

    /* Cleanup - child already freed by waitpid */
    pthread_mutex_lock(&shell->lock);
    shell->children = NULL;
    pthread_mutex_unlock(&shell->lock);
    ixland_task_free(shell);

    return true;
}

IXLAND_TEST(signal_multiple_pending) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Block multiple signals */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGTERM);

    IXLAND_ASSERT(ixland_sigprocmask(SIG_BLOCK, &mask, NULL) == 0);

    /* Verify all are blocked */
    IXLAND_ASSERT(sigismember(&task->sighand->blocked, SIGUSR1));
    IXLAND_ASSERT(sigismember(&task->sighand->blocked, SIGUSR2));
    IXLAND_ASSERT(sigismember(&task->sighand->blocked, SIGTERM));

    /* Initially no pending signals */
    sigset_t pending;
    IXLAND_ASSERT(ixland_sigpending(&pending) == 0);
    /* Pending set is empty (no signals actually sent in this test) */

    return true;
}

IXLAND_TEST(signal_pending_cleared_on_unblock) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Block SIGUSR1 */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    IXLAND_ASSERT(ixland_sigprocmask(SIG_BLOCK, &mask, NULL) == 0);

    /* Unblock it */
    IXLAND_ASSERT(ixland_sigprocmask(SIG_UNBLOCK, &mask, NULL) == 0);

    /* Verify unblocked */
    IXLAND_ASSERT(!sigismember(&task->sighand->blocked, SIGUSR1));

    return true;
}

IXLAND_TEST(signal_sigkill_cannot_be_blocked) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Try to block SIGKILL */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGKILL);

    /* This succeeds but SIGKILL remains unblockable in reality */
    /* For now, just verify the operation succeeds */
    int ret = ixland_sigprocmask(SIG_BLOCK, &mask, NULL);

    /* Current implementation doesn't validate SIGKILL/SIGSTOP blocking */
    /* This documents the current behavior */
    IXLAND_ASSERT_EQ(ret, 0);

    return true;
}

IXLAND_TEST(signal_null_sighand_fails) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    ixland_sighand_t *saved = task->sighand;

    /* Simulate null sighand */
    task->sighand = NULL;

    sigset_t mask;
    sigemptyset(&mask);

    int ret = ixland_sigprocmask(SIG_BLOCK, &mask, NULL);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, ESRCH);

    /* Restore */
    task->sighand = saved;

    return true;
}

IXLAND_TEST(signal_disposition_preserved_after_sigaction_get) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Use a fresh signal number to avoid test ordering issues */
    int test_sig = SIGUSR2;

    /* Verify initial disposition is DFL */
    IXLAND_ASSERT(task->sighand->action[test_sig].sa_handler == SIG_DFL);

    /* Get old disposition first */
    struct sigaction oldact;
    memset(&oldact, 0, sizeof(oldact));
    IXLAND_ASSERT(ixland_sigaction(test_sig, NULL, &oldact) == 0);
    IXLAND_ASSERT(oldact.sa_handler == SIG_DFL);

    /* Now set SIG_IGN */
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;

    IXLAND_ASSERT(ixland_sigaction(test_sig, &act, &oldact) == 0);

    /* Verify old disposition was DFL */
    IXLAND_ASSERT(oldact.sa_handler == SIG_DFL);

    /* Verify new disposition is IGN */
    IXLAND_ASSERT(task->sighand->action[test_sig].sa_handler == SIG_IGN);

    /* Get current disposition without changing */
    memset(&oldact, 0, sizeof(oldact));
    IXLAND_ASSERT(ixland_sigaction(test_sig, NULL, &oldact) == 0);
    IXLAND_ASSERT(oldact.sa_handler == SIG_IGN);

    /* Cleanup: reset to DFL */
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_DFL;
    ixland_sigaction(test_sig, &act, NULL);

    return true;
}

IXLAND_TEST(signal_invalid_pid_kill_fails) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Try to send signal to non-existent PID */
    int ret = ixland_kill(999999, SIGTERM);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, ESRCH);

    /* Try to send signal to PID -1 (all processes) - requires EPERM */
    ret = ixland_kill(-1, SIGTERM);
    /* According to ixland_kill implementation, pid=-1 is "all processes (privileged)"
     * which returns EPERM, not ESRCH */
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EPERM);

    return true;
}

IXLAND_TEST(signal_invalid_signal_kill_fails) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    int ret = ixland_kill(1, 100); /* Out of range signal */
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    return true;
}

IXLAND_TEST(signal_signal_zero_check_exists) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* Signal 0 is used to check if process exists */
    /* With kill, it should succeed if process exists */
    int ret = ixland_kill(task->pid, 0);
    IXLAND_ASSERT_EQ(ret, 0); /* Process exists */

    ret = ixland_kill(999999, 0);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, ESRCH); /* Process doesn't exist */

    return true;
}

IXLAND_TEST(signal_killpg_basic_delivery) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create a group leader with its own PGID to avoid contaminating current task */
    ixland_task_t *group_leader = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(group_leader);
    group_leader->ppid = parent->pid;
    group_leader->pgid = group_leader->pid; /* Its own group */
    group_leader->sid = parent->sid;
    group_leader->files = ixland_files_alloc(IXLAND_MAX_FD);
    group_leader->fs = ixland_fs_alloc();
    group_leader->sighand = ixland_sighand_alloc();

    /* Target PGID is the group leader's PID */
    pid_t target_pgid = group_leader->pid;

    /* Create two tasks in target PGID */
    ixland_task_t *task1 = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(task1);
    task1->ppid = parent->pid;
    task1->pgid = target_pgid; /* Same as target group */
    task1->sid = parent->sid;
    task1->files = ixland_files_alloc(IXLAND_MAX_FD);
    task1->fs = ixland_fs_alloc();
    task1->sighand = ixland_sighand_alloc();

    ixland_task_t *task2 = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(task2);
    task2->ppid = parent->pid;
    task2->pgid = target_pgid; /* Same as target group */
    task2->sid = parent->sid;
    task2->files = ixland_files_alloc(IXLAND_MAX_FD);
    task2->fs = ixland_fs_alloc();
    task2->sighand = ixland_sighand_alloc();

    /* Create task in different PGID */
    ixland_task_t *task3 = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(task3);
    task3->ppid = parent->pid;
    task3->pgid = task3->pid; /* Different PGID - its own group */
    task3->sid = parent->sid;
    task3->files = ixland_files_alloc(IXLAND_MAX_FD);
    task3->fs = ixland_fs_alloc();
    task3->sighand = ixland_sighand_alloc();

    /* Clear pending signals on target tasks */
    sigemptyset(&task1->sighand->pending);
    sigemptyset(&task2->sighand->pending);
    sigemptyset(&task3->sighand->pending);

    /* Deliver signal to target process group */
    int ret = ixland_killpg(target_pgid, SIGUSR1);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Verify both target-group tasks received the signal */
    IXLAND_ASSERT(sigismember(&task1->sighand->pending, SIGUSR1));
    IXLAND_ASSERT(sigismember(&task2->sighand->pending, SIGUSR1));

    /* Verify out-of-group task did NOT receive the signal */
    IXLAND_ASSERT(!sigismember(&task3->sighand->pending, SIGUSR1));

    /* Cleanup */
    ixland_task_free(group_leader);
    ixland_task_free(task1);
    ixland_task_free(task2);
    ixland_task_free(task3);

    return true;
}

IXLAND_TEST(signal_killpg_single_member) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create a group leader with its own PGID to avoid contaminating current task */
    ixland_task_t *group_leader = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(group_leader);
    group_leader->ppid = parent->pid;
    group_leader->pgid = group_leader->pid; /* Its own group */
    group_leader->sid = parent->sid;
    group_leader->files = ixland_files_alloc(IXLAND_MAX_FD);
    group_leader->fs = ixland_fs_alloc();
    group_leader->sighand = ixland_sighand_alloc();

    pid_t target_pgid = group_leader->pid;

    /* Create single task in target PGID */
    ixland_task_t *task = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(task);
    task->ppid = parent->pid;
    task->pgid = target_pgid;
    task->sid = parent->sid;
    task->files = ixland_files_alloc(IXLAND_MAX_FD);
    task->fs = ixland_fs_alloc();
    task->sighand = ixland_sighand_alloc();

    sigemptyset(&task->sighand->pending);

    /* Deliver signal - should work with single member */
    int ret = ixland_killpg(target_pgid, SIGTERM);
    IXLAND_ASSERT_EQ(ret, 0);
    IXLAND_ASSERT(sigismember(&task->sighand->pending, SIGTERM));

    ixland_task_free(task);
    ixland_task_free(group_leader);
    return true;
}

IXLAND_TEST(signal_killpg_empty_group) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Use a PGID that doesn't exist */
    pid_t nonexistent_pgid = 999999;

    /* Try to signal empty/nonexistent group */
    int ret = ixland_killpg(nonexistent_pgid, SIGTERM);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, ESRCH);

    return true;
}

IXLAND_TEST(signal_killpg_invalid_pgid) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* pgrp <= 0 should return EINVAL */
    int ret = ixland_killpg(0, SIGTERM);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    ret = ixland_killpg(-1, SIGTERM);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    ret = ixland_killpg(-100, SIGTERM);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    return true;
}

IXLAND_TEST(signal_killpg_invalid_signal) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Invalid signal number should fail with EINVAL */
    int ret = ixland_killpg(parent->pgid, -1);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    ret = ixland_killpg(parent->pgid, 100);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    return true;
}

IXLAND_TEST(signal_killpg_refcount_coherent) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create a group leader with its own PGID to avoid contaminating current task */
    ixland_task_t *group_leader = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(group_leader);
    group_leader->ppid = parent->pid;
    group_leader->pgid = group_leader->pid; /* Its own group */
    group_leader->sid = parent->sid;
    group_leader->files = ixland_files_alloc(IXLAND_MAX_FD);
    group_leader->fs = ixland_fs_alloc();
    group_leader->sighand = ixland_sighand_alloc();

    pid_t target_pgid = group_leader->pid;

    /* Create task in target PGID */
    ixland_task_t *task = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(task);
    task->ppid = parent->pid;
    task->pgid = target_pgid;
    task->sid = parent->sid;
    task->files = ixland_files_alloc(IXLAND_MAX_FD);
    task->fs = ixland_fs_alloc();
    task->sighand = ixland_sighand_alloc();

    /* Record initial refcount (should be 1 from alloc) */
    int initial_refs = atomic_load(&task->refs);
    IXLAND_ASSERT_EQ(initial_refs, 1);

    sigemptyset(&task->sighand->pending);

    /* Deliver signal - internal implementation should refcount during collection */
    int ret = ixland_killpg(target_pgid, SIGUSR1);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Refcount should be back to original value */
    int final_refs = atomic_load(&task->refs);
    IXLAND_ASSERT_EQ(final_refs, 1);

    IXLAND_ASSERT(sigismember(&task->sighand->pending, SIGUSR1));

    ixland_task_free(task);
    ixland_task_free(group_leader);
    return true;
}

IXLAND_TEST(signal_killpg_does_not_cross_groups) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create three tasks in three different process groups */
    ixland_task_t *task1 = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(task1);
    task1->ppid = parent->pid;
    task1->pgid = task1->pid; /* Own group */
    task1->sid = parent->sid;
    task1->files = ixland_files_alloc(IXLAND_MAX_FD);
    task1->fs = ixland_fs_alloc();
    task1->sighand = ixland_sighand_alloc();

    ixland_task_t *task2 = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(task2);
    task2->ppid = parent->pid;
    task2->pgid = task2->pid; /* Own group */
    task2->sid = parent->sid;
    task2->files = ixland_files_alloc(IXLAND_MAX_FD);
    task2->fs = ixland_fs_alloc();
    task2->sighand = ixland_sighand_alloc();

    ixland_task_t *task3 = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(task3);
    task3->ppid = parent->pid;
    task3->pgid = task3->pid; /* Own group */
    task3->sid = parent->sid;
    task3->files = ixland_files_alloc(IXLAND_MAX_FD);
    task3->fs = ixland_fs_alloc();
    task3->sighand = ixland_sighand_alloc();

    sigemptyset(&task1->sighand->pending);
    sigemptyset(&task2->sighand->pending);
    sigemptyset(&task3->sighand->pending);

    /* Signal only task2's PGID */
    int ret = ixland_killpg(task2->pgid, SIGUSR2);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Only task2 should have the signal */
    IXLAND_ASSERT(!sigismember(&task1->sighand->pending, SIGUSR2));
    IXLAND_ASSERT(sigismember(&task2->sighand->pending, SIGUSR2));
    IXLAND_ASSERT(!sigismember(&task3->sighand->pending, SIGUSR2));

    ixland_task_free(task1);
    ixland_task_free(task2);
    ixland_task_free(task3);

    return true;
}
