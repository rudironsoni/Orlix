#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "../../fs/fdtable.h"
#include "../../fs/vfs.h"
#include "../../kernel/signal/ixland_signal.h"
#include "../../kernel/task/task.h"
#include "../harness/ixland_test.h"

/* External declarations for syscalls tested */
extern pid_t ixland_waitpid(pid_t pid, int *wstatus, int options);
extern void ixland_exit(int status);

/* Helper thread that exits with given status */
typedef struct {
    int exit_status;
    ixland_task_t *parent;
} child_thread_args_t;

static void *child_thread_runner(void *arg) {
    child_thread_args_t *args = (child_thread_args_t *)arg;

    /* Get current task */
    ixland_task_t *child = ixland_current_task();
    if (child) {
        child->ppid = args->parent->pid;

        /* Link child to parent */
        pthread_mutex_lock(&args->parent->lock);
        child->parent = args->parent;
        child->next_sibling = args->parent->children;
        args->parent->children = child;
        pthread_mutex_unlock(&args->parent->lock);
    }

    /* Exit with specified status */
    ixland_exit(args->exit_status);
    return NULL;
}

static void *child_thread_runner_signaled(void *arg) {
    child_thread_args_t *args = (child_thread_args_t *)arg;

    /* Get current task */
    ixland_task_t *child = ixland_current_task();
    if (child) {
        child->ppid = args->parent->pid;

        /* Link child to parent */
        pthread_mutex_lock(&args->parent->lock);
        child->parent = args->parent;
        child->next_sibling = args->parent->children;
        args->parent->children = child;
        pthread_mutex_unlock(&args->parent->lock);

        /* Mark as signaled before exit */
        atomic_store(&child->signaled, true);
        atomic_store(&child->termsig, SIGTERM);
    }

    /* Exit with specified status */
    ixland_exit(args->exit_status);
    return NULL;
}

IXLAND_TEST(wait_parent_observes_child_exit) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create child task structure */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    /* Allocate resources for child */
    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();
    IXLAND_ASSERT_NOT_NULL(child->files);
    IXLAND_ASSERT_NOT_NULL(child->fs);
    IXLAND_ASSERT_NOT_NULL(child->sighand);

    /* Set up parent-child relationship */
    child->ppid = parent->pid;
    child->parent = parent;

    pthread_mutex_lock(&parent->lock);
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Simulate child exit with status 42 */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 42;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* Parent waits for child - save PID before reap */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT(WIFEXITED(status));
    IXLAND_ASSERT_EQ(WEXITSTATUS(status), 42);

    /* Child should be reaped */
    IXLAND_ASSERT_NULL(parent->children);

    return true;
}

IXLAND_TEST(wait_no_children_returns_echild) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Ensure no children */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);

    /* Try to wait with no children */
    int status;
    pid_t result = ixland_waitpid(-1, &status, 0);

    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ECHILD);

    return true;
}

IXLAND_TEST(wait_specific_pid_finds_correct_child) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create two children */
    ixland_task_t *child1 = ixland_task_alloc();
    ixland_task_t *child2 = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child1);
    IXLAND_ASSERT_NOT_NULL(child2);

    child1->files = ixland_files_alloc(IXLAND_MAX_FD);
    child1->fs = ixland_fs_alloc();
    child1->sighand = ixland_sighand_alloc();
    child2->files = ixland_files_alloc(IXLAND_MAX_FD);
    child2->fs = ixland_fs_alloc();
    child2->sighand = ixland_sighand_alloc();

    child1->ppid = parent->pid;
    child1->parent = parent;
    child2->ppid = parent->pid;
    child2->parent = parent;

    /* Link both children */
    pthread_mutex_lock(&parent->lock);
    child2->next_sibling = child1;
    child1->next_sibling = NULL;
    parent->children = child2;
    pthread_mutex_unlock(&parent->lock);

    /* Exit child2 only */
    pthread_mutex_lock(&child2->lock);
    child2->exit_status = 99;
    atomic_store(&child2->exited, true);
    atomic_store(&child2->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child2->lock);

    /* Wait specifically for child2 */
    int status;
    pid_t child2_pid = child2->pid;
    pid_t result = ixland_waitpid(child2_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child2_pid);
    IXLAND_ASSERT(WIFEXITED(status));
    IXLAND_ASSERT_EQ(WEXITSTATUS(status), 99);

    /* child1 should still be in the list */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NOT_NULL(parent->children);
    IXLAND_ASSERT_EQ(parent->children->pid, child1->pid);
    pthread_mutex_unlock(&parent->lock);

    /* Cleanup child1 */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);
    ixland_task_free(child1);

    return true;
}

