//
//  IXLANDTestHelpers.h
//  IXLANDTestSuite
//
//  Test utilities and helpers for libixland testing
//

#import <Foundation/Foundation.h>
#import <mach/mach.h>

NS_ASSUME_NONNULL_BEGIN

// Memory tracking
@interface IXLANDMemoryTracker : NSObject
+ (NSUInteger)currentResidentSize;
+ (NSUInteger)currentVirtualSize;
+ (NSDictionary *)memoryReport;
@end

// Test environment setup
@interface IXLANDTestEnvironment : NSObject
@property (readonly, nonatomic) NSString *testDirectory;
@property (readonly, nonatomic) NSString *tempDirectory;

+ (instancetype)sharedEnvironment;
- (void)setup;
- (void)cleanup;
- (NSString *)pathForFile:(NSString *)filename;
- (void)resetLibraryState;
@end

// Performance measurement
@interface IXLANDPerformanceTimer : NSObject
@property (readonly, nonatomic) NSTimeInterval elapsedTime;
@property (readonly, nonatomic) NSUInteger iterations;

- (void)start;
- (void)stop;
- (void)measureBlock:(void (^)(void))block iterations:(NSUInteger)count;
- (NSString *)report;
@end

// Leak detection
#define IXLANDAssertNoMemoryLeak(testBlock, description) \
    do { \
        NSUInteger __baseline = [IXLANDMemoryTracker currentResidentSize]; \
        testBlock; \
        [[IXLANDTestEnvironment sharedEnvironment] cleanup]; \
        NSUInteger __current = [IXLANDMemoryTracker currentResidentSize]; \
        XCTAssertEqualWithAccuracy(__current, __baseline, 4096, @"Memory leak: %@", description); \
    } while(0)

// Stress testing macros
#define IXLANDStressTest(name, iterations) \
    - (void)test##name##_Stress_##iterations##Iterations

NS_ASSUME_NONNULL_END
