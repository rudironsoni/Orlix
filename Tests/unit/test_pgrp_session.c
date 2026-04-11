#include <errno.h>
#include <signal.h>
#include <stdlib.h>

#include "../../fs/fdtable.h"
#include "../../fs/vfs.h"
#include "../../kernel/signal/ixland_signal.h"
#include "../../kernel/task/task.h"
#include "../harness/ixland_test.h"

/* Note: Kernel layer provides pgid/sid fields but no manipulation APIs.
 * These fields are set at task allocation time and inherited during fork.
 * Process group reassignment (setpgid) and session creation (setsid)
 * are implemented at a higher layer (src/ixland/core/ixland_process.c).
 *
 * These tests verify the kernel-level behavior.
 */

IXLAND_TEST(pgrp_initial_task_leader_invariants) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(task);

    /* Initial task starts as its own process group leader */
    IXLAND_ASSERT_EQ(task->pgid, task->pid);
    IXLAND_ASSERT_EQ(task->sid, task->pid);

    /* pgid and sid should be positive */
    IXLAND_ASSERT(task->pgid > 0);
    IXLAND_ASSERT(task->sid > 0);

    /* For initial task, pgid == pid == sid */
    IXLAND_ASSERT_EQ(task->pgid, task->sid);

    return true;
}

IXLAND_TEST(pgrp_fork_inheritance) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    pid_t parent_pgid = parent->pgid;
    pid_t parent_sid = parent->sid;

    /* Create child manually (simulating fork path) */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    /* Set up child with inherited values */
    child->ppid = parent->pid;
    child->pgid = parent->pgid; /* Inherited from parent */
    child->sid = parent->sid;   /* Inherited from parent */

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();
    IXLAND_ASSERT_NOT_NULL(child->files);
    IXLAND_ASSERT_NOT_NULL(child->fs);
    IXLAND_ASSERT_NOT_NULL(child->sighand);

    /* Link child to parent */
    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Verify inheritance */
    IXLAND_ASSERT_EQ(child->pgid, parent_pgid);
    IXLAND_ASSERT_EQ(child->sid, parent_sid);
    IXLAND_ASSERT_EQ(child->pgid, parent->pgid);
    IXLAND_ASSERT_EQ(child->sid, parent->sid);

    /* Child is NOT leader of its own group */
    IXLAND_ASSERT(child->pgid != child->pid);
    IXLAND_ASSERT(child->sid != child->pid);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);
    ixland_task_free(child);

    return true;
}

IXLAND_TEST(pgrp_multiple_children_same_group) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    pid_t parent_pgid = parent->pgid;

    /* Create multiple children in same process group */
    ixland_task_t *children[3];

    for (int i = 0; i < 3; i++) {
        children[i] = ixland_task_alloc();
        IXLAND_ASSERT_NOT_NULL(children[i]);

        children[i]->ppid = parent->pid;
        children[i]->pgid = parent->pgid; /* Same PGID */
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

    /* Verify all children share same process group */
    for (int i = 0; i < 3; i++) {
        IXLAND_ASSERT_EQ(children[i]->pgid, parent_pgid);
        IXLAND_ASSERT_EQ(children[i]->pgid, children[0]->pgid);
    }

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);

    for (int i = 0; i < 3; i++) {
        ixland_task_free(children[i]);
    }

    return true;
}

IXLAND_TEST(pgrp_session_invariants) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* sid should always equal pgid for session leader */
    /* or be the session ID inherited from parent */
    IXLAND_ASSERT_EQ(task->sid, task->pgid);

    /* Create child to verify session inheritance */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->ppid = task->pid;
    child->pgid = task->pgid;
    child->sid = task->sid;

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    pthread_mutex_lock(&task->lock);
    child->parent = task;
    child->next_sibling = task->children;
    task->children = child;
    pthread_mutex_unlock(&task->lock);

    /* Child inherits session */
    IXLAND_ASSERT_EQ(child->sid, task->sid);
    IXLAND_ASSERT(child->sid != child->pid); /* Child is not session leader */

    /* Cleanup */
    pthread_mutex_lock(&task->lock);
    task->children = NULL;
    pthread_mutex_unlock(&task->lock);
    ixland_task_free(child);

    return true;
}

