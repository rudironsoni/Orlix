/**
 * @file test_zombie_process.c
 * @brief Comprehensive unit tests for zombie process handling
 *
 * Validates:
 * - Exited processes become zombies (state ZOMBIE)
 * - Zombies retain exit status for parent
 * - waitpid() reaps zombies and returns status
 * - WIFEXITED, WIFSIGNALED macros work correctly
 * - WNOHANG option returns 0 if no zombies
 * - WNOHANG returns PID if zombie exists
 */

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

/**
 * Test: Exit creates zombie process
 * Validates: Exited process becomes zombie (state ZOMBIE)
 */
IXLAND_TEST(exit_creates_zombie) {
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

    /* Verify initial state is RUNNING */
    IXLAND_ASSERT_EQ(atomic_load(&child->state), IXLAND_TASK_RUNNING);
    IXLAND_ASSERT_FALSE(atomic_load(&child->exited));

    /* Simulate child exit - this creates zombie */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 42;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* Verify zombie state */
    IXLAND_ASSERT_EQ(atomic_load(&child->state), IXLAND_TASK_ZOMBIE);
    IXLAND_ASSERT_TRUE(atomic_load(&child->exited));
    IXLAND_ASSERT_EQ(child->exit_status, 42);

    /* Cleanup - reap the zombie */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);
    IXLAND_ASSERT_EQ(result, child_pid);

    return true;
}

/**
 * Test: Zombie retains exit status for parent
 * Validates: Zombie process preserves exit code until reaped
 */
IXLAND_TEST(zombie_retains_exit_status) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
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

    /* Child exits with specific status */
    const int expected_status = 123;
    pthread_mutex_lock(&child->lock);
    child->exit_status = expected_status;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* Verify status is retained */
    IXLAND_ASSERT_EQ(child->exit_status, expected_status);

    /* Parent waits and retrieves status */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT(WIFEXITED(status));
    IXLAND_ASSERT_EQ(WEXITSTATUS(status), expected_status);

    return true;
}

/**
 * Test: waitpid reaps zombie
 * Validates: waitpid() properly reaps zombie and removes from children list
 */
IXLAND_TEST(waitpid_reaps_zombie) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
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

    /* Child becomes zombie */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 0;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* Verify child is in list before reap */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NOT_NULL(parent->children);
    pthread_mutex_unlock(&parent->lock);

    /* Reap the zombie */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child_pid);

    /* Verify child is removed from list after reap */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NULL(parent->children);
    pthread_mutex_unlock(&parent->lock);

    return true;
}

/**
 * Test: WIFEXITED macro works
 * Validates: WIFEXITED returns true for normally exited process
 */
IXLAND_TEST(wifexited_works) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
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

    /* Normal exit (not signaled) */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 42;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT_TRUE(WIFEXITED(status));
    IXLAND_ASSERT_FALSE(WIFSIGNALED(status));

    return true;
}

/**
 * Test: WEXITSTATUS extracts exit code
 * Validates: WEXITSTATUS returns correct exit code (0-255)
 */
IXLAND_TEST(wexitstatus_extracts_exit_code) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Test exit code 42 */
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

    pthread_mutex_lock(&child->lock);
    child->exit_status = 42;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT_TRUE(WIFEXITED(status));
    IXLAND_ASSERT_EQ(WEXITSTATUS(status), 42);

    return true;
}

/**
 * Test: WEXITSTATUS extracts zero exit code
 * Validates: WEXITSTATUS works for exit code 0
 */
IXLAND_TEST(wexitstatus_extracts_zero) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

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

    pthread_mutex_lock(&child->lock);
    child->exit_status = 0;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT_TRUE(WIFEXITED(status));
    IXLAND_ASSERT_EQ(WEXITSTATUS(status), 0);

    return true;
}

/**
 * Test: WEXITSTATUS extracts max exit code
 * Validates: WEXITSTATUS works for exit code 255
 */
