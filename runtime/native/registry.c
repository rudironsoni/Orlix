#include "registry.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static ixland_native_cmd_t *registry_head = NULL;
static pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;

void ixland_native_registry_init(void) {
    /* Registry is lazy-initialized on first register call */
}

void ixland_native_registry_clear(void) {
    pthread_mutex_lock(&registry_lock);
    ixland_native_cmd_t *cmd = registry_head;
    while (cmd) {
        ixland_native_cmd_t *next = cmd->next;
        free(cmd);
        cmd = next;
    }
    registry_head = NULL;
    pthread_mutex_unlock(&registry_lock);
}

int ixland_native_register(const char *path, ixland_native_entry_t entry) {
    if (!path || !entry) {
        return -1;
    }

    ixland_native_cmd_t *cmd = malloc(sizeof(ixland_native_cmd_t));
    if (!cmd) {
        return -1;
    }

    cmd->path = path;
    cmd->entry = entry;

    pthread_mutex_lock(&registry_lock);
    cmd->next = registry_head;
    registry_head = cmd;
    pthread_mutex_unlock(&registry_lock);

    return 0;
}

ixland_native_entry_t ixland_native_lookup(const char *path) {
    if (!path) {
        return NULL;
    }

    pthread_mutex_lock(&registry_lock);
    ixland_native_cmd_t *cmd = registry_head;
    while (cmd) {
        if (strcmp(cmd->path, path) == 0) {
            ixland_native_entry_t entry = cmd->entry;
            pthread_mutex_unlock(&registry_lock);
            return entry;
        }
        cmd = cmd->next;
    }
    pthread_mutex_unlock(&registry_lock);

    return NULL;
}
