/**
 * @file test_task_alloc_thread_safety.c
 * @brief Thread safety tests for task allocation and deallocation
 *
 * Validates VAL-PROCESS-001, VAL-PROCESS-002, VAL-PROCESS-003
 * Specifically tests concurrent alloc/free operations.
 */

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "../../kernel/task/task.h"
#include "../harness/ixland_test.h"

#define NUM_THREADS 8
#define ALLOCS_PER_THREAD 100

/* Test concurrent task allocation from multiple threads */
static _Atomic int alloc_count = 0;
static _Atomic int free_count = 0;
static _Atomic int success_count = 0;
static _Atomic int ready_count = 0;

static void *alloc_thread(void *arg) {
    (void)arg;

    /* Signal ready and wait for all threads */
    atomic_fetch_add(&ready_count, 1);
    while (atomic_load(&ready_count) < NUM_THREADS) {
        /* Spin-wait for all threads to be ready */
    }

    for (int i = 0; i < ALLOCS_PER_THREAD; i++) {
        ixland_task_t *task = ixland_task_alloc();
        if (task) {
            atomic_fetch_add(&alloc_count, 1);
            atomic_fetch_add(&success_count, 1);

            /* Verify task is properly initialized */
            if (task->pid >= 1000 && task->state == IXLAND_TASK_RUNNING &&
                atomic_load(&task->refs) == 1) {
                atomic_fetch_add(&free_count, 1);
                ixland_task_free(task);
            } else {
                /* Task not properly initialized - just free it */
                ixland_task_free(task);
            }
        }
    }

    return NULL;
}

IXLAND_TEST(task_alloc_concurrent_safety) {
    pthread_t threads[NUM_THREADS];

    /* Reset counters */
    atomic_store(&alloc_count, 0);
    atomic_store(&free_count, 0);
    atomic_store(&success_count, 0);
    atomic_store(&ready_count, 0);

    /* Create worker threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        int ret = pthread_create(&threads[i], NULL, alloc_thread, NULL);
        IXLAND_ASSERT(ret == 0);
    }

    /* Wait for all threads to complete */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Verify results */
    int total_allocs = atomic_load(&alloc_count);
    int total_frees = atomic_load(&free_count);
    int total_success = atomic_load(&success_count);

    /* All allocations should succeed (or at least most) */
    IXLAND_ASSERT(total_success > 0);

    /* Each successful alloc should have been freed */
    IXLAND_ASSERT_EQ(total_allocs, total_frees);

    /* Total should be threads * allocs per thread */
    IXLAND_ASSERT_EQ(total_success, NUM_THREADS * ALLOCS_PER_THREAD);

    return true;
}

/* Test that lookup and free are thread-safe */
static ixland_task_t *shared_task = NULL;
static _Atomic int lookup_count = 0;
static _Atomic int lookup_ready = 0;

static void *lookup_thread(void *arg) {
    (void)arg;

    /* Signal ready and wait */
    atomic_fetch_add(&lookup_ready, 1);
    while (atomic_load(&lookup_ready) < NUM_THREADS) {
        /* Spin-wait */
    }

    for (int i = 0; i < 50; i++) {
        ixland_task_t *task = ixland_task_lookup(shared_task->pid);
        if (task) {
            atomic_fetch_add(&lookup_count, 1);

            /* Verify it's the same task */
            IXLAND_ASSERT_EQ(task->pid, shared_task->pid);

            /* Simulate some work */
            usleep(1);

            ixland_task_free(task);
        }
    }

    return NULL;
}

IXLAND_TEST(task_lookup_refcount_thread_safety) {
    /* Initialize task system first */
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Create a shared task */
    shared_task = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(shared_task);

    /* Get initial refcount */
    int initial_refs = atomic_load(&shared_task->refs);
    IXLAND_ASSERT(initial_refs == 1);

    pthread_t threads[NUM_THREADS];
    atomic_store(&lookup_count, 0);
    atomic_store(&lookup_ready, 0);

    /* Create threads that will lookup and release the task */
    for (int i = 0; i < NUM_THREADS; i++) {
        int ret = pthread_create(&threads[i], NULL, lookup_thread, NULL);
        IXLAND_ASSERT(ret == 0);
    }

    /* Wait for completion */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Verify all lookups succeeded */
    int total_lookups = atomic_load(&lookup_count);
    IXLAND_ASSERT_EQ(total_lookups, NUM_THREADS * 50);

    /* Refcount should be back to initial value */
    int final_refs = atomic_load(&shared_task->refs);
    IXLAND_ASSERT_EQ(final_refs, initial_refs);

    /* Cleanup the shared task */
    ixland_task_free(shared_task);

    return true;
}

