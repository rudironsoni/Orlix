// ixland_cmd_tests.m
// Unit tests for command management

#import <XCTest/XCTest.h>
#import "ixland_system/ixland_system.h"

@interface AShellCmdTests : XCTestCase
@end

@implementation AShellCmdTests

- (void)setUp {
    [super setUp];
    ashell_env_initialize();
}

- (void)tearDown {
    [super tearDown];
}

// Test command lookup for built-in commands
- (void)testBuiltInCommandsExist {
    // Test that common commands exist
    XCTAssertTrue(ashell_command_exists("cd"), @"cd command should exist");
    XCTAssertTrue(ashell_command_exists("pwd"), @"pwd command should exist");
    XCTAssertTrue(ashell_command_exists("echo"), @"echo command should exist");
}

// Test command lookup for non-existent command
- (void)testNonExistentCommand {
    XCTAssertFalse(ashell_command_exists("nonexistent_command_xyz"), 
                   @"Non-existent command should return false");
}

// Test commands as string
- (void)testCommandsAsString {
    NSString* commands = commandsAsString();
    XCTAssertTrue(commands != nil, @"Should get commands as string");
    XCTAssertTrue(commands.length > 0, @"Should have at least one command");
}

// Test commands as array
- (void)testCommandsAsArray {
    NSArray* commands = commandsAsArray();
    XCTAssertTrue(commands != nil, @"Should get commands as array");
    XCTAssertTrue(commands.count > 0, @"Should have at least one command");
}

// Test getopt string retrieval for existing command
- (void)testGetoptString {
    NSString* opts = getoptString(@"ls");
    XCTAssertTrue(opts != nil, @"Should get getopt string for ls");
}

// Test getopt for non-existent command
- (void)testGetoptStringNonExistent {
    NSString* opts = getoptString(@"nonexistent");
    // May return nil or empty string
    XCTAssertTrue(opts == nil || opts.length == 0, 
                  @"Non-existent command should have no options");
}

// Test operates on retrieval
- (void)testOperatesOn {
    NSString* operates = operatesOn(@"ls");
    XCTAssertTrue(operates != nil, @"Should get operates on string");
}

// Test command info retrieval
- (void)testCommandInfo {
    ashell_command_info_t info;
    memset(&info, 0, sizeof(info));
    
    int result = ashell_get_command_info("ls", &info);
    XCTAssertEqual(result, 0, @"Getting command info should succeed for existing command");
}

// Test command info for non-existent
- (void)testCommandInfoNonExistent {
    ashell_command_info_t info;
    memset(&info, 0, sizeof(info));
    
    int result = ashell_get_command_info("nonexistent", &info);
    XCTAssertEqual(result, -1, @"Getting command info should fail for non-existent");
}

@end
