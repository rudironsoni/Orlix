// ixland_env_tests.m
// Unit tests for environment variable management
// XNU-style: Use explicit boolean assertions for C pointers

#import <XCTest/XCTest.h>
#import "ixland_system/ixland_system.h"

@interface AShellEnvTests : XCTestCase
@end

@implementation AShellEnvTests

- (void)setUp {
    [super setUp];
}

- (void)tearDown {
    [super tearDown];
}

// Test environment initialization
- (void)testEnvironmentInitialization {
    XCTAssertTrue(ashell_env_is_initialized() != 0, @"Environment should be initialized");
}

// Test PATH manipulation - append
- (void)testPathAppend {
    const char* testDir = "/usr/local/test/bin";
    
    int result = ashell_env_path_append(testDir);
    XCTAssertEqual(result, 0, @"Appending to PATH should succeed");
    
    // Get PATH and verify it contains our directory
    char* path = ixland_getenv("PATH");
    XCTAssertTrue(path != NULL, @"PATH should exist");
    XCTAssertTrue(strstr(path, testDir) != NULL, @"PATH should contain appended directory");
}

// Test PATH manipulation - prepend
- (void)testPathPrepend {
    const char* testDir = "/priority/bin";
    
    int result = ashell_env_path_prepend(testDir);
    XCTAssertEqual(result, 0, @"Prepending to PATH should succeed");
    
    char* path = ixland_getenv("PATH");
    XCTAssertTrue(path != NULL, @"PATH should exist");
    XCTAssertTrue(strstr(path, testDir) != NULL, @"PATH should contain prepended directory");
}

// Test PATH manipulation - remove
- (void)testPathRemove {
    const char* testDir = "/tmp/remove_test";
    
    // First add it
    ashell_env_path_append(testDir);
    
    // Then remove it
    int result = ashell_env_path_remove(testDir);
    XCTAssertEqual(result, 0, @"Removing from PATH should succeed");
    
    char* path = ixland_getenv("PATH");
    XCTAssertTrue(strstr(path, testDir) == NULL, @"PATH should not contain removed directory");
}

// Test environment variable set and get
- (void)testEnvironmentVariableSetGet {
    int result = ixland_setenv("TEST_VAR", "test_value", 1);
    XCTAssertEqual(result, 0, @"Setting environment variable should succeed");
    
    char* value = ixland_getenv("TEST_VAR");
    XCTAssertTrue(value != NULL, @"Should be able to get TEST_VAR");
    XCTAssertTrue(strcmp(value, "test_value") == 0, @"Value should match");
}

// Test environment variable unset
- (void)testEnvironmentVariableUnset {
    ixland_setenv("UNSET_ME", "value", 1);
    
    int result = ixland_unsetenv("UNSET_ME");
    XCTAssertEqual(result, 0, @"Unsetting should succeed");
    
    char* value = ixland_getenv("UNSET_ME");
    XCTAssertTrue(value == NULL, @"Unset variable should be NULL");
}

// Test HOME directory
- (void)testHomeDirectory {
    char* home = ixland_getenv("HOME");
    XCTAssertTrue(home != NULL, @"HOME should be set");
    XCTAssertTrue(strlen(home) > 0, @"HOME should not be empty");
}

// Test PATH array operations
- (void)testPathArrayOperations {
    int count = 0;
    char** paths = ashell_env_get_path_array(&count);
    
    XCTAssertTrue(paths != NULL, @"Should get path array");
    XCTAssertGreaterThan(count, 0, @"Should have at least one path entry");
    
    // Free the array
    ashell_env_free_path_array(paths);
}

// Test environment print (should not crash)
- (void)testEnvironmentPrint {
    XCTAssertNoThrow(ashell_env_print(), @"Printing environment should not crash");
}

// Test PATH array with modifications
- (void)testPathArrayAfterModification {
    const char* newPath = "/test/array/path";
    ashell_env_path_append(newPath);
    
    int count = 0;
    char** paths = ashell_env_get_path_array(&count);
    
    BOOL found = NO;
    for (int i = 0; i < count; i++) {
        if (strcmp(paths[i], newPath) == 0) {
            found = YES;
            break;
        }
    }
    
    XCTAssertTrue(found, @"Path array should contain new path");
    ashell_env_free_path_array(paths);
}

@end
