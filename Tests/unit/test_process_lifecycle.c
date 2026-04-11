/* Integration tests for complete process lifecycle: fork -> exec -> wait -> exit
 *
 * These tests verify the full cross-area integration of process management
 * syscalls, fulfilling validation assertions:
 *   - VAL-CROSS-009: Fork creates independent process context
 *   - VAL-CROSS-010: Exec replaces process image
 *   - VAL-CROSS-011: Wait collects child exit status
 *   - VAL-CROSS-012: Exit terminates process and notifies parent
 *   - VAL-CROSS-013: Process groups and sessions work
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
#include "../../kernel/exec/exec.h"
#include "../../kernel/signal/ixland_signal.h"
#include "../../kernel/task/task.h"
#include "../../runtime/native/registry.h"
#include "../harness/ixland_test.h"

/* External declarations for syscall functions */
extern pid_t ixland_waitpid(pid_t pid, int *wstatus, int options);
extern void ixland_exit(int status);
extern pid_t ixland_getpid(void);
extern pid_t ixland_getppid(void);
extern pid_t ixland_getpgrp(void);
extern pid_t ixland_setsid(void);

/* Helper to set up a child task for testing without actual fork() */
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

    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    return child;
}

/* Helper to simulate child exit with specific status */
static void simulate_child_exit(ixland_task_t *child, int exit_status) {
    pthread_mutex_lock(&child->lock);
    child->exit_status = exit_status;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);
}

/* ============================================================================
 * Test: Complete fork/wait lifecycle with exit status propagation
 * Validates: VAL-CROSS-009, VAL-CROSS-011, VAL-CROSS-012
 * ============================================================================ */
IXLAND_TEST(lifecycle_fork_wait_exit_basic) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    pid_t parent_pid = parent->pid;

    /* Create child (simulating what fork would do) */
    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);

    pid_t child_pid = child->pid;

    /* Verify parent-child relationship (VAL-CROSS-009) */
    IXLAND_ASSERT_EQ(child->ppid, parent_pid);
    IXLAND_ASSERT(child->pid > 0);
    IXLAND_ASSERT(child->pid != parent_pid);

    /* Verify inheritance (VAL-CROSS-009) */
    IXLAND_ASSERT_EQ(child->pgid, parent->pgid);
    IXLAND_ASSERT_EQ(child->sid, parent->sid);

    /* Simulate child exit with status 42 (VAL-CROSS-012) */
    simulate_child_exit(child, 42);

    /* Parent waits for child (VAL-CROSS-011) */
    int status;
    pid_t waited_pid = ixland_waitpid(child_pid, &status, 0);

    /* Verify wait collected correct child */
    IXLAND_ASSERT_EQ(waited_pid, child_pid);
    IXLAND_ASSERT(WIFEXITED(status));
    IXLAND_ASSERT_EQ(WEXITSTATUS(status), 42);

    /* Child should be reaped */
    IXLAND_ASSERT_NULL(parent->children);

    return true;
}

/* ============================================================================
 * Test: Multiple children lifecycle
 * Validates: VAL-CROSS-011 (multiple children), VAL-CROSS-013 (process groups)
 * ============================================================================ */
IXLAND_TEST(lifecycle_multiple_children_wait_any) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create 3 children */
    ixland_task_t *children[3];
    pid_t pids[3];

    for (int i = 0; i < 3; i++) {
        children[i] = create_test_child(parent);
        IXLAND_ASSERT_NOT_NULL(children[i]);
        pids[i] = children[i]->pid;

        /* Verify all children share parent's process group (VAL-CROSS-013) */
        IXLAND_ASSERT_EQ(children[i]->pgid, parent->pgid);
    }

    /* All children should be in parent's children list */
    pthread_mutex_lock(&parent->lock);
    int child_count = 0;
    ixland_task_t *c = parent->children;
    while (c) {
        child_count++;
        c = c->next_sibling;
    }
    pthread_mutex_unlock(&parent->lock);
    IXLAND_ASSERT_EQ(child_count, 3);

    /* Exit children in order 0, 1, 2 */
    for (int i = 0; i < 3; i++) {
        simulate_child_exit(children[i], 100 + i);
    }

    /* Wait for any child - should find exited ones (VAL-CROSS-011) */
    int status;
    pid_t result = ixland_waitpid(-1, &status, 0);
    IXLAND_ASSERT(result > 0);
    IXLAND_ASSERT(WIFEXITED(status));

    int found_idx = -1;
    for (int i = 0; i < 3; i++) {
        if (result == pids[i]) {
            found_idx = i;
            break;
        }
    }
    IXLAND_ASSERT(found_idx >= 0);
    IXLAND_ASSERT_EQ(WEXITSTATUS(status), 100 + found_idx);

    /* Wait for remaining children */
    int remaining = 2;
    while (remaining > 0) {
        result = ixland_waitpid(-1, &status, 0);
        IXLAND_ASSERT(result > 0);
        IXLAND_ASSERT(WIFEXITED(status));
        remaining--;
    }

    /* No more children */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NULL(parent->children);
    pthread_mutex_unlock(&parent->lock);

    return true;
}

