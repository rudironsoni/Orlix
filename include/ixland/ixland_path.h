/* Path Subsystem Header
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
    PATH_INVALID = 0,
    PATH_OWN_SANDBOX = 1,
    PATH_EXTERNAL = 2,
    PATH_VIRTUAL_LINUX = 3,
    PATH_ABSOLUTE_HOST = 4
} path_type_t;

/* Path classification contract */
path_type_t path_classify(const char *path);

/* Path normalization contract */
int path_normalize(char *path, size_t path_len);

/* Path resolution: virtual -> host translation */
int path_translate(const char *virtual_path, char *host_path, size_t host_path_len);

/* Path resolution: host -> virtual reverse translation */
int path_reverse_translate(const char *host_path, char *virtual_path,
                           size_t virtual_path_len);

/* Path validation */
bool path_is_valid(const char *path);
bool path_is_safe(const char *path);

/* Legacy aliases for backward compatibility - to be removed later */
#define IXLAND_PATH_INVALID PATH_INVALID
#define IXLAND_PATH_OWN_SANDBOX PATH_OWN_SANDBOX
#define IXLAND_PATH_EXTERNAL PATH_EXTERNAL
#define IXLAND_PATH_VIRTUAL_LINUX PATH_VIRTUAL_LINUX
#define IXLAND_PATH_ABSOLUTE_HOST PATH_ABSOLUTE_HOST
typedef path_type_t ixland_path_type_t;
#define ixland_path_classify path_classify
#define ixland_path_normalize path_normalize
#define ixland_path_translate path_translate
#define ixland_path_reverse_translate path_reverse_translate
#define ixland_path_is_valid path_is_valid
#define ixland_path_is_safe path_is_safe

#ifdef __cplusplus
}
#endif

#endif /* IXLAND_PATH_H */
