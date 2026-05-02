#import <XCTest/XCTest.h>

#include <errno.h>

#include "kernel/init.h"
#include "kernel/cred_internal.h"
#include "IXLandSystemLinuxKernelTests/CgroupContract.h"

@interface CgroupTests : XCTestCase
@end

@implementation CgroupTests

- (void)setUp {
    [super setUp];
    XCTAssertEqual(start_kernel(), 0);
    cred_reset_to_defaults();
}

- (void)testCurrentTaskStartsInRootCgroup {
    XCTAssertEqual(cgroup_contract_current_task_starts_in_root(), 0,
                   @"current task should be a member of virtual root cgroup, errno %d", errno);
}

- (void)testChildInheritsParentCgroup {
    XCTAssertEqual(cgroup_contract_child_inherits_parent_cgroup(), 0,
                   @"child tasks should inherit parent virtual cgroup membership, errno %d", errno);
}

- (void)testProcSelfCgroupReportsRoot {
    XCTAssertEqual(cgroup_contract_proc_self_cgroup_reports_root(), 0,
                   @"/proc/self/cgroup should expose cgroup v2 root membership, errno %d", errno);
}

@end