IXLAND_TEST(wait_any_pid_minus_one_finds_any_child) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Simpler test: use current task and verify waitpid(-1) works */
    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create one child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    child->ppid = parent->pid;
    child->parent = parent;

    pthread_mutex_lock(&parent->lock);
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Exit the child */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 99;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* Wait for any child (pid = -1) */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(-1, &status, 0);

    /* Should return the child */
    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT(WIFEXITED(status));
    IXLAND_ASSERT_EQ(WEXITSTATUS(status), 99);

    /* Child should be reaped */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NULL(parent->children);
    pthread_mutex_unlock(&parent->lock);

    return true;
}

IXLAND_TEST(wait_wrong_pid_returns_error) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create a child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    child->ppid = parent->pid;
    child->parent = parent;

    pthread_mutex_lock(&parent->lock);
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Wait for non-existent child PID */
    int status;
    pid_t result = ixland_waitpid(999999, &status, WNOHANG);

    /* Should return 0 (no matching exited child) for WNOHANG */
    IXLAND_ASSERT_EQ(result, 0);

    /* Cleanup - remove the child from list */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);

    ixland_task_free(child);

    return true;
}

IXLAND_TEST(wait_second_reap_fails) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    child->ppid = parent->pid;
    child->parent = parent;

    pthread_mutex_lock(&parent->lock);
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Child exits */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 1;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* First wait succeeds - save PID before reap */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);
    IXLAND_ASSERT_EQ(result, child_pid);

    /* Second wait should fail (no children) */
    errno = 0;
    result = ixland_waitpid(child_pid, &status, 0);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ECHILD);

    return true;
}

IXLAND_TEST(wait_signaled_child) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create child marked as signaled */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    child->ppid = parent->pid;
    child->parent = parent;

    pthread_mutex_lock(&parent->lock);
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Child exits via signal */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 0;
    atomic_store(&child->signaled, true);
    atomic_store(&child->termsig, SIGTERM);
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* Parent waits - save PID before reap */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT(WIFSIGNALED(status));
    IXLAND_ASSERT_EQ(WTERMSIG(status), SIGTERM);

    return true;
}

IXLAND_TEST(wnohang_live_child_returns_zero) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create child that has NOT exited */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    child->ppid = parent->pid;
    child->parent = parent;
    atomic_store(&child->exited, false); /* Not exited */

    pthread_mutex_lock(&parent->lock);
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* WNOHANG with live child returns 0 */
    int status;
    pid_t result = ixland_waitpid(-1, &status, WNOHANG);

    IXLAND_ASSERT_EQ(result, 0);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);
    ixland_task_free(child);

    return true;
}

IXLAND_TEST(wnohang_exited_child_returns_pid) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create exited child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    child->ppid = parent->pid;
    child->parent = parent;

    pthread_mutex_lock(&parent->lock);
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Child has exited */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 99;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* WNOHANG with exited child returns PID - save before reap */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(-1, &status, WNOHANG);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT(WIFEXITED(status));
    IXLAND_ASSERT_EQ(WEXITSTATUS(status), 99);

    return true;
}

IXLAND_TEST(wait_child_list_unlinked_after_reap) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create two children */
    ixland_task_t *child1 = ixland_task_alloc();
    ixland_task_t *child2 = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child1);
    IXLAND_ASSERT_NOT_NULL(child2);

    child1->files = ixland_files_alloc(IXLAND_MAX_FD);
    child1->fs = ixland_fs_alloc();
    child1->sighand = ixland_sighand_alloc();
    child2->files = ixland_files_alloc(IXLAND_MAX_FD);
    child2->fs = ixland_fs_alloc();
    child2->sighand = ixland_sighand_alloc();

    child1->ppid = parent->pid;
    child1->parent = parent;
    child2->ppid = parent->pid;
    child2->parent = parent;

    /* Link both children */
    pthread_mutex_lock(&parent->lock);
    child1->next_sibling = NULL;
    child2->next_sibling = child1;
    parent->children = child2;
    pthread_mutex_unlock(&parent->lock);

    /* Exit child2 */
    pthread_mutex_lock(&child2->lock);
    child2->exit_status = 1;
    atomic_store(&child2->exited, true);
    atomic_store(&child2->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child2->lock);

    /* Wait for child2 - should only reap that one */
    int status;
    pid_t child2_pid = child2->pid;
    pid_t result = ixland_waitpid(child2_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child2_pid);

    /* Verify child1 still in list */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_EQ(parent->children, child1);
    IXLAND_ASSERT_NULL(child1->next_sibling);
    pthread_mutex_unlock(&parent->lock);

    /* Cleanup child1 */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);
    ixland_task_free(child1);

    return true;
}

IXLAND_TEST(wait_refcount_coherent) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    int initial_refs = atomic_load(&child->refs);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    child->ppid = parent->pid;
    child->parent = parent;

    pthread_mutex_lock(&parent->lock);
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Child exits */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 0;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* Wait should free the child - save PID before reap */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child_pid);
    /* Child is freed, don't access it */

    return true;
}

