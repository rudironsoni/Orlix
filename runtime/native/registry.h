#ifndef IXLAND_NATIVE_REGISTRY_H
#define IXLAND_NATIVE_REGISTRY_H

#include <stdbool.h>

#include "../../kernel/task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Native command entry point signature */
typedef int (*ixland_native_entry_t)(ixland_task_t *task, int argc, char **argv, char **envp);

/* Registry entry */
typedef struct ixland_native_cmd {
    const char *path;            /* Command path (e.g., "/bin/testcmd") */
    ixland_native_entry_t entry; /* Entry point */
    struct ixland_native_cmd *next;
} ixland_native_cmd_t;

/* Registry operations */
int ixland_native_register(const char *path, ixland_native_entry_t entry);
ixland_native_entry_t ixland_native_lookup(const char *path);
void ixland_native_registry_init(void);
void ixland_native_registry_clear(void);

#ifdef __cplusplus
}
#endif

#endif
