/* IXLand Path Subsystem Header
 * Canonical path classification, normalization, and resolution
 */

#ifndef IXLAND_PATH_H
#define IXLAND_PATH_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Path type classification */
typedef enum {
    IXLAND_PATH_INVALID = 0,
    IXLAND_PATH_OWN_SANDBOX = 1,   /* App's sandbox paths */
    IXLAND_PATH_EXTERNAL = 2,      /* User-granted external paths */
    IXLAND_PATH_VIRTUAL_LINUX = 3, /* Virtual Linux paths (/, /bin, /usr, etc.) */
    IXLAND_PATH_ABSOLUTE_HOST = 4  /* Absolute iOS paths outside sandbox */
} ixland_path_type_t;

/* Path classification contract */
ixland_path_type_t ixland_path_classify(const char *path);

/* Path normalization contract */
int ixland_path_normalize(char *path, size_t path_len);

/* Path resolution: virtual -> host translation */
int ixland_path_translate(const char *virtual_path, char *host_path, size_t host_path_len);

/* Path resolution: host -> virtual reverse translation */
int ixland_path_reverse_translate(const char *host_path, char *virtual_path,
                                  size_t virtual_path_len);

/* Path validation */
bool ixland_path_is_valid(const char *path);
bool ixland_path_is_safe(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* IXLAND_PATH_H */
