/* IXLand Path Subsystem Internal Header
 * Private declarations for path owner
 */

#ifndef IXLAND_PATH_PRIVATE_H
#define IXLAND_PATH_PRIVATE_H

#include <stdbool.h>
#include <stddef.h>

#include "../include/ixland/ixland_path.h"

/* Internal path helpers used within fs/ */
ixland_path_type_t __ixland_path_classify(const char *path);
void __ixland_path_normalize(char *path);
int __ixland_path_resolve(const char *path, char *resolved, size_t resolved_len);
void __ixland_path_join(const char *base, const char *rel, char *result, size_t result_len);
bool __ixland_path_in_sandbox(const char *path);
bool __ixland_path_is_virtual_linux(const char *path);
bool __ixland_path_is_own_sandbox(const char *path);
bool __ixland_path_is_external(const char *path);
int __ixland_path_virtual_to_ios(const char *vpath, char *ios_path, size_t ios_path_len);
bool __ixland_path_is_direct(const char *path);

#endif
