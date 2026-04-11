/* File Syscall Tests - stat, fstat, lstat, access
 *
 * Tests file operation syscalls with VFS layer:
 * - ixland_stat() - get file status by path
 * - ixland_fstat() - get file status by file descriptor
 * - ixland_lstat() - get file status without following symlinks
 * - ixland_access() - check file accessibility
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../harness/ixland_test.h"

/* Function declarations from ixland fs modules */
extern int __ixland_stat_impl(const char *pathname, struct stat *statbuf);
extern int __ixland_fstat_impl(int fd, struct stat *statbuf);
extern int __ixland_lstat_impl(const char *pathname, struct stat *statbuf);
extern int __ixland_access_impl(const char *pathname, int mode);

/* Public API wrappers */
extern int ixland_stat(const char *pathname, struct stat *statbuf);
extern int ixland_fstat(int fd, struct stat *statbuf);
extern int ixland_lstat(const char *pathname, struct stat *statbuf);
extern int ixland_access(const char *pathname, int mode);

#define TEST_DIR "/tmp/ixland_test"
#define TEST_FILE TEST_DIR "/testfile.txt"
#define TEST_SYMLINK TEST_DIR "/testlink"

static void setup_test_dir(void) {
    mkdir(TEST_DIR, 0755);
}

static void cleanup_test_dir(void) {
    unlink(TEST_FILE);
    unlink(TEST_SYMLINK);
    rmdir(TEST_DIR);
}

/* ============================================================================
 * stat() tests
 * ============================================================================ */

IXLAND_TEST(stat_null_pathname_returns_efault) {
    struct stat st;
    int result = ixland_stat(NULL, &st);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EFAULT);
    return true;
}

IXLAND_TEST(stat_null_statbuf_returns_efault) {
    int result = ixland_stat("/tmp", NULL);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EFAULT);
    return true;
}

IXLAND_TEST(stat_existing_file_returns_success) {
    setup_test_dir();

    /* Create a test file */
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);
    write(fd, "hello", 5);
    close(fd);

    /* Stat the file */
    struct stat st;
    int result = ixland_stat(TEST_FILE, &st);
    IXLAND_ASSERT_EQ(result, 0);
    IXLAND_ASSERT_EQ(st.st_size, 5);
    IXLAND_ASSERT(S_ISREG(st.st_mode));

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(stat_directory_returns_directory_mode) {
    setup_test_dir();

    struct stat st;
    int result = ixland_stat(TEST_DIR, &st);
    IXLAND_ASSERT_EQ(result, 0);
    IXLAND_ASSERT(S_ISDIR(st.st_mode));

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(stat_nonexistent_file_returns_enoent) {
    const char *nonexistent = "/tmp/ixland_nonexistent_file_12345";
    struct stat st;
    int result = ixland_stat(nonexistent, &st);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOENT);
    return true;
}

/* ============================================================================
 * fstat() tests
 * ============================================================================ */

IXLAND_TEST(fstat_null_statbuf_returns_efault) {
    int result = ixland_fstat(0, NULL);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EFAULT);
    return true;
}

IXLAND_TEST(fstat_invalid_fd_returns_ebadf) {
    struct stat st;
    int result = ixland_fstat(-1, &st);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EBADF);

    result = ixland_fstat(9999, &st);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EBADF);
    return true;
}

IXLAND_TEST(fstat_standard_fd_returns_success) {
    struct stat st;
    int result = ixland_fstat(STDIN_FILENO, &st);
    IXLAND_ASSERT_EQ(result, 0);
    return true;
}

IXLAND_TEST(fstat_open_file_returns_correct_size) {
    setup_test_dir();

    /* Create a test file */
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);
    write(fd, "hello world", 11);
    close(fd);

    /* Reopen and fstat */
    fd = open(TEST_FILE, O_RDONLY);
    IXLAND_ASSERT_GT(fd, -1);

    struct stat st;
    int result = ixland_fstat(fd, &st);
    IXLAND_ASSERT_EQ(result, 0);
    IXLAND_ASSERT_EQ(st.st_size, 11);
    IXLAND_ASSERT(S_ISREG(st.st_mode));

    close(fd);
    cleanup_test_dir();
    return true;
}

/* ============================================================================
 * lstat() tests
 * ============================================================================ */

IXLAND_TEST(lstat_null_pathname_returns_efault) {
    struct stat st;
    int result = ixland_lstat(NULL, &st);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EFAULT);
    return true;
}

