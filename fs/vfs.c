/* Linux UAPI constants FIRST - before any Darwin headers */
#include "include/ixland/linux_abi_constants.h"

#include "vfs.h"
#include "internal/ios/fs/backing_io_decls.h"
#include "internal/ios/fs/sync.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fdtable.h"
#include "pty.h"
#include "../kernel/task.h"
#include "../kernel/cred_internal.h"

static const char *vfs_virtual_root_path = "/";

static int vfs_ensure_backing_initialized(void);

/* Backing storage class roots - discovered at runtime from host container */
static char vfs_persistent_root[MAX_PATH] = {0};
static char vfs_cache_root[MAX_PATH] = {0};
static char vfs_temp_root[MAX_PATH] = {0};
static int vfs_backing_initialized = 0;
static int vfs_etc_bootstrapped = 0;

struct vfs_route_entry {
    enum vfs_route_identity route_id;
    const char *linux_prefix;
    enum vfs_backing_class backing_class;
    bool synthetic;
    bool strip_linux_prefix;
    const char *reverse_linux_prefix;
};

static const struct vfs_route_entry vfs_route_table[] = {
    {VFS_ROUTE_VAR_CACHE, "/var/cache", VFS_BACKING_CACHE, false, true, "/var/cache"},
    {VFS_ROUTE_TMP, "/tmp", VFS_BACKING_TEMP, false, true, "/tmp"},
    {VFS_ROUTE_VAR_TMP, "/var/tmp", VFS_BACKING_TEMP, false, true, NULL},
    {VFS_ROUTE_RUN, "/run", VFS_BACKING_TEMP, false, true, NULL},
    {VFS_ROUTE_PROC, "/proc", VFS_BACKING_SYNTHETIC, true, false, NULL},
    {VFS_ROUTE_SYS, "/sys", VFS_BACKING_SYNTHETIC, true, false, NULL},
    {VFS_ROUTE_DEV, "/dev", VFS_BACKING_SYNTHETIC, true, false, NULL},
    {VFS_ROUTE_ETC, "/etc", VFS_BACKING_PERSISTENT, false, false, "/etc"},
    {VFS_ROUTE_USR, "/usr", VFS_BACKING_PERSISTENT, false, false, "/usr"},
    {VFS_ROUTE_VAR_LIB, "/var/lib", VFS_BACKING_PERSISTENT, false, false, "/var/lib"},
    {VFS_ROUTE_HOME, "/home", VFS_BACKING_PERSISTENT, false, false, "/home"},
    {VFS_ROUTE_ROOT_HOME, "/root", VFS_BACKING_PERSISTENT, false, false, "/root"},
    {VFS_ROUTE_PERSISTENT_ROOT, "/", VFS_BACKING_PERSISTENT, false, false, "/"},
};

static const size_t vfs_route_table_count = sizeof(vfs_route_table) / sizeof(vfs_route_table[0]);

static int vfs_copy_string(const char *src, char *dst, size_t dst_len) {
    size_t len;

    if (!src || !dst || dst_len == 0) {
        return -EINVAL;
    }

    len = strlen(src);
    if (len >= dst_len) {
        return -ENAMETOOLONG;
    }

    memcpy(dst, src, len + 1);
    return 0;
}



int vfs_normalize_linux_path(const char *input, char *output, size_t output_len) {
    char scratch[MAX_PATH];
    size_t input_len;
    size_t normalized_len;

    if (!input || !output || output_len == 0) {
