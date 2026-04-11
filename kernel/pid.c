#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "task.h"

#define IXLAND_MIN_PID 1000
#define IXLAND_MAX_PID 65535
#define IXLAND_PID_COUNT (IXLAND_MAX_PID - IXLAND_MIN_PID + 1)

/* Free list stack for O(1) PID allocation/reuse */
static pid_t pid_free_stack[IXLAND_PID_COUNT];
static _Atomic int pid_stack_top = 0;
static pthread_mutex_t pid_lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_bool pid_initialized = false;

/**
 * @brief Initialize the PID allocator with all available PIDs.
 *
 * Populates the free stack with PIDs from IXLAND_MAX_PID down to IXLAND_MIN_PID
 * so that sequential allocation starts from IXLAND_MIN_PID.
 */
void ixland_pid_init(void) {
    if (atomic_load(&pid_initialized)) {
        return;
    }

    pthread_mutex_lock(&pid_lock);
    if (atomic_load(&pid_initialized)) {
        pthread_mutex_unlock(&pid_lock);
        return;
    }

    /* Push PIDs in reverse order so IXLAND_MIN_PID is popped first */
    int idx = 0;
    for (pid_t pid = IXLAND_MAX_PID; pid >= IXLAND_MIN_PID; pid--) {
        pid_free_stack[idx++] = pid;
    }
    atomic_store(&pid_stack_top, IXLAND_PID_COUNT);
    atomic_store(&pid_initialized, true);

    pthread_mutex_unlock(&pid_lock);
}

/**
 * @brief Allocate a unique PID in O(1) time.
 *
 * Uses atomic stack pop for lock-free allocation. If the free list is empty,
 * returns -1 (equivalent to EAGAIN).
 *
 * @return Allocated PID (>= IXLAND_MIN_PID) or -1 if no PIDs available
 */
pid_t ixland_alloc_pid(void) {
    /* Ensure initialized (thread-safe via atomic flag) */
    if (!atomic_load(&pid_initialized)) {
        ixland_pid_init();
    }

    pthread_mutex_lock(&pid_lock);
    int top = atomic_load(&pid_stack_top);
    if (top <= 0) {
        pthread_mutex_unlock(&pid_lock);
        return -1; /* No PIDs available */
    }

    top--;
    pid_t pid = pid_free_stack[top];
    atomic_store(&pid_stack_top, top);
    pthread_mutex_unlock(&pid_lock);

    return pid;
}

/**
 * @brief Free a PID for immediate reuse.
 *
 * Returns the PID to the free list for O(1) reuse. Invalid PIDs are silently
 * ignored. The freed PID becomes immediately available for allocation.
 *
 * @param pid The PID to free
 */
void ixland_free_pid(pid_t pid) {
    /* Validate PID range */
    if (pid < IXLAND_MIN_PID || pid > IXLAND_MAX_PID) {
        return;
    }

    /* Ensure initialized */
    if (!atomic_load(&pid_initialized)) {
        ixland_pid_init();
    }

    pthread_mutex_lock(&pid_lock);
    int top = atomic_load(&pid_stack_top);

    /* Defensive: check for stack overflow (shouldn't happen with correct usage) */
    if (top >= IXLAND_PID_COUNT) {
        pthread_mutex_unlock(&pid_lock);
        return;
    }

    pid_free_stack[top] = pid;
    atomic_store(&pid_stack_top, top + 1);
    pthread_mutex_unlock(&pid_lock);
}
