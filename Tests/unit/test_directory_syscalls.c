/* Directory Syscall Tests - chdir, fchdir, getcwd, mkdir, rmdir, unlink, symlink, readlink
 *
 * Tests directory and filesystem operation syscalls:
 * - ixland_chdir() - Change working directory
 * - ixland_fchdir() - Change working directory via FD
 * - ixland_getcwd() - Get current working directory
 * - ixland_mkdir() - Create directory
 * - ixland_rmdir() - Remove directory
 * - ixland_unlink() - Remove file
 * - ixland_symlink() - Create symbolic link
 * - ixland_readlink() - Read symbolic link target
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../harness/ixland_test.h"

/* Function declarations from directory syscall ownership */
extern int ixland_chdir(const char *path);
extern int ixland_fchdir(int fd);
extern char *ixland_getcwd(char *buf, size_t size);
extern int ixland_mkdir(const char *pathname, mode_t mode);
extern int ixland_rmdir(const char *pathname);
extern int ixland_unlink(const char *pathname);
extern int ixland_symlink(const char *target, const char *linkpath);
extern ssize_t ixland_readlink(const char *pathname, char *buf, size_t bufsiz);
extern int ixland_chroot(const char *path);
extern int ixland_vfs_translate(const char *vpath, char *ios_path, size_t ios_path_len);

#define TEST_DIR "/tmp/ixland_dir_test"
#define TEST_SUBDIR TEST_DIR "/subdir"
#define TEST_NESTED TEST_DIR "/nested/deep"
#define TEST_FILE TEST_DIR "/testfile.txt"
#define TEST_SYMLINK TEST_DIR "/testlink"

static int test_translate_path(const char *vpath, char *ios_path, size_t len) {
    return ixland_vfs_translate(vpath, ios_path, len);
}

static void cleanup_path_recursive(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) {
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        rmdir(path);
    } else {
        unlink(path);
    }
}

static void setup_test_dir(void) {
    char ios_test_dir[1024];
    if (test_translate_path(TEST_DIR, ios_test_dir, sizeof(ios_test_dir)) != 0) {
        return;
    }

    cleanup_path_recursive(ios_test_dir);
    mkdir(ios_test_dir, 0755);
}

static void cleanup_test_dir(void) {
    char ios_nested[1024];
    char ios_subdir[1024];
    char ios_file[1024];
    char ios_link[1024];
    char ios_test_dir[1024];

    if (test_translate_path(TEST_NESTED, ios_nested, sizeof(ios_nested)) != 0 ||
        test_translate_path(TEST_SUBDIR, ios_subdir, sizeof(ios_subdir)) != 0 ||
        test_translate_path(TEST_FILE, ios_file, sizeof(ios_file)) != 0 ||
        test_translate_path(TEST_SYMLINK, ios_link, sizeof(ios_link)) != 0 ||
        test_translate_path(TEST_DIR, ios_test_dir, sizeof(ios_test_dir)) != 0) {
        return;
    }

    cleanup_path_recursive(ios_link);
    cleanup_path_recursive(ios_file);

    char ios_nested_file[1024];
    char ios_nested_link[1024];
    snprintf(ios_nested_file, sizeof(ios_nested_file), "%s/test.txt", ios_nested);
    snprintf(ios_nested_link, sizeof(ios_nested_link), "%s/link.txt", ios_nested);
    cleanup_path_recursive(ios_nested_link);
    cleanup_path_recursive(ios_nested_file);

    cleanup_path_recursive(ios_nested);
    cleanup_path_recursive(ios_subdir);
    cleanup_path_recursive(ios_test_dir);
}

/* ============================================================================
 * chdir() tests
 * ============================================================================ */

IXLAND_TEST(chdir_null_path_returns_efault) {
    int result = ixland_chdir(NULL);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EFAULT);
    return true;
}

IXLAND_TEST(chdir_empty_path_returns_enoent) {
    int result = ixland_chdir("");
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOENT);
    return true;
}

IXLAND_TEST(chdir_nonexistent_returns_enoent) {
    int result = ixland_chdir("/tmp/ixland_nonexistent_dir_12345");
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOENT);
    return true;
}

