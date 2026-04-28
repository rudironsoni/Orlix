/* IXLandSystemTests/LinuxUAPITestSupport.c
 * Semantic test helpers for Linux UAPI-sensitive assertions
 *
 * This file implements semantic helpers that interpret Linux UAPI values.
 * Tests use these for behavior assertions, not constant accessors.
 *
 * ALLOWED: Semantic interpretation helpers (e.g., is_directory(mode))
 * FORBIDDEN: Constant accessor soup (e.g., sigusr1(), at_nofollow())
 */

#include <linux/stat.h>
#include <linux/fcntl.h>

/* ============================================================================
 * Stat mode semantic helpers - interpret mode values
 * ============================================================================ */

int ixland_test_uapi_mode_is_directory(unsigned int mode) {
    return S_ISDIR(mode);
}

int ixland_test_uapi_mode_is_symlink(unsigned int mode) {
    return S_ISLNK(mode);
}

int ixland_test_uapi_mode_is_regular(unsigned int mode) {
    return S_ISREG(mode);
}

int ixland_test_uapi_mode_is_char_device(unsigned int mode) {
    return S_ISCHR(mode);
}

int ixland_test_uapi_mode_is_block_device(unsigned int mode) {
    return S_ISBLK(mode);
}

int ixland_test_uapi_mode_is_fifo(unsigned int mode) {
    return S_ISFIFO(mode);
}

/* ============================================================================
 * AT_* flag semantic helpers - Linux UAPI constants
 * ============================================================================ */

int ixland_test_uapi_at_symlink_nofollow(void) {
    return AT_SYMLINK_NOFOLLOW;
}

int ixland_test_uapi_at_eaccess(void) {
    return AT_EACCESS;
}

int ixland_test_uapi_at_empty_path(void) {
    return AT_EMPTY_PATH;
}

/* ============================================================================
 * Fcntl flag semantic helpers - Linux UAPI constants
 * ============================================================================ */

int ixland_test_uapi_f_dupfd_cloexec(void) {
    return F_DUPFD_CLOEXEC;
}
