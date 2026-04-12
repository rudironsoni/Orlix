/* IXLand Path Subsystem Implementation
 * Canonical path classification, normalization, and resolution
 */

#include "path.h"

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
/* iOS Subsystem for Linux - Path Utilities
 *
 * Path resolution, validation, and manipulation
 */

#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/ixland/ixland_path.h"
#include "vfs.h"

/* ============================================================================
 * PATH RESOLUTION
 * ============================================================================ */

int __ixland_path_resolve(const char *path, char *resolved, size_t resolved_len) {
    if (!path || !resolved || resolved_len == 0) {
        errno = EINVAL;
        return -1;
    }

    /* Classify the path */
    ixland_path_type_t type = __ixland_path_classify(path);

    /* If it's a real iOS path (own sandbox or external), use directly */
    if (type == IXLAND_PATH_OWN_SANDBOX || type == IXLAND_PATH_EXTERNAL) {
        if (strlen(path) >= resolved_len) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strncpy(resolved, path, resolved_len - 1);
        resolved[resolved_len - 1] = '\0';
        /* Normalize but don't translate */
        __ixland_path_normalize(resolved);
        return 0;
    }

    /* If it's a virtual Linux path, translate through VFS */
    if (type == IXLAND_PATH_VIRTUAL_LINUX) {
        /* First normalize the virtual path */
        char normalized[IXLAND_MAX_PATH];
        if (strlen(path) >= sizeof(normalized)) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strncpy(normalized, path, sizeof(normalized) - 1);
        normalized[sizeof(normalized) - 1] = '\0';
        __ixland_path_normalize(normalized);

        /* Now translate through VFS */
        /* The VFS translate function expects a virtual path and returns iOS path */
        if (ixland_vfs_translate(normalized, resolved, resolved_len) != 0) {
            /* Translation failed - return the normalized path anyway */
            /* This allows fallback to direct kernel access */
            if (strlen(normalized) >= resolved_len) {
                errno = ENAMETOOLONG;
                return -1;
            }
            strncpy(resolved, normalized, resolved_len - 1);
            resolved[resolved_len - 1] = '\0';
        }
        return 0;
    }

    /* For relative paths or other cases, resolve against CWD */
    char cwd[IXLAND_MAX_PATH];
    if (!getcwd(cwd, sizeof(cwd))) {
        return -1;
    }

    /* Join paths */
    if (strlen(cwd) + strlen(path) + 2 >= resolved_len) {
        errno = ENAMETOOLONG;
        return -1;
    }

    snprintf(resolved, resolved_len, "%s/%s", cwd, path);
    __ixland_path_normalize(resolved);

    /* Now classify the resolved path */
    type = __ixland_path_classify(resolved);
    if (type == IXLAND_PATH_VIRTUAL_LINUX) {
        /* It's a virtual path, translate it */
        char translated[IXLAND_MAX_PATH];
        if (ixland_vfs_translate(resolved, translated, sizeof(translated)) == 0) {
            if (strlen(translated) < resolved_len) {
                strncpy(resolved, translated, resolved_len - 1);
                resolved[resolved_len - 1] = '\0';
            }
        }
    }
    /* Otherwise keep the resolved path as-is (real iOS path) */

    return 0;
}

void __ixland_path_normalize(char *path) {
    if (!path || !*path)
        return;

    char *src = path;
    char *dst = path;

    /* Track if path is absolute */
    bool is_absolute = (*src == '/');

    /* Skip leading slashes */
    while (*src == '/') {
        src++;
    }

    /* Handle root case - if absolute, ensure leading slash */
    if (is_absolute) {
        *dst++ = '/';

        /* Handle special case: /./ at start */
        if (src[0] == '.' && (src[1] == '/' || src[1] == '\0')) {
            src++; /* Skip the dot */
            while (*src == '/')
                src++; /* Skip following slashes */
        }
    }

    while (*src) {
        if (*src == '/') {
            /* Skip multiple slashes */
            while (*src == '/')
                src++;

            /* Check for . or .. */
            if (*src == '.') {
                if (src[1] == '\0' || src[1] == '/') {
                    /* Single dot - skip it */
                    src++;
                    continue;
                } else if (src[1] == '.' && (src[2] == '\0' || src[2] == '/')) {
                    /* Double dot - go up one directory */
                    src += 2;
                    if (dst > path + 1) {
                        /* Find previous slash */
                        dst--;
                        while (dst > path && *dst != '/')
                            dst--;
                        if (dst == path)
                            dst++;
                    }
                    continue;
                }
            }

            /* Add single slash */
            if (dst > path && dst[-1] != '/') {
                *dst++ = '/';
            }
        } else {
            *dst++ = *src++;
        }
    }

    /* Remove trailing slash (unless root) */
    if (dst > path + 1 && dst[-1] == '/') {
        dst--;
    }

    *dst = '\0';
}