IXLAND_TEST(chdir_file_returns_enotdir) {
    setup_test_dir();

    /* Create a test file */
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);
    close(fd);

    /* Try to chdir to file */
    int result = ixland_chdir(TEST_FILE);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOTDIR);

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(chdir_directory_returns_success) {
    setup_test_dir();
    mkdir(TEST_SUBDIR, 0755);

    /* Save original cwd */
    char original_cwd[1024];
    getcwd(original_cwd, sizeof(original_cwd));

    /* Change to test directory */
    int result = ixland_chdir(TEST_SUBDIR);
    IXLAND_ASSERT_EQ(result, 0);

    /* Verify cwd changed */
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    IXLAND_ASSERT(strstr(cwd, "subdir") != NULL);

    /* Restore original cwd */
    chdir(original_cwd);
    cleanup_test_dir();
    return true;
}

/* ============================================================================
 * getcwd() tests
 * ============================================================================ */

IXLAND_TEST(getcwd_null_buf_returns_null) {
    char *result = ixland_getcwd(NULL, 1024);
    IXLAND_ASSERT_EQ(result, (char *)NULL);
    IXLAND_ASSERT_EQ(errno, EINVAL);
    return true;
}

IXLAND_TEST(getcwd_zero_size_returns_null) {
    char buf[1024];
    char *result = ixland_getcwd(buf, 0);
    IXLAND_ASSERT_EQ(result, (char *)NULL);
    IXLAND_ASSERT_EQ(errno, EINVAL);
    return true;
}

IXLAND_TEST(getcwd_small_buffer_returns_erange) {
    setup_test_dir();
    mkdir(TEST_SUBDIR, 0755);
    chdir(TEST_SUBDIR);

    /* Try with very small buffer */
    char buf[1];
    char *result = ixland_getcwd(buf, sizeof(buf));
    IXLAND_ASSERT_EQ(result, (char *)NULL);
    IXLAND_ASSERT_EQ(errno, ERANGE);

    chdir("/");
    cleanup_test_dir();
    return true;
}

IXLAND_TEST(getcwd_returns_absolute_path) {
    setup_test_dir();
    mkdir(TEST_SUBDIR, 0755);

    /* Save original cwd */
    char original_cwd[1024];
    getcwd(original_cwd, sizeof(original_cwd));

    /* Change to test directory */
    chdir(TEST_SUBDIR);

    /* Get cwd */
    char buf[1024];
    char *result = ixland_getcwd(buf, sizeof(buf));
    IXLAND_ASSERT_NE(result, (char *)NULL);
    IXLAND_ASSERT_EQ(result, buf);
    IXLAND_ASSERT(buf[0] == '/'); /* Absolute path */
    IXLAND_ASSERT(strstr(buf, "subdir") != NULL);

    /* Restore original cwd */
    chdir(original_cwd);
    cleanup_test_dir();
    return true;
}

IXLAND_TEST(getcwd_roundtrip_with_chdir) {
    setup_test_dir();
    mkdir(TEST_SUBDIR, 0755);

    /* Save original cwd */
    char original_cwd[1024];
    getcwd(original_cwd, sizeof(original_cwd));

    /* Change directory */
    int result = ixland_chdir(TEST_SUBDIR);
    IXLAND_ASSERT_EQ(result, 0);

    /* Get current directory */
    char cwd[1024];
    char *got = ixland_getcwd(cwd, sizeof(cwd));
    IXLAND_ASSERT_NE(got, (char *)NULL);

    /* Verify we can cd back to original */
    result = ixland_chdir(original_cwd);
    IXLAND_ASSERT_EQ(result, 0);

    char cwd2[1024];
    got = ixland_getcwd(cwd2, sizeof(cwd2));
    IXLAND_ASSERT_NE(got, (char *)NULL);
    IXLAND_ASSERT_EQ(strcmp(cwd2, original_cwd), 0);

    cleanup_test_dir();
    return true;
}

/* ============================================================================
 * fchdir() tests
 * ============================================================================ */

IXLAND_TEST(fchdir_invalid_fd_returns_ebadf) {
    int result = ixland_fchdir(-1);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EBADF);

    result = ixland_fchdir(9999);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EBADF);
    return true;
}

IXLAND_TEST(fchdir_stdin_returns_ebadf) {
    int result = ixland_fchdir(STDIN_FILENO);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EBADF);
    return true;
}

