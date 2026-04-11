/**
 * @file test_process_relationships.c
 * @brief Unit tests for parent/child relationship tracking
 *
 * Validates VAL-PROCESS-007, VAL-PROCESS-008: Parent-child linkage and traversal
 */

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "../../fs/fdtable.h"
#include "../../fs/vfs.h"
#include "../../kernel/signal/ixland_signal.h"
#include "../../kernel/task/task.h"
#include "../harness/ixland_test.h"

/* Test parent-child linkage on fork (VAL-PROCESS-007) */
IXLAND_TEST(fork_child_links_to_parent) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create child manually simulating fork behavior */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    /* Allocate resources */
    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();
    IXLAND_ASSERT_NOT_NULL(child->files);
    IXLAND_ASSERT_NOT_NULL(child->fs);
    IXLAND_ASSERT_NOT_NULL(child->sighand);

    /* Set up parent-child relationship */
    child->ppid = parent->pid;
    child->pgid = parent->pgid;
    child->sid = parent->sid;

    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Verify parent-child linkage (VAL-PROCESS-007) */
    IXLAND_ASSERT_EQ(child->ppid, parent->pid);
    IXLAND_ASSERT_EQ(child->parent, parent);

    /* Cleanup - unlink child */
    pthread_mutex_lock(&parent->lock);
    parent->children = child->next_sibling;
    pthread_mutex_unlock(&parent->lock);

    ixland_task_free(child);
    return true;
}

/* Test child list traversal (VAL-PROCESS-008) */
IXLAND_TEST(parent_can_traverse_children) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create multiple children */
    ixland_task_t *child1 = ixland_task_alloc();
    ixland_task_t *child2 = ixland_task_alloc();
    ixland_task_t *child3 = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child1);
    IXLAND_ASSERT_NOT_NULL(child2);
    IXLAND_ASSERT_NOT_NULL(child3);

    /* Allocate resources */
    child1->files = ixland_files_alloc(IXLAND_MAX_FD);
    child1->fs = ixland_fs_alloc();
    child1->sighand = ixland_sighand_alloc();
    child2->files = ixland_files_alloc(IXLAND_MAX_FD);
    child2->fs = ixland_fs_alloc();
    child2->sighand = ixland_sighand_alloc();
    child3->files = ixland_files_alloc(IXLAND_MAX_FD);
    child3->fs = ixland_fs_alloc();
    child3->sighand = ixland_sighand_alloc();

    /* Set up parent-child relationships */
    child1->ppid = parent->pid;
    child2->ppid = parent->pid;
    child3->ppid = parent->pid;

    pthread_mutex_lock(&parent->lock);

    /* Insert at head: child3, then child2, then child1 */
    child3->parent = parent;
    child3->next_sibling = parent->children;
    parent->children = child3;

    child2->parent = parent;
    child2->next_sibling = parent->children;
    parent->children = child2;

    child1->parent = parent;
    child1->next_sibling = parent->children;
    parent->children = child1;

    pthread_mutex_unlock(&parent->lock);

    /* Traverse children and verify all are reachable (VAL-PROCESS-008) */
    pthread_mutex_lock(&parent->lock);

    int count = 0;
    ixland_task_t *child = parent->children;
    while (child) {
        count++;
        IXLAND_ASSERT_EQ(child->parent, parent);
        IXLAND_ASSERT_EQ(child->ppid, parent->pid);
        child = child->next_sibling;
    }

    IXLAND_ASSERT_EQ(count, 3);
    pthread_mutex_unlock(&parent->lock);

    /* Cleanup - unlink all children */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);

    ixland_task_free(child1);
    ixland_task_free(child2);
    ixland_task_free(child3);

    return true;
}

/* Test sibling list forms NULL-terminated list */
IXLAND_TEST(sibling_list_null_terminated) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create a single child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    child->ppid = parent->pid;

    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Verify single child has NULL next_sibling after insertion at head */
    /* (since parent->children was initially NULL) */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NULL(child->next_sibling);
    pthread_mutex_unlock(&parent->lock);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);

    ixland_task_free(child);
    return true;
}

/* Test multiple children have correct sibling chain */
IXLAND_TEST(multiple_children_sibling_chain) {
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
    child2->ppid = parent->pid;

    pthread_mutex_lock(&parent->lock);

    /* Insert child2 first, then child1 */
    child2->parent = parent;
    child2->next_sibling = parent->children;
    parent->children = child2;

    child1->parent = parent;
    child1->next_sibling = parent->children;
    parent->children = child1;

    pthread_mutex_unlock(&parent->lock);

    /* Verify chain: child1 -> child2 -> NULL */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_EQ(parent->children, child1);
    IXLAND_ASSERT_EQ(child1->next_sibling, child2);
    IXLAND_ASSERT_NULL(child2->next_sibling);
    pthread_mutex_unlock(&parent->lock);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);

    ixland_task_free(child1);
    ixland_task_free(child2);

    return true;
}