void __ixland_path_join(const char *base, const char *rel, char *result, size_t result_len) {
    if (!base || !rel || !result || result_len == 0) {
        if (result && result_len > 0)
            *result = '\0';
        return;
    }

    if (rel[0] == '/') {
        /* rel is absolute */
        if (strlen(rel) >= result_len) {
            if (result_len > 0)
                *result = '\0';
            return;
        }
        strncpy(result, rel, result_len - 1);
        result[result_len - 1] = '\0';
        return;
    }

    /* Join base and rel */
    size_t base_len = strlen(base);
    size_t rel_len = strlen(rel);

    if (base_len + rel_len + 2 > result_len) {
        if (result_len > 0)
            *result = '\0';
        return;
    }

    if (base_len > 0 && base[base_len - 1] == '/') {
        snprintf(result, result_len, "%s%s", base, rel);
    } else {
        snprintf(result, result_len, "%s/%s", base, rel);
    }
}

bool __ixland_path_in_sandbox(const char *path) {
    /* For now, allow all paths - will implement sandbox checking later */
    /* TODO: Implement proper sandbox path validation */
    (void)path;
    return true;
}

/* ============================================================================
 * PATH CLASSIFICATION FOR iOS HYBRID ARCHITECTURE
 *
 * Classifies paths into three categories:
 * 1. Virtual Linux paths (/home, /tmp, /etc) - need VFS translation
 * 2. Own app sandbox paths - direct kernel access
 * 3. External paths (security-scoped) - need special handling
 * ============================================================================ */

#include <regex.h>

/* Compiled regex patterns (initialized on first use) */
static regex_t ios_sandbox_regex;
static regex_t ios_simulator_regex;
static regex_t ios_external_regex;
static bool regex_initialized = false;

/* Initialize regex patterns for path detection */
static void __ixland_path_init_regex(void) {
    if (regex_initialized)
        return;

    /* Device sandbox: /var/mobile/Containers/Data/Application/<UUID>/ */
    regcomp(&ios_sandbox_regex, "^/var/mobile/Containers/Data/Application/[A-Fa-f0-9-]+/",
            REG_EXTENDED | REG_NOSUB);

    /* Simulator sandbox:
     * ~/Library/Developer/CoreSimulator/Devices/<SIM-UUID>/data/Containers/Data/Application/<APP-UUID>/
     */
    regcomp(&ios_simulator_regex,
            "/Library/Developer/CoreSimulator/Devices/[A-Fa-f0-9-]+/data/Containers/Data/"
            "Application/[A-Fa-f0-9-]+/",
            REG_EXTENDED | REG_NOSUB);

    /* External paths: file-provider, iCloud, shared containers */
    regcomp(&ios_external_regex,
            "^(/private/var/mobile/Library/Mobile "
            "Documents/|/private/var/mobile/Containers/Shared/AppGroup/|file-provider://)",
            REG_EXTENDED | REG_NOSUB);

    regex_initialized = true;
}

/* Check if path is a virtual Linux path (needs VFS translation) */
bool __ixland_path_is_virtual_linux(const char *path) {
    if (!path || !*path)
        return false;

    /* Virtual Linux path prefixes */
    if (strncmp(path, "/home/", 6) == 0)
        return true;
    if (strcmp(path, "/home") == 0)
        return true;
    if (strncmp(path, "/tmp/", 5) == 0)
        return true;
    if (strcmp(path, "/tmp") == 0)
        return true;
    if (strncmp(path, "/etc/", 5) == 0)
        return true;
    if (strcmp(path, "/etc") == 0)
        return true;
    if (strncmp(path, "/var/", 5) == 0)
        return true;
    if (strcmp(path, "/var") == 0)
        return true;
    if (strncmp(path, "/usr/", 5) == 0)
        return true;
    if (strcmp(path, "/usr") == 0)
        return true;
    if (strncmp(path, "/bin/", 5) == 0)
        return true;
    if (strcmp(path, "/bin") == 0)
        return true;
    if (strncmp(path, "/sbin/", 6) == 0)
        return true;
    if (strcmp(path, "/sbin") == 0)
        return true;
    if (strncmp(path, "/lib/", 5) == 0)
        return true;
    if (strcmp(path, "/lib") == 0)
        return true;
    if (strncmp(path, "/root/", 6) == 0)
        return true;
    if (strcmp(path, "/root") == 0)
        return true;
    if (strncmp(path, "/dev/", 5) == 0)
        return true;
    if (strcmp(path, "/dev") == 0)
        return true;

    return false;
}

