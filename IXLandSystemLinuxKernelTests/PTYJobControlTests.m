#import <XCTest/XCTest.h>

#include <errno.h>

#include "kernel/init.h"
#include "PTYJobControlContract.h"

@interface PTYJobControlTests : XCTestCase
@end

@implementation PTYJobControlTests

- (void)setUp {
    [super setUp];
    XCTAssertEqual(start_kernel(), 0, @"start_kernel must succeed before PTY job-control tests");
    XCTAssertTrue(kernel_is_booted(), @"kernel must be booted before PTY job-control tests");
}

- (void)testTIOCSPGRPRoundTrip {
    XCTAssertEqual(pty_job_control_contract_tiocspgrp_round_trip(), 0, @"errno %d", errno);
}

- (void)testBackgroundTIOCSPGRPDeliversSIGTTOU {
    XCTAssertEqual(pty_job_control_contract_background_tiocspgrp_delivers_sigttou(), 0, @"errno %d", errno);
}

- (void)testBackgroundReadDeliversSIGTTIN {
    XCTAssertEqual(pty_job_control_contract_background_read_delivers_sigttin(), 0, @"errno %d", errno);
}

- (void)testBackgroundWriteDeliversSIGTTOU {
    XCTAssertEqual(pty_job_control_contract_background_write_delivers_sigttou(), 0, @"errno %d", errno);
}

- (void)testSignalCharsTargetForegroundProcessGroup {
    XCTAssertEqual(pty_job_control_contract_signal_chars_target_foreground_pgrp(), 0, @"errno %d", errno);
}

- (void)testDetachClearsDevTtyPolicy {
    XCTAssertEqual(pty_job_control_contract_detach_clears_dev_tty_policy(), 0, @"errno %d", errno);
}

@end
