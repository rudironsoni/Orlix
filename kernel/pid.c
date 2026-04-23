#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "task.h"

#define PID_MIN 1000
#define PID_MAX 65535
#define PID_COUNT (PID_MAX - PID_MIN + 1)

/* Free list stack for O(1) PID allocation/reuse */
static pid_t pid_free_stack[PID_COUNT];
static _Atomic int pid_stack_top = 0;
static ix_mutex_t pid_lock = IX_MUTEX_INITIALIZER;
static atomic_bool pid_initialized = false;

/* Private implementation - matches _impl() suffix convention */
static int32_t pid_alloc_impl(void) {
    /* Ensure initialized (thread-safe via atomic flag) */
    if (!atomic_load(&pid_initialized)) {
        pid_init();
    }

    ix_mutex_lock_impl(&pid_lock);
    int top = atomic_load(&pid_stack_top);
    if (top <= 0) {
        ix_mutex_unlock_impl(&pid_lock);
        return -1; /* No PIDs available */
    }

    top--;
    pid_t pid = pid_free_stack[top];
    atomic_store(&pid_stack_top, top);
    ix_mutex_unlock_impl(&pid_lock);

    return (int32_t)pid;
}

static void pid_free_impl(int32_t pid) {
    /* Validate PID range */
    if (pid < PID_MIN || pid > PID_MAX) {
        return;
    }

    /* Ensure initialized */
    if (!atomic_load(&pid_initialized)) {
        pid_init();
    }

    ix_mutex_lock_impl(&pid_lock);
    int top = atomic_load(&pid_stack_top);

    /* Defensive: check for stack overflow (shouldn't happen with correct usage) */
    if (top >= PID_COUNT) {
        ix_mutex_unlock_impl(&pid_lock);
        return;
    }

    pid_free_stack[top] = pid;
    atomic_store(&pid_stack_top, top + 1);
    ix_mutex_unlock_impl(&pid_lock);
}

/**
 * @brief Initialize the PID allocator with all available PIDs.
 *
 * Populates the free stack with PIDs from PID_MAX down to PID_MIN
 * so that sequential allocation starts from PID_MIN.
 */
void pid_init(void) {
    if (atomic_load(&pid_initialized)) {
        return;
    }

    ix_mutex_lock_impl(&pid_lock);
    if (atomic_load(&pid_initialized)) {
        ix_mutex_unlock_impl(&pid_lock);
        return;
    }

    /* Push PIDs in reverse order so PID_MIN is popped first */
    int idx = 0;
    for (pid_t pid = PID_MAX; pid >= PID_MIN; pid--) {
        pid_free_stack[idx++] = pid;
    }
    atomic_store(&pid_stack_top, PID_COUNT);
    atomic_store(&pid_initialized, true);

    ix_mutex_unlock_impl(&pid_lock);
}

/* Public wrappers declared in task.h */
int32_t alloc_pid(void) {
    return pid_alloc_impl();
}

void free_pid(int32_t pid) {
    pid_free_impl(pid);
}
