/* IXLand Path Subsystem Internal Header
 * Private declarations for path owner
 */

#ifndef IXLAND_PATH_PRIVATE_H
#define IXLAND_PATH_PRIVATE_H

#include <stdbool.h>
#include <stddef.h>

#include "../include/ixland/ixland_path.h"

/* Internal path helpers used within fs/ */
ixland_path_type_t path_classify(const char *path);
void path_normalize(char *path);
int path_resolve(const char *path, char *resolved, size_t resolved_len);
void path_join(const char *base, const char *rel, char *result, size_t result_len);
bool path_in_sandbox(const char *path);
bool path_is_virtual_linux(const char *path);
bool path_is_own_sandbox(const char *path);
bool path_is_external(const char *path);
int path_virtual_to_ios(const char *vpath, char *ios_path, size_t ios_path_len);
bool path_is_direct(const char *path);

#endif
