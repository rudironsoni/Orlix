//
//  IXLANDTimeTests.mm
//  IXLANDTestSuiteTests
//
//  Time syscall tests (sleep, usleep, nanosleep)
//

#import <XCTest/XCTest.h>
#import <unistd.h>
#import <time.h>
#import <sys/time.h>
#import "IXLANDTestHelpers.h"

@interface IXLANDTimeTests : XCTestCase
@end

@implementation IXLANDTimeTests

- (void)setUp {
    [super setUp];
    IXLANDResetLibraryState();
}

- (void)tearDown {
    IXLANDResetLibraryState();
    [super tearDown];
}

// MARK: - sleep Tests

- (void)testSleep_OneSecond {
    NSTimeInterval start = [[NSDate date] timeIntervalSince1970];
    
    unsigned int result = sleep(1);
    
    NSTimeInterval end = [[NSDate date] timeIntervalSince1970];
    NSTimeInterval elapsed = end - start;
    
    XCTAssertEqual(result, 0, @"sleep should return 0 on success");
    XCTAssertGreaterThanOrEqual(elapsed, 0.9, @"Should sleep at least 0.9 seconds");
    XCTAssertLessThan(elapsed, 2.0, @"Should not take longer than 2 seconds");
}

- (void)testSleep_ZeroSeconds {
    NSTimeInterval start = [[NSDate date] timeIntervalSince1970];
    
    unsigned int result = sleep(0);
    
    NSTimeInterval end = [[NSDate date] timeIntervalSince1970];
    NSTimeInterval elapsed = end - start;
    
    XCTAssertEqual(result, 0, @"sleep(0) should return 0");
    XCTAssertLessThan(elapsed, 0.1, @"sleep(0) should return immediately");
}

- (void)testSleep_ShortDuration {
    NSTimeInterval start = [[NSDate date] timeIntervalSince1970];
    
    // Try to sleep for 2 seconds
    unsigned int result = sleep(2);
    
    NSTimeInterval end = [[NSDate date] timeIntervalSince1970];
    NSTimeInterval elapsed = end - start;
    
    XCTAssertEqual(result, 0, @"sleep should return 0");
    XCTAssertGreaterThanOrEqual(elapsed, 1.9, @"Should sleep at least 1.9 seconds");
    XCTAssertLessThan(elapsed, 3.0, @"Should complete within 3 seconds");
}

- (void)testSleep_Interrupted {
    // Note: Actually testing interrupt requires signals, which may not work in test environment
    // This test just verifies sleep works without interruption
    unsigned int result = sleep(0);
    XCTAssertEqual(result, 0, @"sleep without signal should return 0");
}

// MARK: - usleep Tests

- (void)testUsleep_Microseconds {
    NSTimeInterval start = [[NSDate date] timeIntervalSince1970];
    
    int result = usleep(100000); // 100ms
    
    NSTimeInterval end = [[NSDate date] timeIntervalSince1970];
    NSTimeInterval elapsed = end - start;
    
    XCTAssertEqual(result, 0, @"usleep should return 0 on success");
    XCTAssertGreaterThanOrEqual(elapsed, 0.08, @"Should sleep at least 80ms");
    XCTAssertLessThan(elapsed, 0.5, @"Should complete within 500ms");
}

- (void)testUsleep_Zero {
    NSTimeInterval start = [[NSDate date] timeIntervalSince1970];
    
    int result = usleep(0);
    
    NSTimeInterval end = [[NSDate date] timeIntervalSince1970];
    NSTimeInterval elapsed = end - start;
    
    XCTAssertEqual(result, 0, @"usleep(0) should return 0");
    XCTAssertLessThan(elapsed, 0.05, @"usleep(0) should return quickly");
}

- (void)testUsleep_OneMillisecond {
    NSTimeInterval start = [[NSDate date] timeIntervalSince1970];
    
    int result = usleep(1000); // 1ms
    
    NSTimeInterval end = [[NSDate date] timeIntervalSince1970];
    NSTimeInterval elapsed = end - start;
    
    XCTAssertEqual(result, 0, @"usleep should return 0");
    // On iOS, very short sleeps may be rounded up due to scheduling granularity
    XCTAssertGreaterThanOrEqual(elapsed, 0.0005, @"Should sleep at least 0.5ms");
    XCTAssertLessThan(elapsed, 0.1, @"Should complete within 100ms");
}

- (void)testUsleep_LargeValue {
    // usleep only accepts useconds_t (usually up to 1000000)
    // Large values may fail or be truncated
    int result = usleep(2000000); // 2 seconds - might fail
    
    // Result depends on implementation - don't assert, just log
    NSLog(@"usleep(2000000) result: %d, errno: %d", result, errno);
}

// MARK: - nanosleep Tests