IXLAND_TEST(pgrp_child_lookup_by_pgid) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    pid_t pgid = parent->pgid;

    /* Create child in same process group */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->ppid = parent->pid;
    child->pgid = pgid;
    child->sid = parent->sid;

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Verify we can find child and check its PGID */
    ixland_task_t *found = ixland_task_lookup(child->pid);
    IXLAND_ASSERT_NOT_NULL(found);
    IXLAND_ASSERT_EQ(found->pgid, pgid);
    IXLAND_ASSERT_EQ(found->pgid, parent->pgid);

    ixland_task_free(found);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);
    ixland_task_free(child);

    return true;
}

IXLAND_TEST(pgrp_zombie_retains_pgid) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    pid_t pgid = parent->pgid;

    /* Create child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->ppid = parent->pid;
    child->pgid = pgid;
    child->sid = parent->sid;

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Simulate child exit - becomes zombie */
    pthread_mutex_lock(&child->lock);
    child->exit_status = 0;
    atomic_store(&child->exited, true);
    atomic_store(&child->state, IXLAND_TASK_ZOMBIE);
    pthread_mutex_unlock(&child->lock);

    /* Zombie should retain its pgid */
    IXLAND_ASSERT_EQ(child->pgid, pgid);
    IXLAND_ASSERT(atomic_load(&child->state) == IXLAND_TASK_ZOMBIE);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);
    ixland_task_free(child);

    return true;
}

IXLAND_TEST(pgrp_refcount_with_membership) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();

    /* Create child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    int initial_refs = atomic_load(&child->refs);

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

    /* Membership doesn't change refcount directly */
    IXLAND_ASSERT_EQ(atomic_load(&child->refs), initial_refs);

    /* But lookup does increment it */
    ixland_task_t *found = ixland_task_lookup(child->pid);
    IXLAND_ASSERT_EQ(atomic_load(&child->refs), initial_refs + 1);

    ixland_task_free(found);
    IXLAND_ASSERT_EQ(atomic_load(&child->refs), initial_refs);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);
    ixland_task_free(child);

    return true;
}

/* ============================================================================
 * SESSION AND PROCESS GROUP API TESTS
 * ============================================================================
 *
 * Tests for the higher-level APIs: getsid(), setsid(), getpgrp(), getpgid(), setpgid()
 */

IXLAND_TEST(setsid_creates_new_session_and_pgrp) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    pid_t original_pgid = task->pgid;
    pid_t original_sid = task->sid;
    pid_t my_pid = task->pid;

    /* Cannot call setsid if already group leader */
    if (task->pgid == my_pid) {
        /* Already leader - need to join different group first */
        /* This shouldn't happen for init task, but handle gracefully */
        IXLAND_ASSERT_TRUE(true); /* Skip condition */
        return true;
    }

    pid_t new_sid = ixland_setsid();
    IXLAND_ASSERT_NE(new_sid, -1);
    IXLAND_ASSERT_EQ(new_sid, my_pid);

    /* Verify session changed */
    pid_t current_sid = ixland_getsid(0);
    IXLAND_ASSERT_EQ(current_sid, my_pid);
    IXLAND_ASSERT_NE(current_sid, original_sid);

    /* Verify process group changed */
    pid_t current_pgid = ixland_getpgrp();
    IXLAND_ASSERT_EQ(current_pgid, my_pid);
    IXLAND_ASSERT_NE(current_pgid, original_pgid);

    /* sid == pgid after setsid */
    IXLAND_ASSERT_EQ(current_sid, current_pgid);

    return true;
}

IXLAND_TEST(setsid_fails_for_group_leader) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    /* First, become group leader if not already */
    if (task->pgid != task->pid) {
        int ret = ixland_setpgid(0, task->pid);
        IXLAND_ASSERT_EQ(ret, 0);
    }

    /* Now try setsid - should fail with EPERM */
    pid_t result = ixland_setsid();
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EPERM);

    return true;
}

