/* Path Subsystem Internal Header
 * Private declarations for path owner
 */

#ifndef PATH_PRIVATE_H
#define PATH_PRIVATE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    PATH_INVALID = 0,
    PATH_OWN_SANDBOX = 1,
    PATH_EXTERNAL = 2,
    PATH_VIRTUAL_LINUX = 3,
    PATH_ABSOLUTE_HOST = 4
} path_type_t;

/* Internal path helpers used within fs/ */
path_type_t path_classify(const char *path);
void path_normalize(char *path);
int path_normalize_with_len(char *path, size_t path_len);
int path_translate(const char *virtual_path, char *host_path, size_t host_path_len);
int path_reverse_translate(const char *host_path, char *virtual_path, size_t virtual_path_len);
bool path_is_valid(const char *path);
bool path_is_safe(const char *path);
int path_resolve(const char *path, char *resolved, size_t resolved_len);
void path_join(const char *base, const char *rel, char *result, size_t result_len);
bool path_in_sandbox(const char *path);
bool path_is_virtual_linux(const char *path);
bool path_is_own_sandbox(const char *path);
bool path_is_external(const char *path);
int path_virtual_to_ios(const char *vpath, char *ios_path, size_t ios_path_len);
bool path_is_direct(const char *path);

#endif