IXLAND_TEST(wexitstatus_extracts_max) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

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

    pthread_mutex_lock(&child->lock);
    child->exit_status = 255;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT_TRUE(WIFEXITED(status));
    IXLAND_ASSERT_EQ(WEXITSTATUS(status), 255);

    return true;
}

/**
 * Test: Signal termination status encoding - SIGHUP
 * Validates: WTERMSIG extracts correct signal number
 */
IXLAND_TEST(wtermsig_sighup) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

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

    pthread_mutex_lock(&child->lock);
    child->exit_status = 0;
    atomic_store(&child->signaled, true);
    atomic_store(&child->termsig, SIGHUP);
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT_TRUE(WIFSIGNALED(status));
    IXLAND_ASSERT_EQ(WTERMSIG(status), SIGHUP);

    return true;
}

/**
 * Test: Signal termination status encoding - SIGTERM
 * Validates: WTERMSIG extracts correct signal number
 */
IXLAND_TEST(wtermsig_sigterm) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

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

    pthread_mutex_lock(&child->lock);
    child->exit_status = 0;
    atomic_store(&child->signaled, true);
    atomic_store(&child->termsig, SIGTERM);
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT_TRUE(WIFSIGNALED(status));
    IXLAND_ASSERT_EQ(WTERMSIG(status), SIGTERM);

    return true;
}

/**
 * Test: WIFSIGNALED macro works
 * Validates: WIFSIGNALED returns true for signal-terminated process
 */
IXLAND_TEST(wifsignaled_works) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
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

    /* Signal-terminated exit */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 0;
    atomic_store(&child->signaled, true);
    atomic_store(&child->termsig, SIGTERM);
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(child_pid, &status, 0);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT_TRUE(WIFSIGNALED(status));
    IXLAND_ASSERT_FALSE(WIFEXITED(status));
    IXLAND_ASSERT_EQ(WTERMSIG(status), SIGTERM);

    return true;
}

/**
 * Test: WNOHANG returns 0 if no zombies
 * Validates: waitpid with WNOHANG returns 0 when no exited children
 */
IXLAND_TEST(wnohang_returns_zero_if_no_zombies) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create child that has NOT exited (not a zombie) */
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

    /* WNOHANG with live child should return 0 */
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

/**
 * Test: WNOHANG returns PID if zombie exists
 * Validates: waitpid with WNOHANG returns child PID when zombie exists
 */
IXLAND_TEST(wnohang_returns_pid_if_zombie_exists) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
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

    /* Child is a zombie */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 99;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* WNOHANG with zombie should return PID */
    int status;
    pid_t child_pid = child->pid;
    pid_t result = ixland_waitpid(-1, &status, WNOHANG);

    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT_TRUE(WIFEXITED(status));
    IXLAND_ASSERT_EQ(WEXITSTATUS(status), 99);

    return true;
}

/**
 * Test: WNOHANG does not block
 * Validates: waitpid with WNOHANG returns immediately
 */
IXLAND_TEST(wnohang_does_not_block) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create live child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    child->ppid = parent->pid;
    child->parent = parent;
    atomic_store(&child->exited, false);

    pthread_mutex_lock(&parent->lock);
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* WNOHANG should return immediately with 0 */
    int status;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    pid_t result = ixland_waitpid(-1, &status, WNOHANG);

    clock_gettime(CLOCK_MONOTONIC, &end);

    /* Should return 0, not block */
    IXLAND_ASSERT_EQ(result, 0);

    /* Should complete quickly (less than 100ms) */
    long elapsed_ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;
    IXLAND_ASSERT(elapsed_ms < 100);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);
    ixland_task_free(child);

    return true;
}

/**
 * Test: Multiple zombies can be reaped in any order
 * Validates: waitpid can reap specific zombies by PID
 */
