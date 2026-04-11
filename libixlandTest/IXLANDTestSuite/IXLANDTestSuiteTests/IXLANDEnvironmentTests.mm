//
//  IXLANDEnvironmentTests.mm
//  Environment syscall tests - 5 syscalls
//

#import <XCTest/XCTest.h>
#import "IXLANDTestHelpers.h"

@interface IXLANDEnvironmentTests : XCTestCase
@property (nonatomic, strong) IXLANDTestEnvironment *env;
@end

@implementation IXLANDEnvironmentTests

- (void)setUp {
    [super setUp];
    self.env = [[IXLANDTestEnvironment alloc] init];
    [IXLANDTestHelpers resetLibraryState];
}

- (void)tearDown {
    [self.env cleanup];
    self.env = nil;
    [super tearDown];
}

#pragma mark - getenv (1 syscall)

- (void)testGetenv_ExistingVariable_ReturnsValue {
    // Set a variable first
    setenv("IXLAND_TEST_VAR", "test_value", 1);
    
    char *value = getenv("IXLAND_TEST_VAR");
    XCTAssertNotNil([NSString stringWithUTF8String:value]);
    XCTAssertEqual(strcmp(value, "test_value"), 0);
    
    unsetenv("IXLAND_TEST_VAR");
}

- (void)testGetenv_Nonexistent_ReturnsNull {
    char *value = getenv("IXLAND_NONEXISTENT_VAR_12345");
    XCTAssertEqual(value, NULL);
}

- (void)testGetenv_Path_ReturnsValue {
    char *path = getenv("PATH");
    XCTAssertNotEqual(path, NULL);
    XCTAssertGreaterThan(strlen(path), 0);
}

#pragma mark - setenv (1 syscall)

- (void)testSetenv_CreatesNewVariable {
    int result = setenv("IXLAND_SETENV_TEST", "new_value", 0);
    XCTAssertEqual(result, 0);
    
    char *value = getenv("IXLAND_SETENV_TEST");
    XCTAssertEqual(strcmp(value, "new_value"), 0);
    
    unsetenv("IXLAND_SETENV_TEST");
}

- (void)testSetenv_OverwritesExisting {
    setenv("IXLAND_OVERWRITE_TEST", "original", 1);
    
    int result = setenv("IXLAND_OVERWRITE_TEST", "new_value", 1);
    XCTAssertEqual(result, 0);
    
    char *value = getenv("IXLAND_OVERWRITE_TEST");
    XCTAssertEqual(strcmp(value, "new_value"), 0);
    
    unsetenv("IXLAND_OVERWRITE_TEST");
}

- (void)testSetenv_NoOverwrite_KeepsOriginal {
    setenv("IXLAND_NOOVERWRITE_TEST", "original", 1);
    
    int result = setenv("IXLAND_NOOVERWRITE_TEST", "new_value", 0);
    XCTAssertEqual(result, 0);
    
    char *value = getenv("IXLAND_NOOVERWRITE_TEST");
    XCTAssertEqual(strcmp(value, "original"));
    
    unsetenv("IXLAND_NOOVERWRITE_TEST");
}

- (void)testSetenv_EmptyValue_SetsEmpty {
    int result = setenv("IXLAND_EMPTY_TEST", "", 1);
    XCTAssertEqual(result, 0);
    
    char *value = getenv("IXLAND_EMPTY_TEST");
    XCTAssertEqual(strlen(value), 0);
    
    unsetenv("IXLAND_EMPTY_TEST");
}

#pragma mark - unsetenv (1 syscall)

- (void)testUnsetenv_RemovesVariable {
    setenv("IXLAND_UNSET_TEST", "value", 1);
    XCTAssertNotEqual(getenv("IXLAND_UNSET_TEST"), NULL);
    
    int result = unsetenv("IXLAND_UNSET_TEST");
    XCTAssertEqual(result, 0);
    
    XCTAssertEqual(getenv("IXLAND_UNSET_TEST"), NULL);
}

- (void)testUnsetenv_Nonexistent_NoError {
    int result = unsetenv("IXLAND_NONEXISTENT_UNSET");
    XCTAssertEqual(result, 0);
}

- (void)testUnsetenv_AfterGetenv {
    setenv("IXLAND_GET_THEN_UNSET", "temp", 1);
    char *value = getenv("IXLAND_GET_THEN_UNSET");
    XCTAssertEqual(strcmp(value, "temp"), 0);
    
    unsetenv("IXLAND_GET_THEN_UNSET");
    XCTAssertEqual(getenv("IXLAND_GET_THEN_UNSET"), NULL);
}

#pragma mark - clearenv (1 syscall)

- (void)testClearenv_RemovesAllVariables {
    // Set some test variables
    setenv("IXLAND_CLEAR_1", "value1", 1);
    setenv("IXLAND_CLEAR_2", "value2", 1);
    
    XCTAssertNotEqual(getenv("IXLAND_CLEAR_1"), NULL);
    XCTAssertNotEqual(getenv("IXLAND_CLEAR_2"), NULL);
    
    int result = clearenv();
    XCTAssertEqual(result, 0);
    
    // Variables should be gone
    XCTAssertEqual(getenv("IXLAND_CLEAR_1"), NULL);
    XCTAssertEqual(getenv("IXLAND_CLEAR_2"), NULL);
    
    // PATH might also be gone or set to minimal
    // Note: clearenv behavior can vary by system
}

- (void)testClearenv_CanSetNewVariablesAfter {
    clearenv();
    
    int result = setenv("IXLAND_AFTER_CLEAR", "works", 1);
    XCTAssertEqual(result, 0);
    
    char *value = getenv("IXLAND_AFTER_CLEAR");
    XCTAssertEqual(strcmp(value, "works"), 0);
    
    unsetenv("IXLAND_AFTER_CLEAR");
}

#pragma mark - Integration Tests

- (void)testEnvironmentVariablePersistence {
    // Set a variable
    setenv("IXLAND_PERSIST_TEST", "persistent", 1);
    
    // Get it multiple times
    for (int i = 0; i < 100; i++) {
        char *value = getenv("IXLAND_PERSIST_TEST");
        XCTAssertEqual(strcmp(value, "persistent"), 0);
    }
    
    unsetenv("IXLAND_PERSIST_TEST");
}

- (void)testEnvironmentStress_100Variables {
    NSUInteger baseline = [IXLANDMemoryTracker currentMemoryUsage];
    
    // Create 100 environment variables
    for (int i = 0; i < 100; i++) {
        NSString *name = [NSString stringWithFormat:@"IXLAND_STRESS_%d", i];
        NSString *value = [NSString stringWithFormat:@"value_%d_data", i];
        setenv([name UTF8String], [value UTF8String], 1);
    }
    
    // Verify all exist
    for (int i = 0; i < 100; i++) {
        NSString *name = [NSString stringWithFormat:@"IXLAND_STRESS_%d", i];
        char *value = getenv([name UTF8String]);
        XCTAssertNotEqual(value, NULL);
    }
    
    // Clean up
    for (int i = 0; i < 100; i++) {
        NSString *name = [NSString stringWithFormat:@"IXLAND_STRESS_%d", i];
        unsetenv([name UTF8String]);
    }
    
    // Check memory
    IXLANDAssertNoMemoryLeak(baseline, 4096);
}

@end
