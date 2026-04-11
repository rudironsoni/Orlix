#include <errno.h>
#include <string.h>

#include "../harness/ixland_test.h"

/* Direct declaration of function under test - internal API */
extern void __ixland_path_normalize(char *path);

/* String comparison helper for test assertions */
#define IXLAND_ASSERT_STR_EQ(actual, expected)                                                   \
    do {                                                                                         \
        if (strcmp((actual), (expected)) != 0) {                                                 \
            fprintf(stderr, "FAIL: %s:%d: string mismatch: got '%s', expected '%s'\n", __FILE__, \
                    __LINE__, (actual), (expected));                                             \
            return false;                                                                        \
        }                                                                                        \
    } while (0)

#ifndef IXLAND_MAX_PATH
#define IXLAND_MAX_PATH 4096
#endif

/* Path Normalization Tests
 *
 * This unit tests ONLY the __ixland_path_normalize() seam.
 * It does NOT test:
 * - path classification (VIRTUAL_LINUX vs EXTERNAL)
 * - VFS translation routing
 * - CWD-based resolution
 * - root clamping (BLOCKED_NOT_IMPLEMENTED)
 *
 * These tests call __ixland_path_normalize() directly to isolate
 * the pure normalization behavior without crossing into
 * classification or translation seams.
 */

#define TEST_PATH_LEN 4096

IXLAND_TEST(path_normalize_absolute_basic) {
    char path[TEST_PATH_LEN];

    /* Already normalized absolute path stays the same */
    strncpy(path, "/home/user/file.txt", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/home/user/file.txt");

    /* Simple absolute path */
    strncpy(path, "/usr/bin", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/usr/bin");

    return true;
}

IXLAND_TEST(path_normalize_dot_segments) {
    char path[TEST_PATH_LEN];

    /* Single dot in middle is removed */
    strncpy(path, "/home/./user", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/home/user");

    /* Multiple dots */
    strncpy(path, "/home/././user", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/home/user");

    /* Dot at end */
    strncpy(path, "/home/user/.", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/home/user");

    /* Dot at start after slash */
    strncpy(path, "/./home/user", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/home/user");

    return true;
}

IXLAND_TEST(path_normalize_dotdot_segments) {
    char path[TEST_PATH_LEN];

    /* Dotdot in middle backs up one directory */
    strncpy(path, "/home/user/../other", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/home/other");

    /* Multiple dotdot segments */
    strncpy(path, "/a/b/c/../../d", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/a/d");

    /* Dotdot backing up multiple levels */
    strncpy(path, "/a/b/../c/../d", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/a/d");

    return true;
}

IXLAND_TEST(path_normalize_repeated_slashes) {
    char path[TEST_PATH_LEN];

    /* Double slash collapses */
    strncpy(path, "/home//user", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/home/user");

    /* Multiple slashes */
    strncpy(path, "/home///user////file", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/home/user/file");

    /* Leading slashes collapse to one */
    strncpy(path, "///home/user", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/home/user");

    return true;
}

IXLAND_TEST(path_normalize_trailing_slash) {
    char path[TEST_PATH_LEN];

    /* Trailing slash removed */
    strncpy(path, "/home/user/", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/home/user");

    /* Multiple trailing slashes */
    strncpy(path, "/home/user//", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/home/user");

    return true;
}

IXLAND_TEST(path_normalize_root_preserved) {
    char path[TEST_PATH_LEN];

    /* Root stays as root */
    strncpy(path, "/", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/");

    /* Multiple slashes to root stays as root */
    strncpy(path, "///", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/");

    return true;
}

IXLAND_TEST(path_normalize_null_safe) {
    /* Null path should not crash */
    __ixland_path_normalize(NULL);

    /* Empty path should be handled */
    char path[TEST_PATH_LEN] = "";
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "");

    return true;
}

IXLAND_TEST(path_normalize_complex_combinations) {
    char path[TEST_PATH_LEN];

    /* Complex path with all features */
    strncpy(path, "/a//b/./c/../d/./e//", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/a/b/d/e");

    /* Deep nesting with dotdot */
    strncpy(path, "/a/b/c/d/../../../x", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/a/x");

    /* Mixed dots and slashes */
    strncpy(path, "//a/./b//../c/./d//", sizeof(path));
    __ixland_path_normalize(path);
    IXLAND_ASSERT_STR_EQ(path, "/a/c/d");

    return true;
}

/* Note: Tests for the following are BLOCKED_NOT_IMPLEMENTED:
 * - Relative path resolution against CWD (requires __ixland_path_resolve, crosses classification
 * seam)
 * - CWD change effects (requires __ixland_path_resolve, crosses classification seam)
 * - Root clamping at / (BLOCKED_NOT_IMPLEMENTED - ".." at root goes to "/" with current
 * implementation)
 * - Path classification (VIRTUAL_LINUX detection is separate seam)
 * - VFS translation routing (separate seam entirely)
 *
 * The 3 removed tests (vfs_path_relative_uses_cwd, vfs_path_dotdot_segments with /..,
 * vfs_path_cwd_change_affects_resolution) were testing across multiple seams.
 * This unit now tests ONLY pure normalization via __ixland_path_normalize().
 */
