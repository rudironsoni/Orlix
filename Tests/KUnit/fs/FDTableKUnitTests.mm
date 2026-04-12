//
// FDTableKUnitTests.mm - KUnit white-box tests for IXLand fdtable owner
//
// White-box tests for fdtable.c operations:
// - FD allocation and deallocation
// - FD lookup and reference counting
// - cloexec semantics
// - dup/dup2 operations
// - FD inheritance on fork simulation
// - Error paths: EBADF, EINVAL, EMFILE
//

#import <XCTest/XCTest.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "fs/fdtable.h"

@interface FDTableKUnitTests : XCTestCase
@end

@implementation FDTableKUnitTests

#pragma mark - Lifecycle Tests

- (void)testFilesAlloc {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files_alloc should succeed with valid size");
    XCTAssertEqual(files->max_fds, 16, "max_fds should match allocation request");
    XCTAssertTrue(files->fd != NULL, "fd array should be allocated");
    
    if (files) {
        ixland_files_free(files);
    }
}

- (void)testFilesAllocZero {
    ixland_files_t *files = ixland_files_alloc(0);
    XCTAssertTrue(files == NULL, "files_alloc(0) should fail with NULL");
}

- (void)testFilesAllocLarge {
    ixland_files_t *files = ixland_files_alloc(65536);
    XCTAssertTrue(files != NULL, "files_alloc should succeed with large table");
    
    if (files) {
        XCTAssertEqual(files->max_fds, 65536);
        ixland_files_free(files);
    }
}

#pragma mark - FD Allocation Tests

- (void)testFdAllocAndFree {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    ixland_file_t *file = ixland_file_alloc();
    XCTAssertTrue(file != NULL, "file_alloc should succeed");
    
    int fd = ixland_fd_alloc(files, file);
    XCTAssertGreaterThanOrEqual(fd, 0, "fd_alloc should return valid FD (>=0)");
    XCTAssertLessThan(fd, 16, "FD should be within table bounds");
    
    // Verify lookup works
    ixland_file_t *found = ixland_fd_lookup(files, fd);
    XCTAssertEqual(found, file, "lookup should return the allocated file");
    
    // Free FD
    int ret = ixland_fd_free(files, fd);
    XCTAssertEqual(ret, 0, "fd_free should succeed");
    
    // Verify lookup fails after free
    found = ixland_fd_lookup(files, fd);
    XCTAssertTrue(found == NULL, "lookup after free should return NULL");
    
    ixland_files_free(files);
}

- (void)testFdAllocReturnsLowestAvailable {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    // Allocate FDs 0, 1, 2
    ixland_file_t *f0 = ixland_file_alloc();
    ixland_file_t *f1 = ixland_file_alloc();
    ixland_file_t *f2 = ixland_file_alloc();
    
    int fd0 = ixland_fd_alloc(files, f0);
    int fd1 = ixland_fd_alloc(files, f1);
    int fd2 = ixland_fd_alloc(files, f2);
    
    XCTAssertEqual(fd0, 0, "First FD should be 0");
    XCTAssertEqual(fd1, 1, "Second FD should be 1");
    XCTAssertEqual(fd2, 2, "Third FD should be 2");
    
    // Free FD 1
    ixland_fd_free(files, fd1);
    
    // Next allocation should reuse FD 1
    ixland_file_t *f3 = ixland_file_alloc();
    int fd3 = ixland_fd_alloc(files, f3);
    XCTAssertEqual(fd3, 1, "Should reuse lowest available FD (1)");
    
    ixland_files_free(files);
}

- (void)testFdAllocExhausted {
    ixland_files_t *files = ixland_files_alloc(2);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    // Allocate all FDs
    ixland_file_t *f1 = ixland_file_alloc();
    ixland_file_t *f2 = ixland_file_alloc();
    
    int fd1 = ixland_fd_alloc(files, f1);
    int fd2 = ixland_fd_alloc(files, f2);
    XCTAssertGreaterThanOrEqual(fd1, 0);
    XCTAssertGreaterThanOrEqual(fd2, 0);
    
    // Third allocation should fail with EMFILE
    ixland_file_t *f3 = ixland_file_alloc();
    int fd3 = ixland_fd_alloc(files, f3);
    XCTAssertEqual(fd3, -1, "fd_alloc should fail when table exhausted");
    XCTAssertEqual(errno, EMFILE, "errno should be EMFILE");
    
    ixland_file_free(f3);
    ixland_files_free(files);
}

