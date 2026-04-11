/* IXLand Types Header
 * Canonical type definitions for IXLand virtual kernel subsystem
 */

#ifndef IXLAND_TYPES_H
#define IXLAND_TYPES_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* Forward declarations for IXLand core structures */
typedef struct ixland_task ixland_task_t;
typedef struct ixland_files ixland_files_t;
typedef struct ixland_fs ixland_fs_t;
typedef struct ixland_sighand ixland_sighand_t;

/* IXLand error codes */
#define IXLAND_OK 0
#define IXLAND_ERR_NOMEM -1
#define IXLAND_ERR_INVAL -2
#define IXLAND_ERR_AGAIN -3

/* IXLand limits */
#define IXLAND_MAX_PATH 4096
#define IXLAND_MAX_FD 256
#define IXLAND_MAX_ARGS 256
#define IXLAND_MAX_TASKS 1024
#define IXLAND_MAX_NAME 256
#define IXLAND_NSIG 64

/* IXLand directory entry (Linux dirent64 compatible) */
struct ixland_dirent_64 {
    uint64_t d_ino;
    int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

/* IXLand initialization configuration (system-level, minimal) */
typedef struct {
    int debug_enabled;
    const char *home_directory;
    const char *tmp_directory;
} ixland_config_t;

#endif /* IXLAND_TYPES_H */