IXLAND_TEST(wait_multiple_children_sequence) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create 3 children */
    ixland_task_t *children[3];
    pid_t pids[3];

    for (int i = 0; i < 3; i++) {
        children[i] = ixland_task_alloc();
        IXLAND_ASSERT_NOT_NULL(children[i]);

        children[i]->files = ixland_files_alloc(IXLAND_MAX_FD);
        children[i]->fs = ixland_fs_alloc();
        children[i]->sighand = ixland_sighand_alloc();

        children[i]->ppid = parent->pid;
        children[i]->parent = parent;
        pids[i] = children[i]->pid;

        pthread_mutex_lock(&parent->lock);
        children[i]->next_sibling = parent->children;
        parent->children = children[i];
        pthread_mutex_unlock(&parent->lock);
    }

    /* Exit them in order 0, 1, 2 */
    for (int i = 0; i < 3; i++) {
        pthread_mutex_lock(&children[i]->lock);
        children[i]->exit_status = 100 + i;
        atomic_store(&children[i]->exited, true);
        atomic_store(&children[i]->state, IXLAND_TASK_ZOMBIE);
        pthread_mutex_unlock(&children[i]->lock);
    }

    /* Wait for them in different order: 2, 0, 1 */
    int order[] = {2, 0, 1};
    for (int i = 0; i < 3; i++) {
        int idx = order[i];
        int status;
        pid_t result = ixland_waitpid(pids[idx], &status, 0);

        IXLAND_ASSERT_EQ(result, pids[idx]);
        IXLAND_ASSERT(WIFEXITED(status));
        IXLAND_ASSERT_EQ(WEXITSTATUS(status), 100 + idx);
    }

    /* All children should be reaped */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NULL(parent->children);
    pthread_mutex_unlock(&parent->lock);

    return true;
}

IXLAND_TEST(waitpid_wuntraced_reports_stopped_child) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();
    IXLAND_ASSERT_NOT_NULL(child->files);
    IXLAND_ASSERT_NOT_NULL(child->fs);
    IXLAND_ASSERT_NOT_NULL(child->sighand);

    /* Set up parent-child relationship */
    child->ppid = parent->pid;
    child->parent = parent;

    pthread_mutex_lock(&parent->lock);
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Child is stopped (not exited) */
    pthread_mutex_lock(&child->lock);
    atomic_store(&child->stopped, true);
    atomic_store(&child->stopsig, SIGSTOP);
    atomic_store(&child->state, IXLAND_TASK_STOPPED);
    pthread_mutex_unlock(&child->lock);

    /* Without WUNTRACED, should return 0 (WNOHANG) or block (no WNOHANG) */
    /* With WNOHANG, returns 0 since child is not exited */
    int status_no_option;
    pid_t result_no_option = ixland_waitpid(child->pid, &status_no_option, WNOHANG);
    IXLAND_ASSERT_EQ(result_no_option, 0); /* Stopped child not reported without WUNTRACED */

    /* With WUNTRACED, should report stopped child */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child->pid, &status, WUNTRACED);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT_TRUE(WIFSTOPPED(status));
    IXLAND_ASSERT_EQ(WSTOPSIG(status), SIGSTOP);

    /* Child should NOT be reaped (still in children list) */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NOT_NULL(parent->children);
    IXLAND_ASSERT_EQ(parent->children->pid, child_pid);
    pthread_mutex_unlock(&parent->lock);

    /* Cleanup - mark as exited and reap */
    pthread_mutex_lock(&child->lock);
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    ixland_waitpid(child_pid, &status, 0);

    return true;
}

IXLAND_TEST(waitpid_wcontinued_reports_continued_child) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();
    IXLAND_ASSERT_NOT_NULL(child->files);
    IXLAND_ASSERT_NOT_NULL(child->fs);
    IXLAND_ASSERT_NOT_NULL(child->sighand);

    /* Set up parent-child relationship */
    child->ppid = parent->pid;
    child->parent = parent;

    pthread_mutex_lock(&parent->lock);
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Child continued (was stopped, now continued) */
    pthread_mutex_lock(&child->lock);
    atomic_store(&child->continued, true);
    atomic_store(&child->state, IXLAND_TASK_RUNNING);
    pthread_mutex_unlock(&child->lock);

    /* Without WCONTINUED, should return 0 (WNOHANG) */
    int status_no_option;
    pid_t result_no_option = ixland_waitpid(child->pid, &status_no_option, WNOHANG);
    IXLAND_ASSERT_EQ(result_no_option, 0); /* Continued child not reported without WCONTINUED */

    /* With WCONTINUED, should report continued child */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child->pid, &status, WCONTINUED);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT_TRUE(WIFCONTINUED(status));

    /* Child should NOT be reaped (still running) */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NOT_NULL(parent->children);
    IXLAND_ASSERT_EQ(parent->children->pid, child_pid);
    pthread_mutex_unlock(&parent->lock);

    /* Cleanup - mark as exited and reap */
    pthread_mutex_lock(&child->lock);
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    ixland_waitpid(child_pid, &status, 0);

    return true;
}