IXLAND_TEST(lstat_null_statbuf_returns_efault) {
    int result = ixland_lstat("/tmp", NULL);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EFAULT);
    return true;
}

IXLAND_TEST(lstat_symlink_returns_symlink_info) {
    setup_test_dir();

    /* Create a test file */
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);
    close(fd);

    /* Create a symlink to the file */
    int result = symlink(TEST_FILE, TEST_SYMLINK);
    IXLAND_ASSERT_EQ(result, 0);

    /* lstat should return info about the symlink, not the target */
    struct stat st;
    result = ixland_lstat(TEST_SYMLINK, &st);
    IXLAND_ASSERT_EQ(result, 0);
    IXLAND_ASSERT(S_ISLNK(st.st_mode));

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(lstat_regular_file_same_as_stat) {
    setup_test_dir();

    /* Create a test file */
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);
    write(fd, "test", 4);
    close(fd);

    /* lstat and stat should return same info for regular file */
    struct stat lst, st;
    int lresult = ixland_lstat(TEST_FILE, &lst);
    int result = ixland_stat(TEST_FILE, &st);
    IXLAND_ASSERT_EQ(lresult, 0);
    IXLAND_ASSERT_EQ(result, 0);
    IXLAND_ASSERT_EQ(lst.st_size, st.st_size);
    IXLAND_ASSERT_EQ(lst.st_mode, st.st_mode);

    cleanup_test_dir();
    return true;
}

/* ============================================================================
 * access() tests
 * ============================================================================ */

IXLAND_TEST(access_null_pathname_returns_efault) {
    int result = ixland_access(NULL, F_OK);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, EFAULT);
    return true;
}

IXLAND_TEST(access_existing_file_returns_success) {
    setup_test_dir();

    /* Create a test file */
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);
    close(fd);

    /* File exists */
    int result = ixland_access(TEST_FILE, F_OK);
    IXLAND_ASSERT_EQ(result, 0);

    /* File is readable */
    result = ixland_access(TEST_FILE, R_OK);
    IXLAND_ASSERT_EQ(result, 0);

    /* File is writable */
    result = ixland_access(TEST_FILE, W_OK);
    IXLAND_ASSERT_EQ(result, 0);

    cleanup_test_dir();
    return true;
}

IXLAND_TEST(access_nonexistent_file_returns_enoent) {
    const char *nonexistent = "/tmp/ixland_nonexistent_access_12345";
    int result = ixland_access(nonexistent, F_OK);
    IXLAND_ASSERT_EQ(result, -1);
    IXLAND_ASSERT_EQ(errno, ENOENT);
    return true;
}

IXLAND_TEST(access_directory_returns_success) {
    setup_test_dir();

    int result = ixland_access(TEST_DIR, F_OK);
    IXLAND_ASSERT_EQ(result, 0);

    result = ixland_access(TEST_DIR, R_OK | X_OK);
    IXLAND_ASSERT_EQ(result, 0);

    cleanup_test_dir();
    return true;
}

/* ============================================================================
 * Integration tests
 * ============================================================================ */

IXLAND_TEST(file_syscalls_integration) {
    setup_test_dir();

    /* Create a file */
    int fd = open(TEST_FILE, O_CREAT | O_WRONLY, 0644);
    IXLAND_ASSERT_GT(fd, -1);

    /* Write some data */
    const char *data = "integration test data";
    ssize_t written = write(fd, data, strlen(data));
    IXLAND_ASSERT_EQ(written, (ssize_t)strlen(data));
    close(fd);

    /* Verify with stat */
    struct stat st;
    int result = ixland_stat(TEST_FILE, &st);
    IXLAND_ASSERT_EQ(result, 0);
    IXLAND_ASSERT_EQ(st.st_size, (off_t)strlen(data));
    IXLAND_ASSERT(S_ISREG(st.st_mode));

    /* Verify with open + fstat */
    fd = open(TEST_FILE, O_RDONLY);
    IXLAND_ASSERT_GT(fd, -1);

    struct stat fst;
    result = ixland_fstat(fd, &fst);
    IXLAND_ASSERT_EQ(result, 0);
    IXLAND_ASSERT_EQ(fst.st_size, (off_t)strlen(data));

    /* Verify with access */
    result = ixland_access(TEST_FILE, R_OK);
    IXLAND_ASSERT_EQ(result, 0);

    close(fd);
    cleanup_test_dir();
    return true;
}
