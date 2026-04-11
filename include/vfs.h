/*
 * vfs.h - Virtual Filesystem Layer
 *
 * Path translation: /etc/passwd -> $PREFIX/etc/passwd
 */

#ifndef VFS_H
#define VFS_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * VFS Initialization
 * ============================================================================ */

void vfs_init(const char *prefix);
const char *vfs_get_prefix(void);

/* ============================================================================
 * Path Translation
 * ============================================================================ */

/* Translate a path (caller must free returned string with vfs_free_path) */
char *vfs_translate_path(const char *path);

/* Free a path returned by vfs_translate_path */
void vfs_free_path(char *path);

/* Check if a path needs translation */
int vfs_path_needs_translation(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* VFS_H */