- (void)testNanosleep_Basic {
    struct timespec req = { .tv_sec = 0, .tv_nsec = 100000000 }; // 100ms
    struct timespec rem;
    
    NSTimeInterval start = [[NSDate date] timeIntervalSince1970];
    
    int result = nanosleep(&req, &rem);
    
    NSTimeInterval end = [[NSDate date] timeIntervalSince1970];
    NSTimeInterval elapsed = end - start;
    
    XCTAssertEqual(result, 0, @"nanosleep should return 0 on success");
    XCTAssertGreaterThanOrEqual(elapsed, 0.08, @"Should sleep at least 80ms");
    XCTAssertLessThan(elapsed, 0.5, @"Should complete within 500ms");
}

- (void)testNanosleep_Seconds {
    struct timespec req = { .tv_sec = 1, .tv_nsec = 0 };
    struct timespec rem;
    
    NSTimeInterval start = [[NSDate date] timeIntervalSince1970];
    
    int result = nanosleep(&req, &rem);
    
    NSTimeInterval end = [[NSDate date] timeIntervalSince1970];
    NSTimeInterval elapsed = end - start;
    
    XCTAssertEqual(result, 0, @"nanosleep should return 0");
    XCTAssertGreaterThanOrEqual(elapsed, 0.9, @"Should sleep at least 0.9 seconds");
    XCTAssertLessThan(elapsed, 2.0, @"Should complete within 2 seconds");
}

- (void)testNanosleep_Milliseconds {
    struct timespec req = { .tv_sec = 0, .tv_nsec = 50000000 }; // 50ms
    struct timespec rem;
    
    NSTimeInterval start = [[NSDate date] timeIntervalSince1970];
    
    int result = nanosleep(&req, &rem);
    
    NSTimeInterval end = [[NSDate date] timeIntervalSince1970];
    NSTimeInterval elapsed = end - start;
    
    XCTAssertEqual(result, 0, @"nanosleep should return 0");
    XCTAssertGreaterThanOrEqual(elapsed, 0.04, @"Should sleep at least 40ms");
    XCTAssertLessThan(elapsed, 0.2, @"Should complete within 200ms");
}

- (void)testNanosleep_Zero {
    struct timespec req = { .tv_sec = 0, .tv_nsec = 0 };
    struct timespec rem;
    
    NSTimeInterval start = [[NSDate date] timeIntervalSince1970];
    
    int result = nanosleep(&req, &rem);
    
    NSTimeInterval end = [[NSDate date] timeIntervalSince1970];
    NSTimeInterval elapsed = end - start;
    
    XCTAssertEqual(result, 0, @"nanosleep(0) should return 0");
    XCTAssertLessThan(elapsed, 0.05, @"Should return immediately");
}

- (void)testNanosleep_NullRemainder {
    struct timespec req = { .tv_sec = 0, .tv_nsec = 10000000 }; // 10ms
    
    // Pass NULL for remainder (valid when not expecting interruption)
    int result = nanosleep(&req, NULL);
    
    XCTAssertEqual(result, 0, @"nanosleep with NULL remainder should succeed");
}

- (void)testNanosleep_InvalidTime {
    struct timespec req = { .tv_sec = -1, .tv_nsec = 0 }; // Negative seconds
    struct timespec rem;
    
    int result = nanosleep(&req, &rem);
    
    // Should fail with EINVAL
    XCTAssertEqual(result, -1, @"nanosleep with negative time should fail");
    XCTAssertEqual(errno, EINVAL, @"Should set EINVAL");
}

- (void)testNanosleep_InvalidNsec {
    struct timespec req = { .tv_sec = 0, .tv_nsec = 1000000000 }; // 1 billion nanoseconds = 1 second, but may be rejected
    struct timespec rem;
    
    int result = nanosleep(&req, &rem);
    
    // Behavior varies: some implementations accept it, others reject
    if (result == -1) {
        XCTAssertEqual(errno, EINVAL, @"Should set EINVAL if rejected");
    } else {
        // Acceptable - slept for 1 second
        NSLog(@"nanosleep accepted 1 billion nanoseconds");
    }
}

// MARK: - gettimeofday Tests (bonus)

- (void)testGettimeofday_Basic {
    struct timeval tv;
    struct timezone tz;
    
    int result = gettimeofday(&tv, &tz);
    
    XCTAssertEqual(result, 0, @"gettimeofday should succeed");
    XCTAssertGreaterThan(tv.tv_sec, 0, @"tv_sec should be positive (after 1970)");
    XCTAssertGreaterThanOrEqual(tv.tv_usec, 0, @"tv_usec should be >= 0");
    XCTAssertLessThan(tv.tv_usec, 1000000, @"tv_usec should be < 1000000");
}

- (void)testGettimeofday_NullTimezone {
    struct timeval tv;
    
    int result = gettimeofday(&tv, NULL);
    
    XCTAssertEqual(result, 0, @"gettimeofday with NULL timezone should succeed");
    XCTAssertGreaterThan(tv.tv_sec, 0, @"tv_sec should be positive");
}

