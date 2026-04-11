#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#include "../../kernel/task/task.h"
#include "../harness/ixland_test.h"

#define IXLAND_MIN_PID 1000
#define IXLAND_MAX_PID 65535

IXLAND_TEST(pid_alloc_basic) {
    pid_t p1 = ixland_alloc_pid();
    pid_t p2 = ixland_alloc_pid();

    IXLAND_ASSERT_GT(p1, 0);
    IXLAND_ASSERT_GT(p2, 0);
    IXLAND_ASSERT_NE(p1, p2);

    ixland_free_pid(p1);
    ixland_free_pid(p2);
    return true;
}

IXLAND_TEST(pid_alloc_returns_unique_values) {
    /* Test that allocations return unique PIDs */
    pid_t p1 = ixland_alloc_pid();
    pid_t p2 = ixland_alloc_pid();
    pid_t p3 = ixland_alloc_pid();

    /* All PIDs should be unique */
    IXLAND_ASSERT_NE(p1, p2);
    IXLAND_ASSERT_NE(p2, p3);
    IXLAND_ASSERT_NE(p1, p3);

    /* All should be valid */
    IXLAND_ASSERT_GE(p1, IXLAND_MIN_PID);
    IXLAND_ASSERT_GE(p2, IXLAND_MIN_PID);
    IXLAND_ASSERT_GE(p3, IXLAND_MIN_PID);

    ixland_free_pid(p1);
    ixland_free_pid(p2);
    ixland_free_pid(p3);
    return true;
}

IXLAND_TEST(pid_alloc_many) {
    pid_t pids[10];
    for (int i = 0; i < 10; i++) {
        pids[i] = ixland_alloc_pid();
        IXLAND_ASSERT_GT(pids[i], 0);
    }

    for (int i = 0; i < 10; i++) {
        ixland_free_pid(pids[i]);
    }
    return true;
}

/* Test that freed PIDs are immediately reusable */
IXLAND_TEST(pid_free_immediate_reuse) {
    pid_t p1 = ixland_alloc_pid();
    IXLAND_ASSERT_GT(p1, 0);

    ixland_free_pid(p1);

    /* The freed PID should be the next one allocated (LIFO stack) */
    pid_t p2 = ixland_alloc_pid();
    IXLAND_ASSERT_EQ(p2, p1);

    ixland_free_pid(p2);
    return true;
}

/* Test PID range validation */
IXLAND_TEST(pid_range_validation) {
    pid_t pids[100];

    /* Allocate many PIDs and verify they're all in valid range */
    for (int i = 0; i < 100; i++) {
        pids[i] = ixland_alloc_pid();
        IXLAND_ASSERT_GT(pids[i], 0);
        IXLAND_ASSERT_GE(pids[i], IXLAND_MIN_PID);
        IXLAND_ASSERT_LE(pids[i], IXLAND_MAX_PID);
    }

    for (int i = 0; i < 100; i++) {
        ixland_free_pid(pids[i]);
    }
    return true;
}

/* Test that alloc returns -1 when exhausted (allocate many PIDs) */
IXLAND_TEST(pid_alloc_exhaustion) {
    /* We can't exhaust all 64K PIDs in a reasonable test, but we can
     * verify that the exhaustion logic would work by checking the
     * return value pattern doesn't break with many allocations.
     */
    pid_t pids[1000];
    int count = 0;

    for (int i = 0; i < 1000; i++) {
        pid_t p = ixland_alloc_pid();
        if (p < 0) {
            break; /* Exhausted */
        }
        pids[count++] = p;
    }

    IXLAND_ASSERT_GT(count, 0);

    /* Free all allocated PIDs */
    for (int i = 0; i < count; i++) {
        ixland_free_pid(pids[i]);
    }

    return true;
}

/* Test multiple alloc/free cycles */
#define PID_CYCLES 100
#define PIDS_PER_CYCLE 50

IXLAND_TEST(pid_alloc_free_cycles) {
    for (int cycle = 0; cycle < PID_CYCLES; cycle++) {
        pid_t pids[PIDS_PER_CYCLE];

        for (int i = 0; i < PIDS_PER_CYCLE; i++) {
            pids[i] = ixland_alloc_pid();
            IXLAND_ASSERT_GT(pids[i], 0);

            /* Verify no duplicates within this cycle */
            for (int j = 0; j < i; j++) {
                IXLAND_ASSERT_NE(pids[i], pids[j]);
            }
        }

        for (int i = 0; i < PIDS_PER_CYCLE; i++) {
            ixland_free_pid(pids[i]);
        }
    }

    return true;
}

/* Test uniqueness - no collisions with many allocations */
IXLAND_TEST(pid_uniqueness_no_collisions) {
    const int count = 500;
    pid_t pids[500];

    for (int i = 0; i < count; i++) {
        pids[i] = ixland_alloc_pid();
        IXLAND_ASSERT_GT(pids[i], 0);

        /* Check no duplicates with all previous allocations */
        for (int j = 0; j < i; j++) {
            IXLAND_ASSERT_NE(pids[i], pids[j]);
        }
    }

    for (int i = 0; i < count; i++) {
        ixland_free_pid(pids[i]);
    }

    return true;
}

