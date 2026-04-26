/*
 * IXLandSystemTests - RootfsBootstrapTests.m
 *
 * INTERNAL RUNTIME SEMANTIC TEST.
 *
 * Tests the virtual Linux rootfs bootstrap, verifying that
 * identity/config baseline files are created and accessible
 * through IXLand's path mediation.
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

/*
 * Verify that virtual /etc/passwd exists and is readable
 * through IXLand's path mediation.
 */
- (void)testVirtualEtcPasswdExists {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"/etc/passwd".UTF8String, host_path, sizeof(host_path));

    XCTAssertEqual(ret, 0, @"vfs_translate_path for /etc/passwd should succeed");

    /* Verify file is accessible by opening it via IXLand's open() */
    extern int open(const char *, int, ...);
    int fd = open(@"/etc/passwd".UTF8String, O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open /etc/passwd through IXLand should succeed");
    if (fd >= 0) {
        close(fd);
    }
}

/*
 * Verify that virtual /etc/group exists and is readable
 * through IXLand's path mediation.
 */
- (void)testVirtualEtcGroupExists {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"/etc/group".UTF8String, host_path, sizeof(host_path));

    XCTAssertEqual(ret, 0, @"vfs_translate_path for /etc/group should succeed");

    extern int open(const char *, int, ...);
    int fd = open(@"/etc/group".UTF8String, O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open /etc/group through IXLand should succeed");
    if (fd >= 0) {
        close(fd);
    }
}

/*
 * Verify that virtual /etc/hosts exists and is readable
 * through IXLand's path mediation.
 */
- (void)testVirtualEtcHostsExists {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"/etc/hosts".UTF8String, host_path, sizeof(host_path));

    XCTAssertEqual(ret, 0, @"vfs_translate_path for /etc/hosts should succeed");

    extern int open(const char *, int, ...);
    int fd = open(@"/etc/hosts".UTF8String, O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open /etc/hosts through IXLand should succeed");
    if (fd >= 0) {
        close(fd);
    }
}

/*
 * Verify that virtual /etc/resolv.conf exists and is readable
 * through IXLand's path mediation.
 */
- (void)testVirtualEtcResolvConfExists {
    char host_path[MAX_PATH];
    int ret = vfs_translate_path(@"/etc/resolv.conf".UTF8String, host_path, sizeof(host_path));

    XCTAssertEqual(ret, 0, @"vfs_translate_path for /etc/resolv.conf should succeed");

    extern int open(const char *, int, ...);
    int fd = open(@"/etc/resolv.conf".UTF8String, O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open /etc/resolv.conf through IXLand should succeed");
    if (fd >= 0) {
        close(fd);
    }
}

/*
 * Verify that /etc/passwd has Linux-shaped content
 * (at minimum: root user entry with valid fields)
 */
- (void)testVirtualEtcPasswdContent {
    extern int open(const char *, int, ...);
    extern ssize_t read(int, void *, size_t);
    extern int close(int);

    int fd = open(@"/etc/passwd".UTF8String, O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open /etc/passwd should succeed");
    if (fd < 0) return;

    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    XCTAssertTrue(n > 0, @"read /etc/passwd should return content, n=%zd", n);
    if (n <= 0) return;

    buf[n] = '\0';

    /* Verify root user entry exists */
    XCTAssertTrue(strstr(buf, "root") != NULL,
                  @"/etc/passwd should contain root user entry");

    /* Verify ixland user entry exists */
    XCTAssertTrue(strstr(buf, "ixland:x:1000:1000:") != NULL,
                  @"/etc/passwd should contain ixland user entry");
}

/*
 * Verify that /etc/group has Linux-shaped content
 * (at minimum: root group entry with valid fields)
 */
- (void)testVirtualEtcGroupContent {
    extern int open(const char *, int, ...);
    extern ssize_t read(int, void *, size_t);
    extern int close(int);

    int fd = open(@"/etc/group".UTF8String, O_RDONLY);
    XCTAssertTrue(fd >= 0, @"open /etc/group should succeed");
    if (fd < 0) return;

    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    XCTAssertTrue(n > 0, @"read /etc/group should return content");
    if (n <= 0) return;

    buf[n] = '\0';

    /* Verify root group entry exists */
    XCTAssertTrue(strstr(buf, "root:x:0:") != NULL,
                  @"/etc/group should contain root group entry");

    /* Verify ixland group entry exists */
    XCTAssertTrue(strstr(buf, "ixland:x:1000:") != NULL,
                  @"/etc/group should contain ixland group entry");
}

@end