#pragma mark - FD Lookup Error Paths

- (void)testFdLookupNegative {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    ixland_file_t *found = ixland_fd_lookup(files, -1);
    XCTAssertTrue(found == NULL, "lookup with negative FD should fail");
}

- (void)testFdLookupBeyondRange {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    ixland_file_t *found = ixland_fd_lookup(files, 100);
    XCTAssertTrue(found == NULL, "lookup beyond table range should fail");
}

- (void)testFdLookupUnallocated {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    ixland_file_t *found = ixland_fd_lookup(files, 5);
    XCTAssertTrue(found == NULL, "lookup of unallocated FD should fail");
}

#pragma mark - FD Free Error Paths

- (void)testFdFreeNegative {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    int ret = ixland_fd_free(files, -1);
    XCTAssertEqual(ret, -1, "free with negative FD should fail");
    XCTAssertEqual(errno, EBADF, "errno should be EBADF");
}

- (void)testFdFreeBeyondRange {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    int ret = ixland_fd_free(files, 100);
    XCTAssertEqual(ret, -1, "free beyond range should fail");
    XCTAssertEqual(errno, EBADF, "errno should be EBADF");
}

- (void)testFdFreeUnallocated {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    int ret = ixland_fd_free(files, 5);
    XCTAssertEqual(ret, -1, "free of unallocated FD should fail");
    XCTAssertEqual(errno, EBADF, "errno should be EBADF");
}

#pragma mark - Dup Tests

- (void)testFdDup {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    ixland_file_t *file = ixland_file_alloc();
    int fd = ixland_fd_alloc(files, file);
    XCTAssertGreaterThanOrEqual(fd, 0);
    
    // Dup should allocate new FD
    int newfd = ixland_fd_dup(files, fd);
    XCTAssertGreaterThanOrEqual(newfd, 0, "dup should return valid FD");
    XCTAssertNotEqual(newfd, fd, "dup should return different FD");
    
    // Both should point to same file object
    ixland_file_t *f1 = ixland_fd_lookup(files, fd);
    ixland_file_t *f2 = ixland_fd_lookup(files, newfd);
    XCTAssertEqual(f1, f2, "dup should reference same file object");
    XCTAssertEqual(f1->refs, 2, "reference count should be 2 after dup");
    
    ixland_files_free(files);
}

- (void)testFdDupInvalid {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    int newfd = ixland_fd_dup(files, -1);
    XCTAssertEqual(newfd, -1, "dup of invalid FD should fail");
    XCTAssertEqual(errno, EBADF, "errno should be EBADF");
    
    newfd = ixland_fd_dup(files, 100);
    XCTAssertEqual(newfd, -1, "dup of out-of-range FD should fail");
    XCTAssertEqual(errno, EBADF, "errno should be EBADF");
    
    newfd = ixland_fd_dup(files, 5);
    XCTAssertEqual(newfd, -1, "dup of unallocated FD should fail");
    XCTAssertEqual(errno, EBADF, "errno should be EBADF");
    
    ixland_files_free(files);
}

- (void)testFdDupWhenExhausted {
    ixland_files_t *files = ixland_files_alloc(2);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    // Fill the table
    ixland_file_t *f1 = ixland_file_alloc();
    ixland_file_t *f2 = ixland_file_alloc();
    int fd1 = ixland_fd_alloc(files, f1);
    int fd2 = ixland_fd_alloc(files, f2);
    XCTAssertGreaterThanOrEqual(fd1, 0);
    XCTAssertGreaterThanOrEqual(fd2, 0);
    
    // Dup should fail
    int newfd = ixland_fd_dup(files, fd1);
    XCTAssertEqual(newfd, -1, "dup should fail when table full");
    XCTAssertEqual(errno, EMFILE, "errno should be EMFILE");
    
    ixland_files_free(files);
}

