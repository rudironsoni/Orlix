#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "../../fs/fdtable.h"
#include "../harness/ixland_test.h"

/* FD Table Semantics Tests
 *
 * Tests only VERIFIED_IMPLEMENTED_NOW behaviors from fs/fdtable.c:
 * - fd table allocation/free
 * - allocate lowest available fd
 * - close semantics
 * - bounds checks
 * - full-table behavior
 * - descriptor lookup
 * - whole-table duplication
 * - CLOEXEC flag storage (declared in header but check if implemented)
 *
 * BLOCKED_NOT_IMPLEMENTED (declared in header but no implementation):
 * - ixland_fd_dup (single-fd duplication)
 * - ixland_fd_dup2 (dup2 semantics)
 *
 * CLOEXEC functions declared in header but implementation status UNKNOWN:
 * - ixland_fd_set_cloexec (must check if exists)
 * - ixland_fd_get_cloexec (must check if exists)
 * - ixland_fd_close_cloexec (must check if exists)
 */

#define TEST_MAX_FDS 16

IXLAND_TEST(fdtable_alloc_basic) {
    ixland_files_t *files = ixland_files_alloc(TEST_MAX_FDS);
    IXLAND_ASSERT_NOT_NULL(files);
    IXLAND_ASSERT_EQ(files->max_fds, TEST_MAX_FDS);

    /* All slots should be NULL initially */
    for (size_t i = 0; i < files->max_fds; i++) {
        IXLAND_ASSERT_NULL(files->fd[i]);
    }

    ixland_files_free(files);
    return true;
}

IXLAND_TEST(fdtable_alloc_zero_fails) {
    /* Zero max_fds should still allocate but be useless */
    ixland_files_t *files = ixland_files_alloc(0);
    IXLAND_ASSERT_NOT_NULL(files);
    IXLAND_ASSERT_EQ(files->max_fds, 0);
    /* Note: calloc(0, size) behavior is implementation-defined.
     * On macOS/iOS it returns a valid pointer (not NULL) that can be freed.
     * The important thing is the table works correctly. */

    ixland_files_free(files);
    return true;
}

IXLAND_TEST(fdtable_free_null_safe) {
    /* Should not crash on NULL */
    ixland_files_free(NULL);
    return true;
}

IXLAND_TEST(fd_alloc_lowest_available) {
    ixland_files_t *files = ixland_files_alloc(TEST_MAX_FDS);
    IXLAND_ASSERT_NOT_NULL(files);

    /* Allocate first file */
    ixland_file_t *file1 = ixland_file_alloc();
    IXLAND_ASSERT_NOT_NULL(file1);

    int fd1 = ixland_fd_alloc(files, file1);
    IXLAND_ASSERT_EQ(fd1, 0); /* Lowest available */

    /* Allocate second file */
    ixland_file_t *file2 = ixland_file_alloc();
    IXLAND_ASSERT_NOT_NULL(file2);

    int fd2 = ixland_fd_alloc(files, file2);
    IXLAND_ASSERT_EQ(fd2, 1); /* Next lowest */

    /* Close fd0, allocate again */
    IXLAND_ASSERT_EQ(ixland_fd_free(files, 0), 0);

    ixland_file_t *file3 = ixland_file_alloc();
    IXLAND_ASSERT_NOT_NULL(file3);

    int fd3 = ixland_fd_alloc(files, file3);
    IXLAND_ASSERT_EQ(fd3, 0); /* Should reuse lowest available */

    ixland_files_free(files);
    ixland_file_free(file1);
    ixland_file_free(file2);
    ixland_file_free(file3);
    return true;
}

IXLAND_TEST(fd_alloc_table_full) {
    ixland_files_t *files = ixland_files_alloc(2);
    IXLAND_ASSERT_NOT_NULL(files);

    /* Fill the table */
    ixland_file_t *file1 = ixland_file_alloc();
    ixland_file_t *file2 = ixland_file_alloc();
    IXLAND_ASSERT_NOT_NULL(file1);
    IXLAND_ASSERT_NOT_NULL(file2);

    int fd1 = ixland_fd_alloc(files, file1);
    int fd2 = ixland_fd_alloc(files, file2);
    IXLAND_ASSERT_EQ(fd1, 0);
    IXLAND_ASSERT_EQ(fd2, 1);

    /* Try to allocate third - should fail */
    ixland_file_t *file3 = ixland_file_alloc();
    IXLAND_ASSERT_NOT_NULL(file3);

    int fd3 = ixland_fd_alloc(files, file3);
    IXLAND_ASSERT_EQ(fd3, -1);
    IXLAND_ASSERT_EQ(errno, EMFILE);

    /* Table should be unchanged */
    IXLAND_ASSERT_NOT_NULL(files->fd[0]);
    IXLAND_ASSERT_NOT_NULL(files->fd[1]);

    ixland_files_free(files);
    ixland_file_free(file1);
    ixland_file_free(file2);
    ixland_file_free(file3);
    return true;
}

