#import <XCTest/XCTest.h>

#include <errno.h>

#include "kernel/init.h"

extern int kernel_init_contract_start_kernel_creates_current_init_task(void);
extern int kernel_init_contract_init_task_identity_is_linux_shaped(void);
extern int kernel_init_contract_init_task_cwd_and_root_are_slash(void);
extern int kernel_init_contract_kernel_boot_exposes_root(void);
extern int kernel_init_contract_kernel_boot_exposes_etc_passwd(void);
extern int kernel_init_contract_kernel_boot_exposes_dev_root(void);
extern int kernel_init_contract_kernel_boot_exposes_proc_root(void);
extern int kernel_init_contract_kernel_boot_exposes_sys_root_or_documents_policy(void);
extern int kernel_init_contract_kernel_boot_exposes_tmp_and_var_cache_routes(void);
extern int kernel_init_contract_kernel_boot_stdio_policy_is_explicit(void);
extern int kernel_init_contract_proc_self_reflects_current_task(void);
extern int kernel_init_contract_proc_self_fd_reflects_boot_descriptors(void);
extern int kernel_init_contract_proc_self_fdinfo_reflects_boot_descriptors(void);
extern int kernel_init_contract_proc_self_exe_policy_before_exec_is_explicit(void);
extern int kernel_init_contract_kernel_shutdown_and_reboot_restores_init_state(void);

@interface KernelInitTests : XCTestCase
@end

@implementation KernelInitTests

- (void)setUp {
    [super setUp];
    XCTAssertEqual(start_kernel(), 0, @"start_kernel must succeed before LinuxKernel init tests");
    XCTAssertTrue(kernel_is_booted(), @"kernel must be booted before LinuxKernel init tests");
}

- (void)testStartKernelCreatesCurrentInitTask {
    XCTAssertEqual(kernel_init_contract_start_kernel_creates_current_init_task(), 0, @"errno %d", errno);
}

- (void)testInitTaskIdentityIsLinuxShaped {
    XCTAssertEqual(kernel_init_contract_init_task_identity_is_linux_shaped(), 0, @"errno %d", errno);
}

- (void)testInitTaskCwdAndRootAreSlash {
    XCTAssertEqual(kernel_init_contract_init_task_cwd_and_root_are_slash(), 0, @"errno %d", errno);
}

- (void)testKernelBootExposesRoot {
    XCTAssertEqual(kernel_init_contract_kernel_boot_exposes_root(), 0, @"errno %d", errno);
}

- (void)testKernelBootExposesEtcPasswd {
    XCTAssertEqual(kernel_init_contract_kernel_boot_exposes_etc_passwd(), 0, @"errno %d", errno);
}

- (void)testKernelBootExposesDevRoot {
    XCTAssertEqual(kernel_init_contract_kernel_boot_exposes_dev_root(), 0, @"errno %d", errno);
}

- (void)testKernelBootExposesProcRoot {
    XCTAssertEqual(kernel_init_contract_kernel_boot_exposes_proc_root(), 0, @"errno %d", errno);
}

- (void)testKernelBootExposesSysRoot {
    XCTAssertEqual(kernel_init_contract_kernel_boot_exposes_sys_root_or_documents_policy(), 0, @"errno %d", errno);
}

- (void)testKernelBootExposesTmpAndVarCacheRoutes {
    XCTAssertEqual(kernel_init_contract_kernel_boot_exposes_tmp_and_var_cache_routes(), 0, @"errno %d", errno);
}

- (void)testKernelBootStdioPolicyIsExplicit {
    XCTAssertEqual(kernel_init_contract_kernel_boot_stdio_policy_is_explicit(), 0, @"errno %d", errno);
}

- (void)testProcSelfReflectsCurrentTask {
    XCTAssertEqual(kernel_init_contract_proc_self_reflects_current_task(), 0, @"errno %d", errno);
}

- (void)testProcSelfFdReflectsBootDescriptors {
    XCTAssertEqual(kernel_init_contract_proc_self_fd_reflects_boot_descriptors(), 0, @"errno %d", errno);
}

- (void)testProcSelfFdinfoReflectsBootDescriptors {
    XCTAssertEqual(kernel_init_contract_proc_self_fdinfo_reflects_boot_descriptors(), 0, @"errno %d", errno);
}

- (void)testProcSelfExePolicyBeforeExecIsExplicit {
    XCTAssertEqual(kernel_init_contract_proc_self_exe_policy_before_exec_is_explicit(), 0, @"errno %d", errno);
}

- (void)testKernelShutdownAndRebootRestoresInitState {
    XCTAssertEqual(kernel_init_contract_kernel_shutdown_and_reboot_restores_init_state(), 0, @"errno %d", errno);
}

@end