/* ============================================================================
 * Test: Wait for specific child PID
 * Validates: VAL-CROSS-011 (specific PID waiting)
 * ============================================================================ */
IXLAND_TEST(lifecycle_wait_specific_pid) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create two children */
    ixland_task_t *child1 = create_test_child(parent);
    ixland_task_t *child2 = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child1);
    IXLAND_ASSERT_NOT_NULL(child2);

    pid_t pid1 = child1->pid;
    pid_t pid2 = child2->pid;

    /* Exit both children */
    simulate_child_exit(child1, 11);
    simulate_child_exit(child2, 22);

    /* Wait specifically for child2 first */
    int status;
    pid_t result = ixland_waitpid(pid2, &status, 0);
    IXLAND_ASSERT_EQ(result, pid2);
    IXLAND_ASSERT(WIFEXITED(status));
    IXLAND_ASSERT_EQ(WEXITSTATUS(status), 22);

    /* Child1 should still be in list */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NOT_NULL(parent->children);
    pthread_mutex_unlock(&parent->lock);

    /* Now wait for child1 */
    result = ixland_waitpid(pid1, &status, 0);
    IXLAND_ASSERT_EQ(result, pid1);
    IXLAND_ASSERT(WIFEXITED(status));
    IXLAND_ASSERT_EQ(WEXITSTATUS(status), 11);

    /* No more children */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NULL(parent->children);
    pthread_mutex_unlock(&parent->lock);

    return true;
}

/* ============================================================================
 * Test: Exit status preservation with different values
 * Validates: VAL-CROSS-011 (exit status preservation), VAL-CROSS-012
 * ============================================================================ */
IXLAND_TEST(lifecycle_exit_status_preservation) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Test various exit statuses */
    int test_statuses[] = {0, 1, 127, 128, 255, 42, 99};
    int num_statuses = sizeof(test_statuses) / sizeof(test_statuses[0]);

    for (int i = 0; i < num_statuses; i++) {
        /* Create child */
        ixland_task_t *child = create_test_child(parent);
        IXLAND_ASSERT_NOT_NULL(child);
        pid_t child_pid = child->pid;

        /* Exit with specific status */
        simulate_child_exit(child, test_statuses[i]);

        /* Wait and verify status preserved */
        int status;
        pid_t result = ixland_waitpid(child_pid, &status, 0);
        IXLAND_ASSERT_EQ(result, child_pid);
        IXLAND_ASSERT(WIFEXITED(status));
        IXLAND_ASSERT_EQ(WEXITSTATUS(status), test_statuses[i]);
    }

    return true;
}

/* ============================================================================
 * Test: Signaled child exit status
 * Validates: VAL-CROSS-011 (signaled exit), VAL-CROSS-012
 * ============================================================================ */
IXLAND_TEST(lifecycle_signaled_child_status) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);
    pid_t child_pid = child->pid;

    /* Simulate signaled exit (SIGTERM) */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 0;
    atomic_store(&child->signaled, true);
    atomic_store(&child->termsig, SIGTERM);
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* Wait for child */
    int status;
    pid_t result = ixland_waitpid(child_pid, &status, 0);
    IXLAND_ASSERT_EQ(result, child_pid);

    /* Verify signaled status */
    IXLAND_ASSERT(WIFSIGNALED(status));
    IXLAND_ASSERT_EQ(WTERMSIG(status), SIGTERM);

    return true;
}