#pragma mark - Dup2 Tests

- (void)testFdDup2Specific {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    ixland_file_t *file = ixland_file_alloc();
    int fd = ixland_fd_alloc(files, file);
    XCTAssertGreaterThanOrEqual(fd, 0);
    
    // Dup2 to specific FD number
    int ret = ixland_fd_dup2(files, fd, 10);
    XCTAssertEqual(ret, 0, "dup2 should succeed");
    
    // Both should point to same file
    ixland_file_t *f1 = ixland_fd_lookup(files, fd);
    ixland_file_t *f2 = ixland_fd_lookup(files, 10);
    XCTAssertEqual(f1, f2);
    
    ixland_files_free(files);
}

- (void)testFdDup2OverwriteExisting {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    ixland_file_t *f1 = ixland_file_alloc();
    ixland_file_t *f2 = ixland_file_alloc();
    int fd1 = ixland_fd_alloc(files, f1);
    int fd2 = ixland_fd_alloc(files, f2);
    XCTAssertGreaterThanOrEqual(fd1, 0);
    XCTAssertGreaterThanOrEqual(fd2, 0);
    
    // Dup2 fd1 onto fd2 (should overwrite)
    int ret = ixland_fd_dup2(files, fd1, fd2);
    XCTAssertEqual(ret, 0);
    
    // fd2 should now point to f1
    ixland_file_t *found = ixland_fd_lookup(files, fd2);
    XCTAssertEqual(found, f1, "fd2 should now reference f1");
    
    ixland_files_free(files);
}

- (void)testFdDup2InvalidOldfd {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    int ret = ixland_fd_dup2(files, -1, 5);
    XCTAssertEqual(ret, -1, "dup2 with invalid oldfd should fail");
    XCTAssertEqual(errno, EBADF, "errno should be EBADF");
    
    ixland_files_free(files);
}

- (void)testFdDup2NewfdBeyondRange {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    ixland_file_t *file = ixland_file_alloc();
    int fd = ixland_fd_alloc(files, file);
    
    int ret = ixland_fd_dup2(files, fd, 100);
    XCTAssertEqual(ret, -1, "dup2 with out-of-range newfd should fail");
    XCTAssertEqual(errno, EBADF, "errno should be EBADF");
    
    ixland_files_free(files);
}

#pragma mark - Cloexec Tests

- (void)testCloexecInitiallyFalse {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    ixland_file_t *file = ixland_file_alloc();
    int fd = ixland_fd_alloc(files, file);
    XCTAssertGreaterThanOrEqual(fd, 0);
    
    bool cloexec = ixland_fd_get_cloexec(files, fd);
    XCTAssertFalse(cloexec, "FD should not have cloexec initially");
    
    ixland_files_free(files);
}

- (void)testCloexecSetAndClear {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    ixland_file_t *file = ixland_file_alloc();
    int fd = ixland_fd_alloc(files, file);
    XCTAssertGreaterThanOrEqual(fd, 0);
    
    // Set cloexec
    int ret = ixland_fd_set_cloexec(files, fd, true);
    XCTAssertEqual(ret, 0, "set_cloexec should succeed");
    
    bool cloexec = ixland_fd_get_cloexec(files, fd);
    XCTAssertTrue(cloexec, "FD should have cloexec after set");
    
    // Clear cloexec
    ret = ixland_fd_set_cloexec(files, fd, false);
    XCTAssertEqual(ret, 0);
    
    cloexec = ixland_fd_get_cloexec(files, fd);
    XCTAssertFalse(cloexec, "FD should not have cloexec after clear");
    
    ixland_files_free(files);
}

- (void)testCloexecInvalidFd {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    // Get cloexec on invalid FD
    bool cloexec = ixland_fd_get_cloexec(files, -1);
    XCTAssertFalse(cloexec);
    
    // Set cloexec on invalid FD
    int ret = ixland_fd_set_cloexec(files, -1, true);
    XCTAssertEqual(ret, -1, "set_cloexec on invalid FD should fail");
    XCTAssertEqual(errno, EBADF, "errno should be EBADF");
    
    ixland_files_free(files);
}

