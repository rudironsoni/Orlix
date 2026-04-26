#import <XCTest/XCTest.h>

#include <errno.h>
#include <linux/futex.h>
#include <time.h>

extern int futex(int *uaddr, int futex_op, int val,
                 const struct timespec *timeout, int *uaddr2, int val3);
extern int library_init(const void *config);
extern int library_is_initialized(void);

@interface FutexTests : XCTestCase
@end

@implementation FutexTests

- (void)setUp {
    [super setUp];
    if (!library_is_initialized()) {
        library_init(NULL);
    }
}

- (void)testFutexWaitReturnsEnosys {
    int word = 0;
    struct timespec timeout = {0, 0};

    errno = 0;
    int rc = futex(&word, FUTEX_WAIT, 0, &timeout, NULL, 0);

    XCTAssertEqual(rc, -1, @"futex FUTEX_WAIT should currently reject with -1");
    XCTAssertEqual(errno, ENOSYS, @"futex FUTEX_WAIT should set errno to ENOSYS");
}

- (void)testFutexWakeReturnsEnosys {
    int word = 0;

    errno = 0;
    int rc = futex(&word, FUTEX_WAKE, 1, NULL, NULL, 0);

    XCTAssertEqual(rc, -1, @"futex FUTEX_WAKE should currently reject with -1");
    XCTAssertEqual(errno, ENOSYS, @"futex FUTEX_WAKE should set errno to ENOSYS");
}

@end