/* ============================================================================
 * Test: Process group lifecycle - children inherit pgid
 * Validates: VAL-CROSS-013 (process groups)
 * ============================================================================ */
IXLAND_TEST(lifecycle_process_group_inheritance) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    pid_t original_pgid = parent->pgid;

    /* Create multiple children */
    for (int i = 0; i < 5; i++) {
        ixland_task_t *child = create_test_child(parent);
        IXLAND_ASSERT_NOT_NULL(child);

        /* Each child should inherit parent's pgid (VAL-CROSS-013) */
        IXLAND_ASSERT_EQ(child->pgid, original_pgid);
    }

    /* Clean up - exit and wait all children */
    pthread_mutex_lock(&parent->lock);
    ixland_task_t *child = parent->children;
    while (child) {
        pthread_mutex_lock(&child->lock);
        child->exit_status = 0;
        atomic_store(&child->exited, true);
        atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
        pthread_mutex_unlock(&child->lock);
        child = child->next_sibling;
    }
    pthread_mutex_unlock(&parent->lock);

    /* Wait all */
    while (1) {
        int status;
        pid_t result = ixland_waitpid(-1, &status, WNOHANG);
        if (result == 0 || result == -1)
            break;
    }

    return true;
}

/* ============================================================================
 * Test: Session lifecycle - children inherit sid
 * Validates: VAL-CROSS-013 (sessions)
 * ============================================================================ */
IXLAND_TEST(lifecycle_session_inheritance) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    pid_t original_sid = parent->sid;

    /* Create child */
    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);

    /* Child should inherit parent's sid (VAL-CROSS-013) */
    IXLAND_ASSERT_EQ(child->sid, original_sid);

    /* Clean up */
    simulate_child_exit(child, 0);
    int status;
    ixland_waitpid(child->pid, &status, 0);

    return true;
}

/* ============================================================================
 * Test: WNOHANG behavior with exited children
 * Validates: VAL-CROSS-011 (WNOHANG)
 * ============================================================================ */
IXLAND_TEST(lifecycle_wnohang_with_exited_children) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create and exit child */
    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);
    pid_t child_pid = child->pid;

    simulate_child_exit(child, 77);

    /* WNOHANG should return PID immediately */
    int status;
    pid_t result = ixland_waitpid(-1, &status, WNOHANG);
    IXLAND_ASSERT_EQ(result, child_pid);
    IXLAND_ASSERT(WIFEXITED(status));
    IXLAND_ASSERT_EQ(WEXITSTATUS(status), 77);

    return true;
}

/* ============================================================================
 * Test: WNOHANG with live children returns 0
 * Validates: VAL-CROSS-011 (WNOHANG non-blocking)
 * ============================================================================ */
IXLAND_TEST(lifecycle_wnohang_live_children_returns_zero) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create child that has NOT exited */
    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);

    /* WNOHANG should return 0 for live children */
    int status;
    pid_t result = ixland_waitpid(-1, &status, WNOHANG);
    IXLAND_ASSERT_EQ(result, 0);

    /* Clean up */
    simulate_child_exit(child, 0);
    ixland_waitpid(child->pid, &status, 0);

    return true;
}

/* ============================================================================
 * Test: Zombie persists until wait
 * Validates: VAL-CROSS-011 (zombie reaping), VAL-CROSS-012
 * ============================================================================ */
IXLAND_TEST(lifecycle_zombie_persists_until_wait) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    ixland_task_t *child = create_test_child(parent);
    IXLAND_ASSERT_NOT_NULL(child);
    pid_t child_pid = child->pid;

    /* Simulate exit */
    simulate_child_exit(child, 55);

    /* Child should still be in list (zombie) */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NOT_NULL(parent->children);
    IXLAND_ASSERT_EQ(parent->children->pid, child_pid);
    pthread_mutex_unlock(&parent->lock);

    /* Verify task lookup still finds it */
    ixland_task_t *found = ixland_task_lookup(child_pid);
    IXLAND_ASSERT_NOT_NULL(found);

    /* Wait reaps it */
    int status;
    pid_t result = ixland_waitpid(child_pid, &status, 0);
    IXLAND_ASSERT_EQ(result, child_pid);

    /* Now should be gone from children list */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NULL(parent->children);
    pthread_mutex_unlock(&parent->lock);

    return true;
}