IXLAND_TEST(fd_alloc_null_params) {
    ixland_files_t *files = ixland_files_alloc(TEST_MAX_FDS);
    ixland_file_t *file = ixland_file_alloc();
    IXLAND_ASSERT_NOT_NULL(files);
    IXLAND_ASSERT_NOT_NULL(file);

    /* NULL files */
    errno = 0;
    int ret = ixland_fd_alloc(NULL, file);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    /* NULL file */
    errno = 0;
    ret = ixland_fd_alloc(files, NULL);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EINVAL);

    ixland_files_free(files);
    ixland_file_free(file);
    return true;
}

IXLAND_TEST(fd_free_basic) {
    ixland_files_t *files = ixland_files_alloc(TEST_MAX_FDS);
    ixland_file_t *file = ixland_file_alloc();
    IXLAND_ASSERT_NOT_NULL(files);
    IXLAND_ASSERT_NOT_NULL(file);

    int fd = ixland_fd_alloc(files, file);
    IXLAND_ASSERT_EQ(fd, 0);
    IXLAND_ASSERT_NOT_NULL(files->fd[0]);

    /* Close the fd */
    int ret = ixland_fd_free(files, fd);
    IXLAND_ASSERT_EQ(ret, 0);
    IXLAND_ASSERT_NULL(files->fd[0]);

    ixland_files_free(files);
    ixland_file_free(file);
    return true;
}

IXLAND_TEST(fd_free_invalid_fd) {
    ixland_files_t *files = ixland_files_alloc(TEST_MAX_FDS);
    IXLAND_ASSERT_NOT_NULL(files);

    /* Negative fd */
    errno = 0;
    int ret = ixland_fd_free(files, -1);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EBADF);

    /* Out of bounds fd */
    errno = 0;
    ret = ixland_fd_free(files, TEST_MAX_FDS);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EBADF);

    /* Already closed fd */
    ixland_file_t *file = ixland_file_alloc();
    IXLAND_ASSERT_NOT_NULL(file);
    int fd = ixland_fd_alloc(files, file);
    IXLAND_ASSERT_EQ(ixland_fd_free(files, fd), 0);

    errno = 0;
    ret = ixland_fd_free(files, fd); /* Close again */
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EBADF);

    ixland_files_free(files);
    ixland_file_free(file);
    return true;
}

IXLAND_TEST(fd_free_null_files) {
    errno = 0;
    int ret = ixland_fd_free(NULL, 0);
    IXLAND_ASSERT_EQ(ret, -1);
    IXLAND_ASSERT_EQ(errno, EBADF);
    return true;
}

IXLAND_TEST(fd_lookup_basic) {
    ixland_files_t *files = ixland_files_alloc(TEST_MAX_FDS);
    ixland_file_t *file = ixland_file_alloc();
    IXLAND_ASSERT_NOT_NULL(files);
    IXLAND_ASSERT_NOT_NULL(file);

    int fd = ixland_fd_alloc(files, file);
    IXLAND_ASSERT_EQ(fd, 0);

    /* Lookup should succeed */
    ixland_file_t *found = ixland_fd_lookup(files, fd);
    IXLAND_ASSERT_NOT_NULL(found);
    IXLAND_ASSERT_EQ(found, files->fd[0]);

    /* Refcount should be incremented by lookup */
    int refs_after = atomic_load(&found->refs);
    IXLAND_ASSERT(refs_after >= 2); /* Original + lookup ref */

    ixland_file_free(found); /* Release lookup ref */
    ixland_files_free(files);
    ixland_file_free(file);
    return true;
}

IXLAND_TEST(fd_lookup_invalid_fd) {
    ixland_files_t *files = ixland_files_alloc(TEST_MAX_FDS);
    IXLAND_ASSERT_NOT_NULL(files);

    /* Negative fd */
    errno = 0;
    ixland_file_t *found = ixland_fd_lookup(files, -1);
    IXLAND_ASSERT_NULL(found);
    IXLAND_ASSERT_EQ(errno, EBADF);

    /* Out of bounds */
    errno = 0;
    found = ixland_fd_lookup(files, TEST_MAX_FDS);
    IXLAND_ASSERT_NULL(found);
    IXLAND_ASSERT_EQ(errno, EBADF);

    /* Unallocated fd within bounds - returns NULL without setting errno
     * (implementation only sets errno on bounds check, not on unallocated slot) */
    found = ixland_fd_lookup(files, 5); /* Never allocated */
    IXLAND_ASSERT_NULL(found);
    /* Note: ixland_fd_lookup only sets errno on bounds check, not on unallocated slot */

    ixland_files_free(files);
    return true;
}