- (void)testCloseCloexec {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    // Create FDs with and without cloexec
    ixland_file_t *f1 = ixland_file_alloc();
    ixland_file_t *f2 = ixland_file_alloc();
    int fd1 = ixland_fd_alloc(files, f1);
    int fd2 = ixland_fd_alloc(files, f2);
    
    // Set cloexec on fd1 only
    ixland_fd_set_cloexec(files, fd1, true);
    
    // Close all cloexec FDs
    int ret = ixland_fd_close_cloexec(files);
    XCTAssertEqual(ret, 1, "should close 1 cloexec FD");
    
    // fd1 should be closed, fd2 should remain
    XCTAssertTrue(ixland_fd_lookup(files, fd1) == NULL, "fd1 should be closed");
    XCTAssertTrue(ixland_fd_lookup(files, fd2) != NULL, "fd2 should remain open");
    
    ixland_files_free(files);
}

#pragma mark - Fork Inheritance Tests

- (void)testFilesDup {
    ixland_files_t *parent = ixland_files_alloc(16);
    XCTAssertTrue(parent != NULL, "parent should be allocated");
    
    // Allocate some FDs in parent
    ixland_file_t *f1 = ixland_file_alloc();
    ixland_file_t *f2 = ixland_file_alloc();
    int fd1 = ixland_fd_alloc(parent, f1);
    int fd2 = ixland_fd_alloc(parent, f2);
    XCTAssertGreaterThanOrEqual(fd1, 0);
    XCTAssertGreaterThanOrEqual(fd2, 0);
    
    // Dup files (simulates fork)
    ixland_files_t *child = ixland_files_dup(parent);
    XCTAssertTrue(child != NULL, "files_dup should succeed");
    XCTAssertEqual(child->max_fds, parent->max_fds);
    
    // Child should have same FDs
    ixland_file_t *c1 = ixland_fd_lookup(child, fd1);
    ixland_file_t *c2 = ixland_fd_lookup(child, fd2);
    XCTAssertTrue(c1 != NULL, "child should have fd1");
    XCTAssertTrue(c2 != NULL, "child should have fd2");
    
    // Should be same file objects (reference counting)
    XCTAssertEqual(c1, f1, "child fd1 should reference same file as parent");
    XCTAssertEqual(c2, f2, "child fd2 should reference same file as parent");
    
    // Reference counts should be incremented
    XCTAssertEqual(f1->refs, 2, "f1 should have ref count 2 after dup");
    XCTAssertEqual(f2->refs, 2, "f2 should have ref count 2 after dup");
    
    ixland_files_free(child);
    ixland_files_free(parent);
}

- (void)testFilesDupIsIndependent {
    ixland_files_t *parent = ixland_files_alloc(16);
    XCTAssertTrue(parent != NULL, "parent should be allocated");
    
    ixland_file_t *f1 = ixland_file_alloc();
    int fd1 = ixland_fd_alloc(parent, f1);
    XCTAssertGreaterThanOrEqual(fd1, 0);
    
    ixland_files_t *child = ixland_files_dup(parent);
    XCTAssertTrue(child != NULL, "child should be allocated");
    
    // Allocate new FD in child only
    ixland_file_t *f2 = ixland_file_alloc();
    int fd2 = ixland_fd_alloc(child, f2);
    XCTAssertGreaterThanOrEqual(fd2, 0);
    
    // Child should have both, parent should only have fd1
    XCTAssertTrue(ixland_fd_lookup(child, fd1) != NULL, "child should have fd1");
    XCTAssertTrue(ixland_fd_lookup(child, fd2) != NULL, "child should have fd2");
    XCTAssertTrue(ixland_fd_lookup(parent, fd1) != NULL, "parent should have fd1");
    XCTAssertTrue(ixland_fd_lookup(parent, fd2) == NULL, "parent should not see child's new FD");
    
    ixland_files_free(child);
    ixland_files_free(parent);
}

#pragma mark - Reference Counting Tests