- (void)testGettimeofday_Monotonicity {
    // Verify time moves forward
    struct timeval tv1, tv2;
    
    int result1 = gettimeofday(&tv1, NULL);
    usleep(10000); // 10ms
    int result2 = gettimeofday(&tv2, NULL);
    
    XCTAssertEqual(result1, 0, @"First gettimeofday should succeed");
    XCTAssertEqual(result2, 0, @"Second gettimeofday should succeed");
    
    // tv2 should be >= tv1
    BOOL timeAdvanced = (tv2.tv_sec > tv1.tv_sec) ||
                        (tv2.tv_sec == tv1.tv_sec && tv2.tv_usec > tv1.tv_usec);
    XCTAssertTrue(timeAdvanced, @"Time should advance between calls");
}

// MARK: - Stress Tests

- (void)testTimeStress_SleepIterations {
    NSTimeInterval start = [[NSDate date] timeIntervalSince1970];
    
    for (int i = 0; i < 100; i++) {
        unsigned int result = sleep(0); // sleep(0) should return immediately
        XCTAssertEqual(result, 0, @"sleep(0) iteration %d should return 0", i);
    }
    
    NSTimeInterval end = [[NSDate date] timeIntervalSince1970];
    NSTimeInterval elapsed = end - start;
    
    // 100 calls to sleep(0) should be very fast
    XCTAssertLessThan(elapsed, 1.0, @"100 iterations should complete within 1 second");
    
    IXLANDAssertNoMemoryLeak(@"Sleep stress test should not leak");
}

- (void)testTimeStress_UsleepIterations {
    NSTimeInterval start = [[NSDate date] timeIntervalSince1970];
    
    for (int i = 0; i < 100; i++) {
        int result = usleep(1000); // 1ms each
        XCTAssertEqual(result, 0, @"usleep iteration %d should succeed", i);
    }
    
    NSTimeInterval end = [[NSDate date] timeIntervalSince1970];
    NSTimeInterval elapsed = end - start;
    
    // 100 x 1ms = 100ms, but allow for overhead
    XCTAssertGreaterThanOrEqual(elapsed, 0.08, @"Should sleep at least 80ms");
    XCTAssertLessThan(elapsed, 2.0, @"Should complete within 2 seconds (allowing overhead)");
    
    IXLANDAssertNoMemoryLeak(@"Usleep stress test should not leak");
}

- (void)testTimeStress_NanosleepIterations {
    NSTimeInterval start = [[NSDate date] timeIntervalSince1970];
    
    struct timespec req = { .tv_sec = 0, .tv_nsec = 1000000 }; // 1ms
    struct timespec rem;
    
    for (int i = 0; i < 100; i++) {
        int result = nanosleep(&req, &rem);
        XCTAssertEqual(result, 0, @"nanosleep iteration %d should succeed", i);
    }
    
    NSTimeInterval end = [[NSDate date] timeIntervalSince1970];
    NSTimeInterval elapsed = end - start;
    
    // 100 x 1ms = 100ms
    XCTAssertGreaterThanOrEqual(elapsed, 0.08, @"Should sleep at least 80ms");
    XCTAssertLessThan(elapsed, 2.0, @"Should complete within 2 seconds");
    
    IXLANDAssertNoMemoryLeak(@"Nanosleep stress test should not leak");
}

- (void)testTimePrecisionComparison {
    // Compare sleep functions for consistency
    NSTimeInterval elapsedSleep, elapsedUsleep, elapsedNanosleep;
    
    // Test sleep (granularity is 1 second, so use 0)
    NSDate *start1 = [NSDate date];
    sleep(0);
    elapsedSleep = [[NSDate date] timeIntervalSinceDate:start1];
    
    // Test usleep (100ms)
    NSDate *start2 = [NSDate date];
    usleep(100000);
    elapsedUsleep = [[NSDate date] timeIntervalSinceDate:start2];
    
    // Test nanosleep (100ms)
    struct timespec req = { .tv_sec = 0, .tv_nsec = 100000000 };
    struct timespec rem;
    NSDate *start3 = [NSDate date];
    nanosleep(&req, &rem);
    elapsedNanosleep = [[NSDate date] timeIntervalSinceDate:start3];
    
    NSLog(@"Timing comparison - sleep(0): %.4fs, usleep(100ms): %.4fs, nanosleep(100ms): %.4fs",
          elapsedSleep, elapsedUsleep, elapsedNanosleep);
    
    // usleep and nanosleep of same duration should be similar
    NSTimeInterval diff = fabs(elapsedUsleep - elapsedNanosleep);
    XCTAssertLessThan(diff, 0.05, @"usleep and nanosleep should have similar timing (within 50ms)");
}

@end