/* Check if path is within own app sandbox */
bool __ixland_path_is_own_sandbox(const char *path) {
    if (!path || !*path)
        return false;

    __ixland_path_init_regex();

    /* Check device sandbox pattern */
    if (regexec(&ios_sandbox_regex, path, 0, NULL, 0) == 0) {
        return true;
    }

    /* Check simulator sandbox pattern */
    if (regexec(&ios_simulator_regex, path, 0, NULL, 0) == 0) {
        return true;
    }

    /* Also check common writable locations that are always accessible */
    /* These are subdirectories that might be passed without full sandbox path */
    if (strstr(path, "/Documents/") || strstr(path, "/Documents")) {
        return true;
    }
    if (strstr(path, "/Library/") || strstr(path, "/Library")) {
        return true;
    }
    if (strstr(path, "/tmp/") || strstr(path, "/tmp")) {
        return true;
    }

    return false;
}

/* Check if path is external (requires security-scoped access) */
bool __ixland_path_is_external(const char *path) {
    if (!path || !*path)
        return false;

    __ixland_path_init_regex();

    /* Check external path patterns */
    if (regexec(&ios_external_regex, path, 0, NULL, 0) == 0) {
        return true;
    }

    /* Check for iCloud Drive patterns */
    if (strstr(path, "/Mobile Documents/")) {
        return true;
    }

    /* Check for shared app groups */
    if (strstr(path, "/Containers/Shared/AppGroup/")) {
        return true;
    }

    /* If it's not virtual Linux and not own sandbox, treat as external */
    if (!__ixland_path_is_virtual_linux(path) && !__ixland_path_is_own_sandbox(path)) {
        /* This is a catch-all for paths we can't categorize */
        /* The iOS kernel will ultimately enforce permissions */
        return true;
    }

    return false;
}

/* Classify a path and return the type */
ixland_path_type_t __ixland_path_classify(const char *path) {
    if (!path || !*path) {
        return IXLAND_PATH_INVALID;
    }

    if (__ixland_path_is_virtual_linux(path)) {
        return IXLAND_PATH_VIRTUAL_LINUX;
    }

    if (__ixland_path_is_own_sandbox(path)) {
        return IXLAND_PATH_OWN_SANDBOX;
    }

    if (__ixland_path_is_external(path)) {
        return IXLAND_PATH_EXTERNAL;
    }

    /* Unknown path type - let iOS kernel decide */
    return IXLAND_PATH_EXTERNAL;
}

/* Convert virtual Linux path to iOS path using VFS */
int __ixland_path_virtual_to_ios(const char *vpath, char *ios_path, size_t ios_path_len) {
    if (!vpath || !ios_path || ios_path_len == 0) {
        return -1;
    }

    /* Check if it's actually a virtual path */
    if (!__ixland_path_is_virtual_linux(vpath)) {
        /* Not a virtual path, copy as-is */
        if (strlen(vpath) >= ios_path_len) {
            errno = ENAMETOOLONG;
            return -1;
        }
        strncpy(ios_path, vpath, ios_path_len - 1);
        ios_path[ios_path_len - 1] = '\0';
        return 0;
    }

    /* Use existing VFS resolution */
    /* This function will be called from VFS layer */
    /* For now, return the path as-is and let VFS handle it */
    if (strlen(vpath) >= ios_path_len) {
        errno = ENAMETOOLONG;
        return -1;
    }
    strncpy(ios_path, vpath, ios_path_len - 1);
    ios_path[ios_path_len - 1] = '\0';

    return 0;
}

/* Check if path can be accessed directly (not virtual) */
bool __ixland_path_is_direct(const char *path) {
    if (!path || !*path)
        return false;

    ixland_path_type_t type = __ixland_path_classify(path);
    return (type == IXLAND_PATH_OWN_SANDBOX || type == IXLAND_PATH_EXTERNAL);
}