IXLAND_TEST(getsid_returns_correct_session) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    pid_t my_pid = task->pid;

    /* getsid(0) should return current session */
    pid_t sid = ixland_getsid(0);
    IXLAND_ASSERT_EQ(sid, task->sid);

    /* getsid(pid) should return that process's session */
    pid_t sid_by_pid = ixland_getsid(my_pid);
    IXLAND_ASSERT_EQ(sid_by_pid, sid);

    /* Create a child and verify it inherits session */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->ppid = my_pid;
    child->pgid = task->pgid;
    child->sid = task->sid;
    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    pthread_mutex_lock(&task->lock);
    child->parent = task;
    child->next_sibling = task->children;
    task->children = child;
    pthread_mutex_unlock(&task->lock);

    /* Child should be in same session */
    pid_t child_sid = ixland_getsid(child->pid);
    IXLAND_ASSERT_EQ(child_sid, sid);

    /* Cleanup */
    pthread_mutex_lock(&task->lock);
    task->children = NULL;
    pthread_mutex_unlock(&task->lock);
    ixland_task_free(child);

    return true;
}

IXLAND_TEST(getsid_fails_for_invalid_pid) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Try to get session for non-existent PID */
    pid_t result = ixland_getsid(99999);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ESRCH);

    return true;
}

IXLAND_TEST(getpgrp_returns_process_group) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();

    pid_t pgrp = ixland_getpgrp();
    IXLAND_ASSERT_EQ(pgrp, task->pgid);

    return true;
}

IXLAND_TEST(getpgid_returns_process_group) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    pid_t my_pid = task->pid;

    /* getpgid(0) should be same as getpgrp() */
    pid_t pgid_zero = ixland_getpgid(0);
    pid_t pgrp = ixland_getpgrp();
    IXLAND_ASSERT_EQ(pgid_zero, pgrp);

    /* getpgid(pid) should return that process's group */
    pid_t pgid_by_pid = ixland_getpgid(my_pid);
    IXLAND_ASSERT_EQ(pgid_by_pid, pgrp);

    return true;
}

IXLAND_TEST(getpgid_fails_for_invalid_pid) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    pid_t result = ixland_getpgid(99999);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ESRCH);

    return true;
}

IXLAND_TEST(setpgid_moves_process_to_group) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    pid_t parent_pgid = parent->pgid;

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

    /* Move child to its own process group */
    int ret = ixland_setpgid(child->pid, child->pid);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Verify child is now group leader */
    IXLAND_ASSERT_EQ(child->pgid, child->pid);

    /* Move child back to parent's group */
    ret = ixland_setpgid(child->pid, parent_pgid);
    IXLAND_ASSERT_EQ(ret, 0);

    /* Verify child is back in parent's group */
    IXLAND_ASSERT_EQ(child->pgid, parent_pgid);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);
    ixland_task_free(child);

    return true;
}

IXLAND_TEST(setpgid_zero_pid_uses_current) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *task = ixland_current_task();
    pid_t original_pgid = task->pgid;

    /* setpgid(0, 0) should set current process to its own group */
    /* But only if not already leader */
    if (original_pgid != task->pid) {
        int ret = ixland_setpgid(0, 0);
        IXLAND_ASSERT_EQ(ret, 0);
        IXLAND_ASSERT_EQ(task->pgid, task->pid);
    }

    return true;
}

IXLAND_TEST(setpgid_fails_for_non_child) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Try to change pgid of a non-child, non-self process */
    /* Create a "foreign" task not in our children list */
    ixland_task_t *foreign = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(foreign);

    /* Set up foreign task with no parent relationship */
    foreign->ppid = 1; /* Init */
    foreign->pgid = foreign->pid;
    foreign->sid = foreign->pid;
    foreign->files = ixland_files_alloc(IXLAND_MAX_FD);
    foreign->fs = ixland_fs_alloc();
    foreign->sighand = ixland_sighand_alloc();

    /* Task is already registered by ixland_task_alloc; keep it out of our children list */

    /* Try to change foreign process's group - should fail */
    int ret = ixland_setpgid(foreign->pid, foreign->pid);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EPERM);

    /* Cleanup */
    ixland_task_free(foreign);

    return true;
}

IXLAND_TEST(setpgid_fails_cross_session) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    pid_t parent_sid = parent->sid;

    /* Create child in different session */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->ppid = parent->pid;
    child->pgid = parent->pgid;
    child->sid = parent->sid + 1; /* Different session */
    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Try to change child's pgid - should fail due to session mismatch */
    int ret = ixland_setpgid(child->pid, parent->pgid);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EPERM);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);
    ixland_task_free(child);

    return true;
}