IXLAND_TEST(fchdir_file_returns_enotdir) {
    setup_test_dir();

    /* Create a test file */
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);
    close(fd);

    /* Open the file */
    fd = open(TEST_FILE, O_RDONLY);
    IXLAND_ASSERT_GT(fd, -1);

    /* Try to fchdir to file */
    int result = ixland_fchdir(fd);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOTDIR);

    close(fd);
    cleanup_test_dir();
    return true;
}

/* ============================================================================
 * mkdir() tests
 * ============================================================================ */

IXLAND_TEST(mkdir_null_pathname_returns_efault) {
    int result = ixland_mkdir(NULL, 0755);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EFAULT);
    return true;
}

IXLAND_TEST(mkdir_empty_pathname_returns_enoent) {
    int result = ixland_mkdir("", 0755);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOENT);
    return true;
}

IXLAND_TEST(mkdir_new_directory_returns_success) {
    setup_test_dir();

    int result = ixland_mkdir(TEST_SUBDIR, 0755);
    IXLAND_ASSERT_EQ(result, 0);

    /* Verify directory exists */
    struct stat st;
    result = stat(TEST_SUBDIR, &st);
    IXLAND_ASSERT_EQ(result, 0);
    IXLAND_ASSERT(S_ISDIR(st.st_mode));

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(mkdir_with_mode_sets_permissions) {
    setup_test_dir();

    /* Create directory with specific mode */
    int result = ixland_mkdir(TEST_SUBDIR, 0700);
    IXLAND_ASSERT_EQ(result, 0);

    /* Verify mode was set (may be masked by umask) */
    struct stat st;
    result = stat(TEST_SUBDIR, &st);
    IXLAND_ASSERT_EQ(result, 0);
    IXLAND_ASSERT(S_ISDIR(st.st_mode));
    /* Check owner has read/write/execute */
    IXLAND_ASSERT(st.st_mode & S_IRWXU);

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(mkdir_existing_directory_returns_eexist) {
    setup_test_dir();
    mkdir(TEST_SUBDIR, 0755);

    /* Try to create same directory again */
    int result = ixland_mkdir(TEST_SUBDIR, 0755);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EEXIST);

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(mkdir_existing_file_returns_eexist) {
    setup_test_dir();

    /* Create a file */
    int fd = open(TEST_SUBDIR, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);
    close(fd);

    /* Try to create directory with same name */
    int result = ixland_mkdir(TEST_SUBDIR, 0755);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EEXIST);

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(mkdir_no_parent_returns_enoent) {
    /* Try to create directory without parent */
    int result = ixland_mkdir("/tmp/nonexistent_parent_12345/subdir", 0755);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOENT);
    return true;
}

IXLAND_TEST(mkdir_nested_parent_missing) {
    setup_test_dir();

    /* Try to create nested directory without intermediate */
    int result = ixland_mkdir(TEST_NESTED, 0755);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOENT);

    cleanup_test_dir();
    return true;
}

/* ============================================================================
 * rmdir() tests
 * ============================================================================ */

IXLAND_TEST(rmdir_null_pathname_returns_efault) {
    int result = ixland_rmdir(NULL);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EFAULT);
    return true;
}

IXLAND_TEST(rmdir_empty_pathname_returns_enoent) {
    int result = ixland_rmdir("");
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOENT);
    return true;
}

IXLAND_TEST(rmdir_nonexistent_returns_enoent) {
    int result = ixland_rmdir("/tmp/ixland_nonexistent_rmdir_12345");
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOENT);
    return true;
}

IXLAND_TEST(rmdir_file_returns_enotdir) {
    setup_test_dir();

    /* Create a file */
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);
    close(fd);

    /* Try to rmdir a file */
    int result = ixland_rmdir(TEST_FILE);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOTDIR);

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(rmdir_empty_directory_returns_success) {
    setup_test_dir();
    mkdir(TEST_SUBDIR, 0755);

    /* Remove empty directory */
    int result = ixland_rmdir(TEST_SUBDIR);
    IXLAND_ASSERT_EQ(result, 0);

    /* Verify directory is gone */
    struct stat st;
    result = stat(TEST_SUBDIR, &st);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOENT);

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(rmdir_nonempty_directory_returns_enotempty) {
    setup_test_dir();
    mkdir(TEST_SUBDIR, 0755);

    /* Create a file in subdirectory */
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/file.txt", TEST_SUBDIR);
    int fd = open(filepath, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);
    close(fd);

    /* Try to remove non-empty directory */
    int result = ixland_rmdir(TEST_SUBDIR);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOTEMPTY);

    /* Cleanup */
    unlink(filepath);
    rmdir(TEST_SUBDIR);
    cleanup_test_dir();
    return true;
}

