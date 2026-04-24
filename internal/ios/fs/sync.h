#ifndef FS_SYNC_H
#define FS_SYNC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque synchronization types.
 * Implementation uses host pthread primitives in internal/ios/fs/sync.c */
struct fs_mutex_opaque;
struct fs_cond_opaque;

typedef struct fs_mutex_opaque fs_mutex_t;
typedef struct fs_cond_opaque fs_cond_t;

/* Static initializer values */
#define FS_MUTEX_INITIALIZER {(void*)0}
#define FS_COND_INITIALIZER {(void*)0}

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