/* Test that free with refs > 1 only decrements, doesn't free */
IXLAND_TEST(task_free_decrements_only_when_refs_high) {
    IXLAND_ASSERT(ixland_task_init() == 0);

    /* Allocate a task */
    ixland_task_t *task = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(task);
    pid_t pid = task->pid;

    /* Verify initial refcount is 1 */
    IXLAND_ASSERT_EQ(atomic_load(&task->refs), 1);

    /* Lookup the task multiple times to increase refcount */
    ixland_task_t *ref1 = ixland_task_lookup(pid);
    ixland_task_t *ref2 = ixland_task_lookup(pid);
    ixland_task_t *ref3 = ixland_task_lookup(pid);

    /* Refcount should now be 4 */
    IXLAND_ASSERT_EQ(atomic_load(&task->refs), 4);

    /* Free one reference - should not actually free the task */
    ixland_task_free(ref1);
    IXLAND_ASSERT_EQ(atomic_load(&task->refs), 3);

    /* Task should still be lookup-able */
    ixland_task_t *still_there = ixland_task_lookup(pid);
    IXLAND_ASSERT_NOT_NULL(still_there);
    IXLAND_ASSERT_EQ(still_there->pid, pid);
    IXLAND_ASSERT_EQ(atomic_load(&still_there->refs), 4);

    /* Free the new reference */
    ixland_task_free(still_there);
    IXLAND_ASSERT_EQ(atomic_load(&task->refs), 3);

    /* Free remaining references */
    ixland_task_free(ref2);
    IXLAND_ASSERT_EQ(atomic_load(&task->refs), 2);

    ixland_task_free(ref3);
    IXLAND_ASSERT_EQ(atomic_load(&task->refs), 1);

    /* Now free the original reference - this should actually free the task */
    ixland_task_free(task);

    /* Task should no longer be in hash table */
    ixland_task_t *gone = ixland_task_lookup(pid);
    IXLAND_ASSERT_NULL(gone);

    return true;
}

/* Test that alloc returns non-null for valid allocations */
IXLAND_TEST(task_alloc_returns_non_null) {
    /* Allocate multiple tasks */
    ixland_task_t *tasks[10];

    for (int i = 0; i < 10; i++) {
        tasks[i] = ixland_task_alloc();
        IXLAND_ASSERT_NOT_NULL(tasks[i]);

        /* Verify each task has a unique PID */
        for (int j = 0; j < i; j++) {
            IXLAND_ASSERT_NE(tasks[i]->pid, tasks[j]->pid);
        }
    }

    /* Cleanup */
    for (int i = 0; i < 10; i++) {
        ixland_task_free(tasks[i]);
    }

    return true;
}

/* Test that free with refs=0 releases memory and removes from hash */
IXLAND_TEST(task_free_releases_memory_and_removes_from_hash) {
    /* Allocate a task */
    ixland_task_t *task = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(task);
    pid_t pid = task->pid;

    /* Verify it's in the hash table */
    ixland_task_t *found = ixland_task_lookup(pid);
    IXLAND_ASSERT_NOT_NULL(found);
    IXLAND_ASSERT_EQ(found->pid, pid);
    ixland_task_free(found); /* Release lookup reference */

    /* Free the task - refcount goes from 1 to 0 */
    ixland_task_free(task);

    /* Task should no longer be lookup-able */
    ixland_task_t *gone = ixland_task_lookup(pid);
    IXLAND_ASSERT_NULL(gone);

    return true;
}

/* Test that task fields are initialized to safe defaults */
IXLAND_TEST(task_alloc_safe_defaults) {
    ixland_task_t *task = ixland_task_alloc();
    IXLAND_ASSERT_NOT_NULL(task);

    /* Core fields should be initialized */
    IXLAND_ASSERT(task->pid >= 1000);                      /* Valid PID */
    IXLAND_ASSERT(task->state == IXLAND_TASK_RUNNING);     /* Running state */
    IXLAND_ASSERT_EQ(atomic_load(&task->refs), 1);         /* Refcount = 1 */
    IXLAND_ASSERT_EQ(atomic_load(&task->exited), false);   /* Not exited */
    IXLAND_ASSERT_EQ(atomic_load(&task->signaled), false); /* Not signaled */

    /* Process group/session initialized to PID */
    IXLAND_ASSERT_EQ(task->tgid, task->pid);
    IXLAND_ASSERT_EQ(task->pgid, task->pid);
    IXLAND_ASSERT_EQ(task->sid, task->pid);

    /* Resource pointers should be NULL (not allocated by alloc) */
    IXLAND_ASSERT_NULL(task->files);
    IXLAND_ASSERT_NULL(task->fs);
    IXLAND_ASSERT_NULL(task->sighand);
    IXLAND_ASSERT_NULL(task->tty);
    IXLAND_ASSERT_NULL(task->mm);
    IXLAND_ASSERT_NULL(task->exec_image);

    /* Family pointers should be NULL */
    IXLAND_ASSERT_NULL(task->parent);
    IXLAND_ASSERT_NULL(task->children);
    IXLAND_ASSERT_NULL(task->next_sibling);

    /* vfork_parent should be NULL */
    IXLAND_ASSERT_NULL(task->vfork_parent);

    ixland_task_free(task);

    return true;
}

/* Stress test: rapid alloc/free cycles */
IXLAND_TEST(task_alloc_free_stress) {
    const int iterations = 1000;

    for (int i = 0; i < iterations; i++) {
        ixland_task_t *task = ixland_task_alloc();
        IXLAND_ASSERT_NOT_NULL(task);

        /* Verify valid state */
        IXLAND_ASSERT(task->pid >= 1000);
        IXLAND_ASSERT(task->state == IXLAND_TASK_RUNNING);
        IXLAND_ASSERT_EQ(atomic_load(&task->refs), 1);

        ixland_task_free(task);
    }

    return true;
}