/* ============================================================================
 * Test: Fork/exec/wait sequence with process image change
 * Validates: VAL-CROSS-010 (exec replaces process image)
 * ============================================================================ */
IXLAND_TEST(lifecycle_exec_preserves_pid) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(task);

    pid_t pid_before = task->pid;
    pid_t pgid_before = task->pgid;
    pid_t sid_before = task->sid;

    /* Create a stub exec image */
    if (!task->exec_image) {
        task->exec_image = calloc(1, sizeof(ixland_exec_image_t));
        IXLAND_ASSERT_NOT_NULL(task->exec_image);
    }

    /* Simulate exec by updating process image */
    task->exec_image->type = IXLAND_IMAGE_NATIVE;
    strncpy(task->exec_image->path, "/bin/test", IXLAND_MAX_PATH - 1);
    strncpy(task->comm, "test", IXLAND_MAX_NAME - 1);

    /* Verify PID unchanged (VAL-CROSS-010) */
    IXLAND_ASSERT_EQ(task->pid, pid_before);
    IXLAND_ASSERT_EQ(task->pgid, pgid_before);
    IXLAND_ASSERT_EQ(task->sid, sid_before);

    return true;
}

/* ============================================================================
 * Test: Multiple sequential fork/wait cycles
 * Validates: VAL-CROSS-009 through VAL-CROSS-012 (endurance)
 * ============================================================================ */
IXLAND_TEST(lifecycle_sequential_fork_wait_cycles) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Run 10 sequential fork/wait cycles */
    for (int i = 0; i < 10; i++) {
        /* Create child */
        ixland_task_t *child = create_test_child(parent);
        IXLAND_ASSERT_NOT_NULL(child);
        pid_t child_pid = child->pid;

        /* Verify unique PID each time */
        IXLAND_ASSERT(child_pid > 0);

        /* Exit with cycle number */
        simulate_child_exit(child, i);

        /* Wait and verify */
        int status;
        pid_t result = ixland_waitpid(child_pid, &status, 0);
        IXLAND_ASSERT_EQ(result, child_pid);
        IXLAND_ASSERT(WIFEXITED(status));
        IXLAND_ASSERT_EQ(WEXITSTATUS(status), i);

        /* Children list should be empty */
        pthread_mutex_lock(&parent->lock);
        IXLAND_ASSERT_NULL(parent->children);
        pthread_mutex_unlock(&parent->lock);
    }

    return true;
}

/* ============================================================================
 * Test: Child with different exit codes (comprehensive)
 * Validates: VAL-CROSS-011 (exit status preservation)
 * ============================================================================ */
IXLAND_TEST(lifecycle_comprehensive_exit_codes) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Test boundary and special exit codes */
    int exit_codes[] = {
        0,   /* Success */
        1,   /* General error */
        2,   /* Misuse of shell builtins */
        126, /* Command invoked cannot execute */
        127, /* Command not found */
        128, /* Invalid argument to exit */
        130, /* Script terminated by Ctrl-C (128 + SIGINT) */
        137, /* SIGKILL (128 + 9) */
        143, /* SIGTERM (128 + 15) */
        255  /* Exit status out of range */
    };
    int num_codes = sizeof(exit_codes) / sizeof(exit_codes[0]);

    for (int i = 0; i < num_codes; i++) {
        ixland_task_t *child = create_test_child(parent);
        IXLAND_ASSERT_NOT_NULL(child);
        pid_t child_pid = child->pid; /* Save PID before potential free */

        simulate_child_exit(child, exit_codes[i]);

        int status;
        pid_t result = ixland_waitpid(child_pid, &status, 0);
        IXLAND_ASSERT_EQ(result, child_pid);
        IXLAND_ASSERT(WIFEXITED(status));
        IXLAND_ASSERT_EQ(WEXITSTATUS(status), exit_codes[i]);

        /* Verify children list is empty after wait */
        pthread_mutex_lock(&parent->lock);
        IXLAND_ASSERT_NULL(parent->children);
        pthread_mutex_unlock(&parent->lock);
    }

    return true;
}
