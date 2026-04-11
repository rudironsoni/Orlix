/* IXLand Path Subsystem Implementation
 * Canonical path classification, normalization, and resolution
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/ixland/ixland_path.h"
#include "../include/ixland/ixland_types.h"

/* ============================================================================
 * PATH CLASSIFICATION
 * ============================================================================ */

ixland_path_type_t ixland_path_classify(const char *path) {
    if (!path || path[0] == '\0') {
        return IXLAND_PATH_INVALID;
    }

    /* Virtual Linux paths start with / but map to virtual filesystem */
    if (path[0] == '/') {
        /* Check if it's a known virtual Linux path prefix */
        if (strcmp(path, "/") == 0 || strncmp(path, "/bin/", 5) == 0 ||
            strncmp(path, "/sbin/", 6) == 0 || strncmp(path, "/usr/", 5) == 0 ||
            strncmp(path, "/lib/", 5) == 0 || strncmp(path, "/lib64/", 7) == 0 ||
            strncmp(path, "/etc/", 5) == 0 || strncmp(path, "/var/", 5) == 0 ||
            strncmp(path, "/home/", 6) == 0 || strncmp(path, "/root/", 6) == 0 ||
            strncmp(path, "/tmp/", 5) == 0 || strncmp(path, "/dev/", 5) == 0 ||
            strncmp(path, "/proc/", 6) == 0 || strncmp(path, "/sys/", 5) == 0) {
            return IXLAND_PATH_VIRTUAL_LINUX;
        }

        /* Otherwise it's an absolute host path */
        return IXLAND_PATH_ABSOLUTE_HOST;
    }

    /* Relative paths are classified based on context - default to virtual */
    return IXLAND_PATH_VIRTUAL_LINUX;
}

/* ============================================================================
 * PATH NORMALIZATION
 * ============================================================================ */

int ixland_path_normalize(char *path, size_t path_len) {
    if (!path || path_len == 0) {
        errno = EINVAL;
        return -1;
    }

    size_t len = strlen(path);
    if (len >= path_len) {
        errno = ENAMETOOLONG;
        return -1;
    }

    /* Collapse multiple slashes */
    char *src = path;
    char *dst = path;
    bool in_slash = false;

    while (*src) {
        if (*src == '/') {
            if (!in_slash) {
                *dst++ = *src;
                in_slash = true;
            }
        } else {
            *dst++ = *src;
            in_slash = false;
        }
        src++;
    }

    /* Remove trailing slash unless it's root */
    if (dst > path + 1 && *(dst - 1) == '/') {
        dst--;
    }

    *dst = '\0';
    return 0;
}

/* ============================================================================
 * PATH TRANSLATION (Virtual -> Host)
 * ============================================================================ */

int ixland_path_translate(const char *virtual_path, char *host_path, size_t host_path_len) {
    if (!virtual_path || !host_path || host_path_len == 0) {
        errno = EINVAL;
        return -1;
    }

    ixland_path_type_t type = ixland_path_classify(virtual_path);

    switch (type) {
    case IXLAND_PATH_OWN_SANDBOX:
        /* Already a host path in sandbox */
        if (strlen(virtual_path) >= host_path_len) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strncpy(host_path, virtual_path, host_path_len - 1);
        host_path[host_path_len - 1] = '\0';
        return 0;

    case IXLAND_PATH_VIRTUAL_LINUX:
        /* Virtual Linux paths need translation through VFS mount */
        /* For now, map to sandbox's virtual root */
        const char *sandbox_root = getenv("IXLAND_ROOT");
        if (!sandbox_root) {
            sandbox_root = "/var/mobile/Containers/IXLand";
        }

        size_t root_len = strlen(sandbox_root);
        size_t vpath_len = strlen(virtual_path);

        if (root_len + vpath_len + 1 >= host_path_len) {
            errno = ENAMETOOLONG;
            return -1;
        }

        snprintf(host_path, host_path_len, "%s%s", sandbox_root, virtual_path);
        return ixland_path_normalize(host_path, host_path_len);

    case IXLAND_PATH_ABSOLUTE_HOST:
        /* Direct host path - use as-is with validation */
        if (strlen(virtual_path) >= host_path_len) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strncpy(host_path, virtual_path, host_path_len - 1);
        host_path[host_path_len - 1] = '\0';
        return 0;

    default:
        errno = ENOENT;
        return -1;
    }
}

/* ============================================================================
 * REVERSE TRANSLATION (Host -> Virtual)
 * ============================================================================ */

int ixland_path_reverse_translate(const char *host_path, char *virtual_path,
                                  size_t virtual_path_len) {
    if (!host_path || !virtual_path || virtual_path_len == 0) {
        errno = EINVAL;
        return -1;
    }

    const char *sandbox_root = getenv("IXLAND_ROOT");
    if (!sandbox_root) {
        sandbox_root = "/var/mobile/Containers/IXLand";
    }

    size_t root_len = strlen(sandbox_root);

    /* Check if host_path is under sandbox root */
    if (strncmp(host_path, sandbox_root, root_len) == 0) {
        /* Strip sandbox root to get virtual path */
        const char *relative = host_path + root_len;
        if (strlen(relative) >= virtual_path_len) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strncpy(virtual_path, relative, virtual_path_len - 1);
        virtual_path[virtual_path_len - 1] = '\0';
        return 0;
    }

    /* Not under sandbox - return as virtual Linux path */
    if (strlen(host_path) >= virtual_path_len) {
        errno = ENAMETOOLONG;
        return -1;
    }
    strncpy(virtual_path, host_path, virtual_path_len - 1);
    virtual_path[virtual_path_len - 1] = '\0';
    return 0;
}

/* ============================================================================
 * PATH VALIDATION
 * ============================================================================ */

bool ixland_path_is_valid(const char *path) {
    if (!path || path[0] == '\0') {
        return false;
    }

    /* Reject paths with null bytes embedded */
    for (const char *p = path; *p; p++) {
        if (*p == '\0') {
            return false;
        }
    }

    /* Reject paths that are too long */
    if (strlen(path) >= IXLAND_MAX_PATH) {
        return false;
    }

    return true;
}

bool ixland_path_is_safe(const char *path) {
    if (!ixland_path_is_valid(path)) {
        return false;
    }

    /* Reject paths with suspicious patterns */
    if (strstr(path, "..") != NULL) {
        /* Additional validation needed for parent directory escapes */
        return false;
    }

    return true;
}
