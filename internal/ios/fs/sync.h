#ifndef FS_SYNC_H
#define FS_SYNC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Concrete synchronization types with storage-sized wrappers.
 * Implementation uses host pthread primitives internally.
 * These types can be stored by value in Linux-owner structures. */

#define FS_MUTEX_STORAGE_SIZE 64
#define FS_COND_STORAGE_SIZE 64

typedef struct fs_mutex {
    char _storage[FS_MUTEX_STORAGE_SIZE];
    int _initialized;
} fs_mutex_t;

typedef struct fs_cond {
    char _storage[FS_COND_STORAGE_SIZE];
    int _initialized;
} fs_cond_t;

/* Static initializer values */
#define FS_MUTEX_INITIALIZER {{0}, 0}
#define FS_COND_INITIALIZER {{0}, 0}

/* Mutex operations */
int fs_mutex_init(fs_mutex_t *mutex);
int fs_mutex_destroy(fs_mutex_t *mutex);
int fs_mutex_lock(fs_mutex_t *mutex);
int fs_mutex_unlock(fs_mutex_t *mutex);

/* Condition variable operations */
int fs_cond_init(fs_cond_t *cond);
int fs_cond_destroy(fs_cond_t *cond);
int fs_cond_wait(fs_cond_t *cond, fs_mutex_t *mutex);
int fs_cond_broadcast(fs_cond_t *cond);

#ifdef __cplusplus
}
#endif

#endif
