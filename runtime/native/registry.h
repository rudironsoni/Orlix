#ifndef NATIVE_REGISTRY_H
#define NATIVE_REGISTRY_H

#include <stdbool.h>

#include "../../kernel/task.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*native_entry_fn)(struct task_struct *task, int argc, char **argv, char **envp);

typedef struct native_cmd {
    const char *path;
    native_entry_fn entry;
    struct native_cmd *next;
} native_cmd_t;

int native_register(const char *path, native_entry_fn entry);
native_entry_fn native_lookup(const char *path);
void native_registry_init(void);
void native_registry_clear(void);

#ifdef __cplusplus
}
#endif

#endif