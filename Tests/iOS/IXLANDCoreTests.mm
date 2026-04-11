// IXLANDCoreTests.mm
// XCTest wrapper for ixland C test harness

#import <XCTest/XCTest.h>

// C test harness interface
extern "C" {
    int ixland_test_run_all(const char *filter);
}

@interface IXLANDCoreTests : XCTestCase
@end

@implementation IXLANDCoreTests

- (void)testAllCoreTests {
    int result = ixland_test_run_all(NULL);
    XCTAssertEqual(result, 0, @"ixland core tests failed");
}

- (void)testPIDTests {
    int result = ixland_test_run_all("pid");
    XCTAssertEqual(result, 0, @"ixland PID tests failed");
}

- (void)testFDTests {
    int result = ixland_test_run_all("fd");
    XCTAssertEqual(result, 0, @"ixland FD tests failed");
}

- (void)testExecNative {
    int result = ixland_test_run_all("exec_native");
    XCTAssertEqual(result, 0, @"ixland exec native tests failed");
}

- (void)testExecErrors {
    int result = ixland_test_run_all("exec_command_not_found");
    XCTAssertEqual(result, 0, @"ixland exec error tests failed");
}

- (void)testExecCloexec {
    int result = ixland_test_run_all("exec_cloexec");
    XCTAssertEqual(result, 0, @"ixland exec CLOEXEC tests failed");
}

- (void)testExecClassification {
    int result = ixland_test_run_all("exec_image_classification");
    XCTAssertEqual(result, 0, @"ixland exec classification tests failed");
}

- (void)testSignalMask {
    int result = ixland_test_run_all("sigprocmask");
    XCTAssertEqual(result, 0, @"ixland signal mask tests failed");
}

- (void)testSignalExecReset {
    int result = ixland_test_run_all("signal_reset_on_exec");
    XCTAssertEqual(result, 0, @"ixland signal exec reset tests failed");
}

- (void)testTaskLifecycle {
    int result = ixland_test_run_all("task_init_state|task_pid|task_refs|task_lookup|task_exit|task_sighand");
    XCTAssertEqual(result, 0, @"ixland task lifecycle tests failed");
}

- (void)testWaitBasic {
    int result = ixland_test_run_all("wait_parent");
    XCTAssertEqual(result, 0, @"ixland wait basic tests failed");
}

- (void)testWaitErrors {
    int result = ixland_test_run_all("wait_no_children|wait_wrong|wait_second_reap");
    XCTAssertEqual(result, 0, @"ixland wait error tests failed");
}

- (void)testWaitSignaled {
    int result = ixland_test_run_all("wait_signaled");
    XCTAssertEqual(result, 0, @"ixland wait signaled tests failed");
}

- (void)testWaitNoHang {
    int result = ixland_test_run_all("wnohang");
    XCTAssertEqual(result, 0, @"ixland wait WNOHANG tests failed");
}

- (void)testWaitMultiple {
    int result = ixland_test_run_all("wait_child_list|wait_refcount|wait_multiple");
    XCTAssertEqual(result, 0, @"ixland wait multiple children tests failed");
}

- (void)testProcessGroups {
    int result = ixland_test_run_all("pgrp_initial|pgrp_fork|pgrp_multiple|pgrp_child_lookup");
    XCTAssertEqual(result, 0, @"ixland process group tests failed");
}

- (void)testSessions {
    int result = ixland_test_run_all("pgrp_session|pgrp_zombie|pgrp_refcount");
    XCTAssertEqual(result, 0, @"ixland session tests failed");
}

- (void)testSignalDelivery {
    int result = ixland_test_run_all("signal_");
    XCTAssertEqual(result, 0, @"ixland signal delivery tests failed");
}

- (void)testSignalProcessGroups {
    int result = ixland_test_run_all("signal_group");
    XCTAssertEqual(result, 0, @"ixland signal process group tests failed");
}

- (void)testSignalKillpg {
    int result = ixland_test_run_all("signal_killpg");
    XCTAssertEqual(result, 0, @"ixland killpg tests failed");
}

- (void)testSignalForeground {
    int result = ixland_test_run_all("signal_shell");
    XCTAssertEqual(result, 0, @"ixland signal foreground tests failed");
}

- (void)testSignalErrors {
    int result = ixland_test_run_all("signal_invalid|signal_null|signal_invalid_pid");
    XCTAssertEqual(result, 0, @"ixland signal error tests failed");
}

- (void)testSignalWait {
    int result = ixland_test_run_all("signal_pending|signal_disposition|signal_zero");
    XCTAssertEqual(result, 0, @"ixland signal wait tests failed");
}

- (void)testSignalPending {
    int result = ixland_test_run_all("pending_mask_basic|pending_delivery|pending_multiple|pending_ignored");
    XCTAssertEqual(result, 0, @"ixland signal pending tests failed");
}

- (void)testSignalPendingErrors {
    int result = ixland_test_run_all("pending_null|pending_invalid");
    XCTAssertEqual(result, 0, @"ixland signal pending error tests failed");
}

- (void)testPathNormalization {
    int result = ixland_test_run_all("path_normalize");
    XCTAssertEqual(result, 0, @"ixland path normalization tests failed");
}

- (void)testFDTable {
    int result = ixland_test_run_all("fdtable");
    XCTAssertEqual(result, 0, @"ixland FD table tests failed");
}

- (void)testSignalStopContinue {
    int result = ixland_test_run_all("stopcont_");
    XCTAssertEqual(result, 0, @"ixland STOP/CONTINUE signal tests failed");
}

@end