/* Test orphan reparenting to init */
IXLAND_TEST(orphan_reparenting_to_init) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create a child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    /* Link child to parent */
    child->ppid = parent->pid;

    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Verify initial linkage */
    IXLAND_ASSERT_EQ(child->parent, parent);
    IXLAND_ASSERT_EQ(child->ppid, parent->pid);

    /* Simulate orphan reparenting (what happens when parent exits) */
    pthread_mutex_lock(&child->lock);
    child->ppid = 1; /* init's PID */
    pthread_mutex_unlock(&child->lock);

    /* Verify orphan was reparented */
    IXLAND_ASSERT_EQ(child->ppid, 1);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);

    ixland_task_free(child);
    return true;
}

/* Test child lookup by traversing from parent */
IXLAND_TEST(child_lookup_by_traversal) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    child->ppid = parent->pid;

    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Find child by traversing */
    ixland_task_t *found = NULL;
    pthread_mutex_lock(&parent->lock);
    ixland_task_t *c = parent->children;
    while (c) {
        if (c->pid == child->pid) {
            found = c;
            break;
        }
        c = c->next_sibling;
    }
    pthread_mutex_unlock(&parent->lock);

    IXLAND_ASSERT_NOT_NULL(found);
    IXLAND_ASSERT_EQ(found, child);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);

    ixland_task_free(child);
    return true;
}

/* Test multi-level process tree (grandparent -> parent -> child) */
IXLAND_TEST(multi_level_process_tree) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *grandparent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(grandparent);

    /* Create parent */
    ixland_task_t *parent = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(parent);

    parent->files = ixland_files_alloc(IXLAND_MAX_FD);
    parent->fs = ixland_fs_alloc();
    parent->sighand = ixland_sighand_alloc();

    parent->ppid = grandparent->pid;

    pthread_mutex_lock(&grandparent->lock);
    parent->parent = grandparent;
    parent->next_sibling = grandparent->children;
    grandparent->children = parent;
    pthread_mutex_unlock(&grandparent->lock);

    /* Create child of parent */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    child->ppid = parent->pid;

    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Verify three-level hierarchy */
    IXLAND_ASSERT_EQ(parent->parent, grandparent);
    IXLAND_ASSERT_EQ(parent->ppid, grandparent->pid);
    IXLAND_ASSERT_EQ(child->parent, parent);
    IXLAND_ASSERT_EQ(child->ppid, parent->pid);

    /* Traverse from grandparent to find child (via parent) */
    pthread_mutex_lock(&grandparent->lock);
    IXLAND_ASSERT_NOT_NULL(grandparent->children);
    IXLAND_ASSERT_EQ(grandparent->children, parent);
    pthread_mutex_unlock(&grandparent->lock);

    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_NOT_NULL(parent->children);
    IXLAND_ASSERT_EQ(parent->children, child);
    pthread_mutex_unlock(&parent->lock);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);

    pthread_mutex_lock(&grandparent->lock);
    grandparent->children = NULL;
    pthread_mutex_unlock(&grandparent->lock);

    ixland_task_free(child);
    ixland_task_free(parent);

    return true;
}

/* Test children maintain correct order (LIFO) */
IXLAND_TEST(children_insertion_order_lifo) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create children in order */
    ixland_task_t *child1 = ixland_task_alloc();
    ixland_task_t *child2 = ixland_task_alloc();
    ixland_task_t *child3 = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child1);
    IXLAND_ASSERT_NOT_NULL(child2);
    IXLAND_ASSERT_NOT_NULL(child3);

    child1->files = ixland_files_alloc(IXLAND_MAX_FD);
    child1->fs = ixland_fs_alloc();
    child1->sighand = ixland_sighand_alloc();
    child2->files = ixland_files_alloc(IXLAND_MAX_FD);
    child2->fs = ixland_fs_alloc();
    child2->sighand = ixland_sighand_alloc();
    child3->files = ixland_files_alloc(IXLAND_MAX_FD);
    child3->fs = ixland_fs_alloc();
    child3->sighand = ixland_sighand_alloc();

    /* Insert in order: 1, 2, 3 */
    pthread_mutex_lock(&parent->lock);

    child1->ppid = parent->pid;
    child1->parent = parent;
    child1->next_sibling = parent->children;
    parent->children = child1;

    child2->ppid = parent->pid;
    child2->parent = parent;
    child2->next_sibling = parent->children;
    parent->children = child2;

    child3->ppid = parent->pid;
    child3->parent = parent;
    child3->next_sibling = parent->children;
    parent->children = child3;

    pthread_mutex_unlock(&parent->lock);

    /* Verify LIFO order: 3 -> 2 -> 1 -> NULL */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_EQ(parent->children, child3);
    IXLAND_ASSERT_EQ(child3->next_sibling, child2);
    IXLAND_ASSERT_EQ(child2->next_sibling, child1);
    IXLAND_ASSERT_NULL(child1->next_sibling);
    pthread_mutex_unlock(&parent->lock);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);

    ixland_task_free(child1);
    ixland_task_free(child2);
    ixland_task_free(child3);

    return true;
}