/* ============================================================================
 * unlink() tests
 * ============================================================================ */

IXLAND_TEST(unlink_null_pathname_returns_efault) {
    int result = ixland_unlink(NULL);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EFAULT);
    return true;
}

IXLAND_TEST(unlink_empty_pathname_returns_enoent) {
    int result = ixland_unlink("");
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOENT);
    return true;
}

IXLAND_TEST(unlink_nonexistent_returns_enoent) {
    int result = ixland_unlink("/tmp/ixland_nonexistent_unlink_12345");
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOENT);
    return true;
}

IXLAND_TEST(unlink_file_returns_success) {
    setup_test_dir();

    /* Create a file */
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);
    close(fd);

    /* Verify file exists */
    struct stat st;
    int result = stat(TEST_FILE, &st);
    IXLAND_ASSERT_EQ(result, 0);

    /* Unlink the file */
    result = ixland_unlink(TEST_FILE);
    IXLAND_ASSERT_EQ(result, 0);

    /* Verify file is gone */
    result = stat(TEST_FILE, &st);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOENT);

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(unlink_directory_returns_eisdir) {
    setup_test_dir();
    mkdir(TEST_SUBDIR, 0755);

    /* Try to unlink a directory */
    int result = ixland_unlink(TEST_SUBDIR);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EISDIR);

    cleanup_test_dir();
    return true;
}

/* ============================================================================
 * symlink() and readlink() tests
 * ============================================================================ */

IXLAND_TEST(symlink_null_target_returns_efault) {
    setup_test_dir();

    int result = ixland_symlink(NULL, TEST_SYMLINK);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EFAULT);

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(symlink_null_linkpath_returns_efault) {
    int result = ixland_symlink("/tmp/target", NULL);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EFAULT);
    return true;
}

IXLAND_TEST(symlink_empty_linkpath_returns_enoent) {
    int result = ixland_symlink("/tmp/target", "");
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOENT);
    return true;
}

IXLAND_TEST(symlink_existing_returns_eexist) {
    setup_test_dir();

    /* Create a file */
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);
    close(fd);

    /* Try to create symlink with same name */
    int result = ixland_symlink("/tmp/target", TEST_FILE);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EEXIST);

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(symlink_creates_symlink) {
    setup_test_dir();

    /* Create a file as target */
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);
    close(fd);

    /* Create symlink pointing to file */
    int result = ixland_symlink(TEST_FILE, TEST_SYMLINK);
    IXLAND_ASSERT_EQ(result, 0);

    /* Verify symlink exists and points to target */
    struct stat st;
    result = lstat(TEST_SYMLINK, &st);
    IXLAND_ASSERT_EQ(result, 0);
    IXLAND_ASSERT(S_ISLNK(st.st_mode));

    /* Verify we can stat through the symlink */
    result = stat(TEST_SYMLINK, &st);
    IXLAND_ASSERT_EQ(result, 0);
    IXLAND_ASSERT(S_ISREG(st.st_mode));

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(readlink_null_pathname_returns_efault) {
    char buf[256];
    ssize_t result = ixland_readlink(NULL, buf, sizeof(buf));
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EFAULT);
    return true;
}

IXLAND_TEST(readlink_null_buf_returns_efault) {
    setup_test_dir();

    /* Create symlink first */
    symlink("/tmp/target", TEST_SYMLINK);

    ssize_t result = ixland_readlink(TEST_SYMLINK, NULL, 256);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EFAULT);

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(readlink_zero_bufsiz_returns_einval) {
    setup_test_dir();

    /* Create symlink first */
    symlink("/tmp/target", TEST_SYMLINK);

    char buf[256];
    ssize_t result = ixland_readlink(TEST_SYMLINK, buf, 0);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(readlink_nonexistent_returns_enoent) {
    char buf[256];
    ssize_t result = ixland_readlink("/tmp/ixland_nonexistent_symlink_12345", buf, sizeof(buf));
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOENT);
    return true;
}

