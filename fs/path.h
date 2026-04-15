#ifndef PATH_H
#define PATH_H

#include <sys/types.h>

#define MAX_PATH 4096

#ifdef __cplusplus
extern "C" {
#endif

int path_init(void);
void path_deinit(void);

int path_resolve(const char *path, char *resolved, size_t resolved_len);
int path_normalize(const char *path, char *normalized, size_t normalized_len);
int path_join(const char *base, const char *name, char *result, size_t result_len);

#ifdef __cplusplus
}
#endif

#endif