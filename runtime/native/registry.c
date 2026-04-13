#include "registry.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static native_cmd_t *registry_head = NULL;
static pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;

void native_registry_init(void) {
    /* Registry is lazy-initialized on first register call */
}

void native_registry_clear(void) {
    pthread_mutex_lock(&registry_lock);
    native_cmd_t *cmd = registry_head;
    while (cmd) {
        native_cmd_t *next = cmd->next;
        free(cmd);
        cmd = next;
    }
    registry_head = NULL;
    pthread_mutex_unlock(&registry_lock);
}

int native_register(const char *path, native_entry_fn entry) {
    if (!path || !entry) {
        return -1;
    }

    native_cmd_t *cmd = malloc(sizeof(native_cmd_t));
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

native_entry_fn native_lookup(const char *path) {
    if (!path) {
        return NULL;
    }

    pthread_mutex_lock(&registry_lock);
    native_cmd_t *cmd = registry_head;
    while (cmd) {
        if (strcmp(cmd->path, path) == 0) {
            native_entry_fn entry = cmd->entry;
            pthread_mutex_unlock(&registry_lock);
            return entry;
        }
        cmd = cmd->next;
    }
    pthread_mutex_unlock(&registry_lock);

    return NULL;
}