IXLAND_TEST(fd_lookup_null_files) {
    errno = 0;
    ixland_file_t *found = ixland_fd_lookup(NULL, 0);
    IXLAND_ASSERT_NULL(found);
    IXLAND_ASSERT_EQ(errno, EBADF);
    return true;
}

IXLAND_TEST(fdtable_dup_basic) {
    ixland_files_t *parent = ixland_files_alloc(TEST_MAX_FDS);
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Add some files to parent */
    ixland_file_t *file1 = ixland_file_alloc();
    ixland_file_t *file2 = ixland_file_alloc();
    IXLAND_ASSERT_NOT_NULL(file1);
    IXLAND_ASSERT_NOT_NULL(file2);

    int fd1 = ixland_fd_alloc(parent, file1);
    int fd2 = ixland_fd_alloc(parent, file2);
    IXLAND_ASSERT_EQ(fd1, 0);
    IXLAND_ASSERT_EQ(fd2, 1);

    /* After insertion: refs = 2 (caller + table) */
    IXLAND_ASSERT_EQ(atomic_load(&file1->refs), 2);
    IXLAND_ASSERT_EQ(atomic_load(&file2->refs), 2);

    /* Duplicate the table */
    ixland_files_t *child = ixland_files_dup(parent);
    IXLAND_ASSERT_NOT_NULL(child);
    IXLAND_ASSERT_EQ(child->max_fds, parent->max_fds);

    /* After dup: refs = 3 (caller + parent + child) */
    IXLAND_ASSERT_EQ(atomic_load(&file1->refs), 3);
    IXLAND_ASSERT_EQ(atomic_load(&file2->refs), 3);

    /* Child should have same file object pointers */
    IXLAND_ASSERT_EQ(child->fd[0], parent->fd[0]);
    IXLAND_ASSERT_EQ(child->fd[1], parent->fd[1]);
    IXLAND_ASSERT_NULL(child->fd[2]);

    /* Free child table: refs decrements to 2 */
    ixland_files_free(child);
    IXLAND_ASSERT_EQ(atomic_load(&file1->refs), 2);
    IXLAND_ASSERT_EQ(atomic_load(&file2->refs), 2);

    /* Free parent table: refs decrements to 1 (caller's original) */
    ixland_files_free(parent);
    IXLAND_ASSERT_EQ(atomic_load(&file1->refs), 1);
    IXLAND_ASSERT_EQ(atomic_load(&file2->refs), 1);

    /* Caller must still free original file references */
    ixland_file_free(file1);
    ixland_file_free(file2);
    return true;
}

IXLAND_TEST(fdtable_dup_null_parent) {
    errno = 0;
    ixland_files_t *child = ixland_files_dup(NULL);
    IXLAND_ASSERT_NULL(child);
    IXLAND_ASSERT_EQ(errno, EINVAL);
    return true;
}

IXLAND_TEST(fdtable_dup_empty_table) {
    ixland_files_t *parent = ixland_files_alloc(TEST_MAX_FDS);
    IXLAND_ASSERT_NOT_NULL(parent);

    /* Duplicate empty table */
    ixland_files_t *child = ixland_files_dup(parent);
    IXLAND_ASSERT_NOT_NULL(child);
    IXLAND_ASSERT_EQ(child->max_fds, TEST_MAX_FDS);

    /* All slots should be NULL */
    for (size_t i = 0; i < child->max_fds; i++) {
        IXLAND_ASSERT_NULL(child->fd[i]);
    }

    ixland_files_free(parent);
    ixland_files_free(child);
    return true;
}

IXLAND_TEST(file_refcount_basic) {
    ixland_file_t *file = ixland_file_alloc();
    IXLAND_ASSERT_NOT_NULL(file);

    /* Initial refcount should be 1 */
    int refs = atomic_load(&file->refs);
    IXLAND_ASSERT_EQ(refs, 1);

    /* Dup increments refcount */
    ixland_file_t *dup = ixland_file_dup(file);
    IXLAND_ASSERT_EQ(dup, file);
    refs = atomic_load(&file->refs);
    IXLAND_ASSERT_EQ(refs, 2);

    /* Free decrements */
    ixland_file_free(file); /* Decrement to 1 */
    refs = atomic_load(&file->refs);
    IXLAND_ASSERT_EQ(refs, 1);

    /* Final free should actually free */
    ixland_file_free(dup); /* Decrement to 0, should free */

    return true;
}

IXLAND_TEST(file_dup_null) {
    ixland_file_t *dup = ixland_file_dup(NULL);
    IXLAND_ASSERT_NULL(dup);
    return true;
}

/* Note: CLOEXEC helper functions (ixland_fd_set_cloexec, ixland_fd_get_cloexec,
 * ixland_fd_close_cloexec) are declared in fdtable.h but not implemented
 * in fdtable.c. The exec subsystem directly checks flags & FD_CLOEXEC
 * from fcntl.h. Testing CLOEXEC behavior requires the exec path, not
 * these unimplemented helpers.
 */
