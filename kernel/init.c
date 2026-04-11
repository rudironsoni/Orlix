/* iOS Subsystem for Linux - Library Initialization
 *
 * Automatic initialization using constructor attribute
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../fs/vfs.h"
#include "../runtime/native/registry.h"
#include "task.h"

/* Global initialization flag */
static atomic_int ixland_initialized = 0;
static pthread_mutex_t ixland_init_lock = PTHREAD_MUTEX_INITIALIZER;

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void ixland_library_init(void) __attribute__((constructor(101))) __attribute__((used));
void ixland_library_deinit(void) __attribute__((destructor)) __attribute__((used));

void ixland_library_init(void) {
    /* Check if already initialized */
    if (atomic_load(&ixland_initialized)) {
        return;
    }

    pthread_mutex_lock(&ixland_init_lock);

    /* Double-check after acquiring lock */
    if (atomic_load(&ixland_initialized)) {
        pthread_mutex_unlock(&ixland_init_lock);
        return;
    }

    /* Initialize VFS first (safe initialization) */
    int vfs_result = ixland_vfs_init();
    if (vfs_result != 0) {
        /* VFS init failed - continue anyway with defaults */
        /* This allows the library to work even if HOME is not set */
    }

    /* Initialize task system - creates init task */
    int task_result = ixland_task_init();
    if (task_result != 0) {
        pthread_mutex_unlock(&ixland_init_lock);
        return;
    }

    /* Initialize native command registry */
    ixland_native_registry_init();

    /* Set initialized flag */
    atomic_store(&ixland_initialized, 1);

    pthread_mutex_unlock(&ixland_init_lock);
}

void ixland_library_deinit(void) {
    if (!atomic_load(&ixland_initialized)) {
        return;
    }

    ixland_task_deinit();
    ixland_vfs_deinit();

    atomic_store(&ixland_initialized, 0);

    if (getenv("IXLAND_DEBUG")) {
        fprintf(stderr, "ixland: Library deinitialized\n");
    }
}

/* ============================================================================
 * PUBLIC INITIALIZATION API
 * ============================================================================ */

int ixland_init(const ixland_config_t *config) {
    /* Initialization happens automatically via constructor,
     * but this function allows explicit initialization if needed */
    (void)config; /* Config ignored for now */
    if (!atomic_load(&ixland_initialized)) {
        ixland_library_init();
    }
    return atomic_load(&ixland_initialized) ? 0 : -1;
}

int ixland_is_initialized(void) {
    return atomic_load(&ixland_initialized);
}

const char *ixland_version(void) {
    return "1.0.0";
}