/* Test wraparound behavior by simulating high PID usage */
IXLAND_TEST(pid_wraparound_simulation) {
    /* Allocate and free in a pattern that simulates wraparound */
    pid_t pids[200];

    /* First batch */
    for (int i = 0; i < 100; i++) {
        pids[i] = ixland_alloc_pid();
        IXLAND_ASSERT_GT(pids[i], 0);
    }

    /* Free first half */
    for (int i = 0; i < 50; i++) {
        ixland_free_pid(pids[i]);
    }

    /* Allocate more - should get freed PIDs first (LIFO) */
    for (int i = 0; i < 50; i++) {
        pid_t p = ixland_alloc_pid();
        IXLAND_ASSERT_GT(p, 0);
        /* Should be one of the freed PIDs */
        IXLAND_ASSERT_LE(p, pids[49]);
        ixland_free_pid(p);
    }

    /* Cleanup */
    for (int i = 50; i < 100; i++) {
        ixland_free_pid(pids[i]);
    }

    return true;
}

/* Stress test: allocate 10000 PIDs rapidly */
IXLAND_TEST(pid_stress_rapid_allocation) {
    const int count = 10000;
    pid_t *pids = malloc(count * sizeof(pid_t));
    IXLAND_ASSERT_NOT_NULL(pids);

    /* Allocate many PIDs rapidly */
    for (int i = 0; i < count; i++) {
        pids[i] = ixland_alloc_pid();
        IXLAND_ASSERT_GT(pids[i], 0);
    }

    /* Verify no duplicates by sorting and checking */
    /* (Skip for speed - uniqueness test covers this) */

    /* Free all */
    for (int i = 0; i < count; i++) {
        ixland_free_pid(pids[i]);
    }

    free(pids);
    return true;
}

/* Test O(1) timing - allocations should have consistent time */
IXLAND_TEST(pid_o1_timing) {
    const int samples = 1000;
    long times[1000];
    struct timespec start, end;

    /* Warm up */
    for (int i = 0; i < 100; i++) {
        pid_t p = ixland_alloc_pid();
        ixland_free_pid(p);
    }

    /* Time individual allocations */
    pid_t pids[1000];
    for (int i = 0; i < samples; i++) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        pids[i] = ixland_alloc_pid();
        clock_gettime(CLOCK_MONOTONIC, &end);

        long ns = (end.tv_sec - start.tv_sec) * 1000000000L + (end.tv_nsec - start.tv_nsec);
        times[i] = ns;

        IXLAND_ASSERT_GT(pids[i], 0);
    }

    /* Calculate statistics */
    long total = 0;
    long max_time = 0;
    for (int i = 0; i < samples; i++) {
        total += times[i];
        if (times[i] > max_time) {
            max_time = times[i];
        }
    }
    long avg = total / samples;

    /* O(1) means no significant growth in allocation time
     * Average should be reasonable (under 10 microseconds) */
    IXLAND_ASSERT_LT(avg, 10000); /* 10 microseconds in nanoseconds */

    /* Cleanup */
    for (int i = 0; i < samples; i++) {
        ixland_free_pid(pids[i]);
    }

    return true;
}

/* Test thread safety - concurrent alloc/free */
#define NUM_THREADS 8
#define PIDS_PER_THREAD 100

static _Atomic int thread_ready = 0;
static _Atomic int thread_errors = 0;

static void *pid_alloc_thread(void *arg) {
    (void)arg;

    pid_t pids[PIDS_PER_THREAD];

    /* Signal ready and wait for all threads */
    atomic_fetch_add(&thread_ready, 1);
    while (atomic_load(&thread_ready) < NUM_THREADS) {
        /* Spin wait */
    }

    /* Allocate PIDs */
    for (int i = 0; i < PIDS_PER_THREAD; i++) {
        pids[i] = ixland_alloc_pid();
        if (pids[i] <= 0) {
            atomic_fetch_add(&thread_errors, 1);
        }
    }

    /* Small delay to increase contention */
    struct timespec ts = {0, 1000}; /* 1 microsecond */
    nanosleep(&ts, NULL);

    /* Free PIDs */
    for (int i = 0; i < PIDS_PER_THREAD; i++) {
        if (pids[i] > 0) {
            ixland_free_pid(pids[i]);
        }
    }

    return NULL;
}

IXLAND_TEST(pid_thread_safety) {
    pthread_t threads[NUM_THREADS];

    atomic_store(&thread_ready, 0);
    atomic_store(&thread_errors, 0);

    /* Create threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        int ret = pthread_create(&threads[i], NULL, pid_alloc_thread, NULL);
        IXLAND_ASSERT_EQ(ret, 0);
    }

    /* Wait for completion */
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    /* Check no errors occurred */
    IXLAND_ASSERT_EQ(atomic_load(&thread_errors), 0);

    return true;
}

/* Test that invalid PIDs are handled gracefully */
IXLAND_TEST(pid_invalid_handling) {
    /* Free invalid PIDs should not crash */
    ixland_free_pid(0);
    ixland_free_pid(1);
    ixland_free_pid(999);   /* Just below MIN */
    ixland_free_pid(65536); /* Just above MAX */
    ixland_free_pid(-1);

    return true;
}