IXLAND_TEST(readlink_regular_file_returns_einval) {
    setup_test_dir();

    /* Create a file (not a symlink) */
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);
    close(fd);

    char buf[256];
    ssize_t result = ixland_readlink(TEST_FILE, buf, sizeof(buf));
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(symlink_readlink_roundtrip) {
    setup_test_dir();

    const char *target = "/home/user/test/target/path";

    /* Create symlink */
    int result = ixland_symlink(target, TEST_SYMLINK);
    IXLAND_ASSERT_EQ(result, 0);

    /* Read symlink target */
    char buf[256];
    ssize_t len = ixland_readlink(TEST_SYMLINK, buf, sizeof(buf));
    IXLAND_ASSERT_GT(len, 0);

    /* Verify target matches (POSIX doesn't require null-termination) */
    IXLAND_ASSERT_EQ((size_t)len, strlen(target));
    IXLAND_ASSERT_EQ(strncmp(buf, target, (size_t)len), 0);

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(symlink_relative_target_preserved) {
    setup_test_dir();

    const char *relative_target = "../other/file.txt";

    /* Create symlink with relative target */
    int result = ixland_symlink(relative_target, TEST_SYMLINK);
    IXLAND_ASSERT_EQ(result, 0);

    /* Read and verify target is preserved */
    char buf[256];
    ssize_t len = ixland_readlink(TEST_SYMLINK, buf, sizeof(buf));
    IXLAND_ASSERT_GT(len, 0);
    IXLAND_ASSERT_EQ((size_t)len, strlen(relative_target));
    IXLAND_ASSERT_EQ(strncmp(buf, relative_target, (size_t)len), 0);

    cleanup_test_dir();
    return true;
}

/* ============================================================================
 * chroot() tests
 * ============================================================================ */

IXLAND_TEST(chroot_returns_eperm) {
    int result = ixland_chroot("/tmp");
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EPERM);
    return true;
}

/* ============================================================================
 * Integration tests
 * ============================================================================ */

IXLAND_TEST(directory_operations_integration) {
    setup_test_dir();

    /* Create nested directory structure */
    int result = ixland_mkdir(TEST_SUBDIR, 0755);
    IXLAND_ASSERT_EQ(result, 0);

    char nested_dir[256];
    snprintf(nested_dir, sizeof(nested_dir), "%s/nested", TEST_SUBDIR);
    result = ixland_mkdir(nested_dir, 0700);
    IXLAND_ASSERT_EQ(result, 0);

    /* Create a file in nested dir */
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s/test.txt", nested_dir);
    int fd = open(filepath, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);
    close(fd);

    /* Create symlink to file */
    char linkpath[256];
    snprintf(linkpath, sizeof(linkpath), "%s/link.txt", nested_dir);
    result = ixland_symlink(filepath, linkpath);
    IXLAND_ASSERT_EQ(result, 0);

    /* Change to nested directory */
    char original_cwd[1024];
    getcwd(original_cwd, sizeof(original_cwd));

    result = ixland_chdir(nested_dir);
    IXLAND_ASSERT_EQ(result, 0);

    /* Get cwd and verify */
    char cwd[1024];
    char *got = ixland_getcwd(cwd, sizeof(cwd));
    IXLAND_ASSERT_NE(got, (char *)NULL);
    IXLAND_ASSERT(strstr(cwd, "nested") != NULL);

    /* Go back */
    result = ixland_chdir(original_cwd);
    IXLAND_ASSERT_EQ(result, 0);

    /* Cleanup: remove file, symlink, directories */
    unlink(filepath);
    unlink(linkpath);
    rmdir(nested_dir);
    rmdir(TEST_SUBDIR);

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(mkdir_chdir_getcwd_roundtrip) {
    setup_test_dir();

    /* Create and cd to directory */
    ixland_mkdir(TEST_SUBDIR, 0755);

    char original[1024];
    getcwd(original, sizeof(original));

    ixland_chdir(TEST_SUBDIR);

    char cwd1[1024];
    ixland_getcwd(cwd1, sizeof(cwd1));

    /* Go back and verify */
    ixland_chdir(original);

    char cwd2[1024];
    ixland_getcwd(cwd2, sizeof(cwd2));
    IXLAND_ASSERT_EQ(strcmp(cwd2, original), 0);

    /* Cleanup */
    rmdir(TEST_SUBDIR);
    cleanup_test_dir();
    return true;
}