IXLAND_TEST(multiple_zombies_reap_any_order) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create multiple children */
    ixland_task_t *child1 = ixland_task_alloc();
    ixland_task_t *child2 = ixland_task_alloc();
    ixland_task_t *child3 = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child1);
    IXLAND_ASSERT_NOT_NULL(child2);
    IXLAND_ASSERT_NOT_NULL(child3);

    /* Setup all children */
    ixland_task_t *children[3] = {child1, child2, child3};
    pid_t pids[3];

    for (int i = 0; i < 3; i++) {
        children[i]->files = ixland_files_alloc(IXLAND_MAX_FD);
        children[i]->fs = ixland_fs_alloc();
        children[i]->sighand = ixland_sighand_alloc();
        children[i]->ppid = parent->pid;
        children[i]->parent = parent;

        pthread_mutex_lock(&parent->lock);
        children[i]->next_sibling = parent->children;
        parent->children = children[i];
        pthread_mutex_unlock(&parent->lock);

        /* Make zombie with unique exit code */
        pthread_mutex_lock(&children[i]->lock);
        children[i]->exit_status = 100 + i;
        atomic_store(&children[i]->exited, true);
        atomic_store(&children[i]->state, IXLAND_TASK_ZOMBIE);
        pthread_mutex_unlock(&children[i]->lock);

        pids[i] = children[i]->pid;
    }

    /* Reap in reverse order */
    for (int i = 2; i >= 0; i--) {
        int status;
        pid_t result = ixland_waitpid(pids[i], &status, 0);

        IXLAND_ASSERT_EQ(result, pids[i]);
        IXLAND_ASSERT_TRUE(WIFEXITED(status));
        IXLAND_ASSERT_EQ(WEXITSTATUS(status), 100 + i);
    }

    /* All children should be reaped */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NULL(parent->children);
    pthread_mutex_unlock(&parent->lock);

    return true;
}

/**
 * Test: Zombie remains in task table until reaped
 * Validates: Zombie task structure persists until waitpid called
 */
IXLAND_TEST(zombie_remains_until_reaped) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
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

    pid_t child_pid = child->pid;

    /* Child becomes zombie */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 55;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* Verify zombie can be found in task table */
    ixland_task_t *found = ixland_task_lookup(child_pid);
    IXLAND_ASSERT_NOT_NULL(found);
    IXLAND_ASSERT_EQ(found->pid, child_pid);
    IXLAND_ASSERT_TRUE(atomic_load(&found->exited));
    IXLAND_ASSERT_EQ(atomic_load(&found->state), IXLAND_TASK_ZOMBIE);
    IXLAND_ASSERT_EQ(found->exit_status, 55);

    /* Release lookup reference */
    ixland_task_free(found);

    /* Reap the zombie */
    int status;
    pid_t result = ixland_waitpid(child_pid, &status, 0);
    IXLAND_ASSERT_EQ(result, child_pid);

    return true;
}

/**
 * Test: waitpid with WNOHANG returns 0 for live children
 * Validates: Proper behavior when children exist but haven't exited
 */
IXLAND_TEST(waitpid_wnohang_live_children) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create a live child (not exited) */
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

    /* WNOHANG with live children returns 0 (no zombies ready) */
    int status;
    pid_t result = ixland_waitpid(-1, &status, WNOHANG);

    /* When there are live children but no zombies, WNOHANG returns 0 */
    IXLAND_ASSERT_EQ(result, 0);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);
    ixland_task_free(child);

    return true;
}

/**
 * Test: Zombie not reaped until waitpid called
 * Validates: Zombie persists in children list until explicitly reaped
 */
IXLAND_TEST(zombie_persists_until_waitpid) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
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

    /* Child becomes zombie */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 77;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* Save PID before reap */
    pid_t child_pid = child->pid;

    /* Verify child is still in list (multiple checks) */
    for (int i = 0; i < 3; i++) {
        pthread_mutex_lock(&parent->lock);
        IXLAND_ASSERT_NOT_NULL(parent->children);
        pthread_mutex_unlock(&parent->lock);
    }

    /* Now reap */
    int status;
    pid_t result = ixland_waitpid(child_pid, &status, 0);
    IXLAND_ASSERT_EQ(result, child_pid);

    /* Verify child is gone */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NULL(parent->children);
    pthread_mutex_unlock(&parent->lock);

    return true;
}