/* Test unlinking specific child from middle of list */
IXLAND_TEST(unlink_middle_child) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create three children */
    ixland_task_t *child1 = ixland_task_alloc();
    ixland_task_t *child2 = ixland_task_alloc();
    ixland_task_t *child3 = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child1);
    IXLAND_ASSERT_NOT_NULL(child2);
    IXLAND_ASSERT_NOT_NULL(child3);

    child1->files = ixland_files_alloc(IXLAND_MAX_FD);
    child1->fs = ixland_fs_alloc();
    child1->sighand = ixland_sighand_alloc();
    child2->files = ixland_files_alloc(IXLAND_MAX_FD);
    child2->fs = ixland_fs_alloc();
    child2->sighand = ixland_sighand_alloc();
    child3->files = ixland_files_alloc(IXLAND_MAX_FD);
    child3->fs = ixland_fs_alloc();
    child3->sighand = ixland_sighand_alloc();

    /* Link in order: 1, 2, 3 (results in 3 -> 2 -> 1) */
    pthread_mutex_lock(&parent->lock);

    child1->ppid = parent->pid;
    child1->parent = parent;
    child1->next_sibling = parent->children;
    parent->children = child1;

    child2->ppid = parent->pid;
    child2->parent = parent;
    child2->next_sibling = parent->children;
    parent->children = child2;

    child3->ppid = parent->pid;
    child3->parent = parent;
    child3->next_sibling = parent->children;
    parent->children = child3;

    pthread_mutex_unlock(&parent->lock);

    /* Unlink child2 (middle) - list becomes 3 -> 1 */
    pthread_mutex_lock(&parent->lock);
    ixland_task_t **pp = &parent->children;
    while (*pp && *pp != child2) {
        pp = &(*pp)->next_sibling;
    }
    if (*pp) {
        *pp = child2->next_sibling;
    }
    pthread_mutex_unlock(&parent->lock);

    /* Verify child2 is unlinked */
    pthread_mutex_lock(&parent->lock);
    IXLAND_ASSERT_EQ(parent->children, child3);
    IXLAND_ASSERT_EQ(child3->next_sibling, child1);
    IXLAND_ASSERT_NULL(child1->next_sibling);
    pthread_mutex_unlock(&parent->lock);

    /* Cleanup */
    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);

    ixland_task_free(child1);
    ixland_task_free(child2);
    ixland_task_free(child3);

    return true;
}

/* Test that init task is its own parent */
IXLAND_TEST(init_task_self_parent) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *init = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(init);

    /* After init, the current task is init */
    IXLAND_ASSERT_EQ(init->ppid, init->pid);
    IXLAND_ASSERT_EQ(init->pid, init->pgid);
    IXLAND_ASSERT_EQ(init->pid, init->sid);

    return true;
}

/* Test child reference counting with parent pointer */
IXLAND_TEST(child_with_parent_refcount) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    ixland_task_t *parent = ixland_current_task();
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Create child */
    ixland_task_t *child = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(child);

    child->files = ixland_files_alloc(IXLAND_MAX_FD);
    child->fs = ixland_fs_alloc();
    child->sighand = ixland_sighand_alloc();

    child->ppid = parent->pid;

    pthread_mutex_lock(&parent->lock);
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
    pthread_mutex_unlock(&parent->lock);

    /* Child should have initial refcount of 1 */
    IXLAND_ASSERT_EQ(atomic_load(&child->refs), 1);

    /* Lookup should increment refcount */
    ixland_task_t *found = ixland_task_lookup(child->pid);
    IXLAND_ASSERT_NOT_NULL(found);
    IXLAND_ASSERT_EQ(atomic_load(&child->refs), 2);

    /* Cleanup */
    ixland_task_free(found);
    IXLAND_ASSERT_EQ(atomic_load(&child->refs), 1);

    pthread_mutex_lock(&parent->lock);
    parent->children = NULL;
    pthread_mutex_unlock(&parent->lock);

    ixland_task_free(child);

    return true;
}