- (void)testFileReferenceCounting {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    ixland_file_t *file = ixland_file_alloc();
    XCTAssertEqual(file->refs, 1, "new file should have ref count 1");
    
    int fd1 = ixland_fd_alloc(files, file);
    XCTAssertEqual(file->refs, 1, "alloc should not increment refs");
    
    int fd2 = ixland_fd_dup(files, fd1);
    XCTAssertEqual(file->refs, 2, "dup should increment refs");
    
    ixland_fd_free(files, fd1);
    XCTAssertEqual(file->refs, 1, "free should decrement refs");
    
    ixland_fd_free(files, fd2);
    // File should be freed when refs reach 0
    
    ixland_files_free(files);
}

- (void)testFileDupIncrementsRefs {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL, "files should be allocated");
    
    ixland_file_t *file = ixland_file_alloc();
    int fd = ixland_fd_alloc(files, file);
    
    int initialRefs = file->refs;
    
    // Dup the file object directly
    ixland_file_t *duped = ixland_file_dup(file);
    XCTAssertEqual(duped, file, "file_dup should return same pointer");
    XCTAssertEqual(file->refs, initialRefs + 1, "refs should be incremented");
    
    ixland_file_free(duped);
    XCTAssertEqual(file->refs, initialRefs, "refs should be decremented");

    ixland_files_free(files);
}

#pragma mark - FD Table Edge Cases (formerly in misleading OpenKUnitTests)

- (void)testCloseStdFd {
    ixland_files_t *files = ixland_files_alloc(16);
    XCTAssertTrue(files != NULL);

    int ret = ixland_fd_free(files, 0);
    XCTAssertEqual(ret, -1);
    XCTAssertEqual(errno, EBADF);

    ret = ixland_fd_free(files, 1);
    XCTAssertEqual(ret, -1);
    XCTAssertEqual(errno, EBADF);

    ret = ixland_fd_free(files, 2);
    XCTAssertEqual(ret, -1);
    XCTAssertEqual(errno, EBADF);

    ixland_files_free(files);
}

- (void)testFdReuse {
    ixland_files_t *files = ixland_files_alloc(8);
    XCTAssertTrue(files != NULL);

    ixland_file_t *f1 = ixland_file_alloc();
    int fd1 = ixland_fd_alloc(files, f1);
    XCTAssertEqual(fd1, 0);

    ixland_file_t *f2 = ixland_file_alloc();
    int fd2 = ixland_fd_alloc(files, f2);
    XCTAssertEqual(fd2, 1);

    ixland_fd_free(files, fd1);

    ixland_file_t *f3 = ixland_file_alloc();
    int fd3 = ixland_fd_alloc(files, f3);
    XCTAssertEqual(fd3, 0, "should reuse freed FD");

    ixland_files_free(files);
}

- (void)testAllocNullFile {
    ixland_files_t *files = ixland_files_alloc(8);
    XCTAssertTrue(files != NULL);

    int fd = ixland_fd_alloc(files, NULL);
    XCTAssertEqual(fd, -1, "alloc NULL file should fail");
    XCTAssertEqual(errno, EINVAL, "errno should be EINVAL");

    ixland_files_free(files);
}

- (void)testAllocNullTable {
    ixland_file_t *file = ixland_file_alloc();
    XCTAssertTrue(file != NULL);

    int fd = ixland_fd_alloc(NULL, file);
    XCTAssertEqual(fd, -1, "alloc with NULL table should fail");
    XCTAssertEqual(errno, EINVAL, "errno should be EINVAL");

    ixland_file_free(file);
}

- (void)testOpenWithMaxFds {
    ixland_files_t *files = ixland_files_alloc(1);
    XCTAssertTrue(files != NULL);
    XCTAssertEqual(files->max_fds, 1);

    ixland_file_t *file = ixland_file_alloc();
    int fd = ixland_fd_alloc(files, file);
    XCTAssertEqual(fd, 0, "should get FD 0");

    ixland_file_t *f2 = ixland_file_alloc();
    int fd2 = ixland_fd_alloc(files, f2);
    XCTAssertEqual(fd2, -1, "should fail with max_fd=1");
    XCTAssertEqual(errno, EMFILE);

    ixland_file_free(f2);
    ixland_fd_free(files, fd);
    ixland_files_free(files);
}

@end
