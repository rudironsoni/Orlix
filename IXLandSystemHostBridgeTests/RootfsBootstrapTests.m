/*
 * Host bridge Rootfs bootstrap tests
 * These tests require host file operations and therefore live in the
 * IXLandSystemHostBridgeTests target.
 */

#import <XCTest/XCTest.h>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fs/vfs.h"

@interface RootfsBootstrapTests : XCTestCase
@end

@implementation RootfsBootstrapTests

- (void)setUp {
    [super setUp];
}

- (void)tearDown {
    [super tearDown];
}

- (void)testVirtualEtcPasswdExists {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path("/etc/passwd", host_path, sizeof(host_path));

    XCTAssertEqual(ret, 0, @"vfs_translate_path for /etc/passwd should succeed");

    /* Verify file is accessible by opening it via host open() */
    int fd = open(host_path, O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open /etc/passwd via host should succeed");
    if (fd >= 0) close(fd);
}

- (void)testVirtualEtcGroupExists {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path("/etc/group", host_path, sizeof(host_path));

    XCTAssertEqual(ret, 0, @"vfs_translate_path for /etc/group should succeed");

    int fd = open(host_path, O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open /etc/group via host should succeed");
    if (fd >= 0) close(fd);
}

- (void)testVirtualEtcHostsExists {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path("/etc/hosts", host_path, sizeof(host_path));

    XCTAssertEqual(ret, 0, @"vfs_translate_path for /etc/hosts should succeed");

    int fd = open(host_path, O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open /etc/hosts via host should succeed");
    if (fd >= 0) close(fd);
}

- (void)testVirtualEtcResolvConfExists {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path("/etc/resolv.conf", host_path, sizeof(host_path));

    XCTAssertEqual(ret, 0, @"vfs_translate_path for /etc/resolv.conf should succeed");

    int fd = open(host_path, O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open /etc/resolv.conf via host should succeed");
    if (fd >= 0) close(fd);
}

@end
