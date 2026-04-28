/* IXLandSystemTests/KernelBootTests.m
 * Objective-C XCTest wrapper for virtual kernel boot contract tests.
 *
 * All Linux contract logic lives in KernelBoot.c (C translation unit).
 * This file only calls C test functions and asserts results.
 * It does NOT rename or re-interpret Linux contracts.
 */

#import <XCTest/XCTest.h>

#include "kernel/init.h"

@interface KernelBootTests : XCTestCase
@end

@implementation KernelBootTests

- (void)setUp {
    [super setUp];
    if (!kernel_is_booted()) {
        start_kernel();
    }
}

- (void)tearDown {
    [super tearDown];
}

- (void)testSystemIsBooted {
    XCTAssertEqual(kernel_boot_test_system_booted(), 0);
}

- (void)testVfsBackingRoots {
    XCTAssertEqual(kernel_boot_test_vfs_backing_roots(), 0);
}

- (void)testVfsRouting {
    XCTAssertEqual(kernel_boot_test_vfs_routing(), 0);
}

- (void)testSyntheticRoots {
    XCTAssertEqual(kernel_boot_test_synthetic_roots(), 0);
}

- (void)testTaskInit {
    XCTAssertEqual(kernel_boot_test_task_init(), 0);
}

- (void)testFdTable {
    XCTAssertEqual(kernel_boot_test_fd_table(), 0);
}

- (void)testBootIsIdempotent {
    XCTAssertEqual(kernel_boot_test_idempotent(), 0);
}

@end