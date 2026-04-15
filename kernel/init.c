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
static atomic_int library_initialized = 0;
static pthread_mutex_t library_init_lock = PTHREAD_MUTEX_INITIALIZER;

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

void library_init_constructor(void) __attribute__((constructor(101))) __attribute__((used));
void library_deinit_destructor(void) __attribute__((destructor)) __attribute__((used));

void library_init_constructor(void) {
/* Check if already initialized */
if (atomic_load(&library_initialized)) {
return;
}

pthread_mutex_lock(&library_init_lock);

/* Double-check after acquiring lock */
if (atomic_load(&library_initialized)) {
pthread_mutex_unlock(&library_init_lock);
return;
}

/* Initialize VFS first (safe initialization) */
int vfs_result = vfs_init();
if (vfs_result != 0) {
/* VFS init failed - continue anyway with defaults */
/* This allows the library to work even if HOME is not set */
}

/* Initialize task system - creates init task */
int task_result = task_init();
if (task_result != 0) {
pthread_mutex_unlock(&library_init_lock);
return;
}

/* Initialize native command registry */
native_registry_init();

/* Set initialized flag */
atomic_store(&library_initialized, 1);

pthread_mutex_unlock(&library_init_lock);
}

void library_deinit_destructor(void) {
if (!atomic_load(&library_initialized)) {
return;
}

task_deinit();
vfs_deinit();

atomic_store(&library_initialized, 0);

if (getenv("IXLAND_DEBUG")) {
fprintf(stderr, "ixland: Library deinitialized\n");
}
}

/* ============================================================================
 * PUBLIC INITIALIZATION API
 * ============================================================================ */

int library_init(const void *config) {
/* Initialization happens automatically via constructor,
 * but this function allows explicit initialization if needed */
(void)config; /* Config ignored for now */
if (!atomic_load(&library_initialized)) {
library_init_constructor();
}
return atomic_load(&library_initialized) ? 0 : -1;
}

int library_is_initialized(void) {
return atomic_load(&library_initialized);
}

const char *library_version(void) {
return "1.0.0";
}

void library_deinit(void) {
library_deinit_destructor();
}
