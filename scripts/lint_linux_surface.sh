#!/bin/sh
set -eu

OWNER_PATHS="fs kernel runtime include"

echo "=== Check 1: Objective-C files outside allowed paths ==="
OBJC_FILES=$(find fs kernel runtime include -type f \( -name '*.m' -o -name '*.mm' \) 2>/dev/null || true)
if [ -n "$OBJC_FILES" ]; then
    echo "FAIL: Objective-C files found in Linux-owner paths:"
    echo "$OBJC_FILES"
    exit 1
fi
echo "   ✓ No stray .m/.mm files in Linux-owner paths"

echo ""
echo "=== Check 2: Host framework imports in Linux-owner paths ==="
HOST_FRAMEWORKS=$(rg -n '^\s*#\s*(include|import)\s*<(Foundation|UIKit|CoreFoundation|CoreServices|CoreGraphics|TargetConditionals|dispatch|os)/' $OWNER_PATHS 2>/dev/null || true)
if [ -n "$HOST_FRAMEWORKS" ]; then
    echo "FAIL: Host framework imports found in Linux-owner paths:"
    echo "$HOST_FRAMEWORKS"
    exit 1
fi
echo "   ✓ No host framework imports in Linux-owner paths"

echo ""
echo "=== Check 3: Forbidden host headers in Linux-owner paths ==="
FORBIDDEN_HEADERS=$(rg -n '^\s*#\s*include\s*<(pthread\.h|dispatch/.*|mach/.*|os/log\.h|objc/.*|sys/sysctl\.h|TargetConditionals\.h|Foundation/.*|UIKit/.*|CoreFoundation/.*)>' $OWNER_PATHS 2>/dev/null || true)
if [ -n "$FORBIDDEN_HEADERS" ]; then
    echo "FAIL: Forbidden host headers in Linux-owner paths:"
    echo "$FORBIDDEN_HEADERS"
    exit 1
fi
echo "   ✓ No forbidden host headers"

echo ""
echo "=== Check 4: Forbidden host APIs/tokens in Linux-owner paths ==="
FORBIDDEN_TOKENS=$(rg -n -e '\b(dlsym|RTLD_NEXT|RTLD_DEFAULT|dlopen|pthread_[a-z_]+|objc_[a-z_]+|mach_[a-z_]+|os_log)\b' -e '\b__(APPLE|MACH)__\b' -e '\bTARGET_OS_[A-Z0-9_]+\b' -g '!include/ixland/clangd_owner_policy.h' $OWNER_PATHS 2>/dev/null || true)
if [ -n "$FORBIDDEN_TOKENS" ]; then
    echo "FAIL: Forbidden host APIs/tokens in Linux-owner paths:"
    echo "$FORBIDDEN_TOKENS"
    exit 1
fi
echo "   ✓ No forbidden host APIs/tokens in Linux-owner paths"

echo ""
echo "=== Check 5: Generic abstraction leakage in Linux-owner paths ==="
GENERIC_ABSTRACTIONS=$(rg -n -e '\b(kmutex|kcond|kthread|konce|ksig|kplatform|kbridge|ix_mutex|ix_cond|ix_thread|ix_platform|ix_bridge|platform_mutex|platform_thread|bridge_mutex|bridge_thread)_[a-z0-9_]*\b' $OWNER_PATHS 2>/dev/null || true)
if [ -n "$GENERIC_ABSTRACTIONS" ]; then
    echo "FAIL: Generic abstraction leakage in Linux-owner paths:"
    echo "$GENERIC_ABSTRACTIONS"
    echo "Use narrow subsystem-owned interfaces under internal/ios/** instead."
    exit 1
fi
echo "   ✓ No generic abstraction leakage in Linux-owner paths"

echo ""
echo "=== Check 6: Wrong-direction mediation boundaries ==="
PUBLIC_IOS=$(rg -n '^\s*#\s*include\s*"internal/ios/.+"' include 2>/dev/null || true)
if [ -n "$PUBLIC_IOS" ]; then
    echo "FAIL: Public headers in include/ must not depend on internal/ios/**:"
    echo "$PUBLIC_IOS"
    exit 1
fi
BROAD_IOS=$(rg -n '^\s*#\s*include\s*"internal/ios/.*/(bridge|platform|generic|common|helpers?|shim|host_api)[^/"]*\.h"' fs kernel runtime 2>/dev/null || true)
if [ -n "$BROAD_IOS" ]; then
    echo "FAIL: Linux-owner code includes broad mediation headers from internal/ios/**:"
    echo "$BROAD_IOS"
    exit 1
fi
echo "   ✓ No wrong-direction broad mediation includes"

echo ""
echo "=== Check 7: Wrong subsystem placement ==="
HOST_IMPL_IN_OWNER=$(rg -n '^\s*(static\s+)?[A-Za-z_][A-Za-z0-9_\s\*]*\s+host_[a-z0-9_]+_impl\s*\([^)]*\)\s*\{' fs kernel runtime include 2>/dev/null || true)
if [ -n "$HOST_IMPL_IN_OWNER" ]; then
    echo "FAIL: host_*_impl function definitions found in Linux-owner paths:"
    echo "$HOST_IMPL_IN_OWNER"
    exit 1
fi
CROSS_SUBSYSTEM=$(rg -n '^\s*#\s*include\s*"internal/ios/fs/.*"' kernel runtime 2>/dev/null || true)
if [ -n "$CROSS_SUBSYSTEM" ]; then
    echo "FAIL: kernel/runtime must not include fs-owned internal/ios mediation headers:"
    echo "$CROSS_SUBSYSTEM"
    exit 1
fi
echo "   ✓ Subsystem placement boundaries hold"

echo ""
echo "=== Check 8: Forbidden logging/debug output in product code ==="
FORBIDDEN_LOGGING=$(rg -n -e '\b(printf|fprintf|vprintf|vfprintf|puts|fputs|putc|putchar|perror|NSLog)\s*\(' -e '\bos_log\s*\(' fs kernel runtime include internal/ios 2>/dev/null || true)
if [ -n "$FORBIDDEN_LOGGING" ]; then
    echo "FAIL: Forbidden logging/debug output in product code:"
    echo "$FORBIDDEN_LOGGING"
    exit 1
fi
echo "   ✓ No forbidden logging/debug output in product code"

echo ""
echo "=== Check 9: ABI/UAPI drift indicators ==="
HANDDEFINED_ABI=$(rg -n '^\s*#define\s+(FUTEX_|AT_|SA_|SIG[A-Z0-9_]+|O_[A-Z0-9_]+|F_[A-Z0-9_]+|RENAME_[A-Z0-9_]+)' fs kernel runtime include 2>/dev/null | rg -v -e 'IX_' -e 'TEST_' -e '_IMPL' -e 'include/ixland/linux_abi_constants.h' || true)
if [ -n "$HANDDEFINED_ABI" ]; then
    echo "FAIL: Hand-defined Linux ABI constants found in Linux-owner paths:"
    echo "$HANDDEFINED_ABI"
    exit 1
fi
BRANDED_UAPI=$(rg -n -e '__attribute__\(\(visibility\("default"\)\)\)\s+.*\b(ixland_|ios_|darwin_)[A-Za-z0-9_]*\s*\(' fs kernel runtime include 2>/dev/null || true)
if [ -n "$BRANDED_UAPI" ]; then
    echo "FAIL: Branded public ABI/UAPI indicators found:"
    echo "$BRANDED_UAPI"
    exit 1
fi
echo "   ✓ No ABI/UAPI drift indicators"

echo ""
echo "=== Check 10: Test target naming ==="
# Verify no old IXLandSystemTests target in project.yml
OLD_TARGET=$(grep -E '^\s+IXLandSystemTests:' project.yml || true)
if [ -n "$OLD_TARGET" ]; then
    echo "FAIL: Old IXLandSystemTests target still exists in project.yml:"
    echo "$OLD_TARGET"
    exit 1
fi
# Verify no IXLandSystemIOSBridgeTests
WRONG_TARGET=$(grep -E '^\s+IXLandSystemIOSBridgeTests:' project.yml || true)
if [ -n "$WRONG_TARGET" ]; then
    echo "FAIL: Wrong target name IXLandSystemIOSBridgeTests in project.yml:"
    echo "$WRONG_TARGET"
    exit 1
fi
# Verify correct targets exist
KERNEL_TESTS=$(grep -E '^\s+IXLandSystemLinuxKernelTests:' project.yml || true)
if [ -z "$KERNEL_TESTS" ]; then
    echo "FAIL: IXLandSystemLinuxKernelTests target missing from project.yml"
    exit 1
fi
BRIDGE_TESTS=$(grep -E '^\s+IXLandSystemHostBridgeTests:' project.yml || true)
if [ -z "$BRIDGE_TESTS" ]; then
    echo "FAIL: IXLandSystemHostBridgeTests target missing from project.yml"
    exit 1
fi
echo "   ✓ Test targets correctly named"

echo ""
echo "=== Check 11: New broad mediation headers under internal/ios ==="
BROAD_HEADERS=$(find internal/ios -type f -name '*.h' 2>/dev/null | rg '/(bridge|platform|generic|common|helpers?|shim|host_api)[^/]*\.h$' || true)
if [ -n "$BROAD_HEADERS" ]; then
    echo "FAIL: Broad mediation headers found under internal/ios/** (must be narrow and subsystem-owned):"
    echo "$BROAD_HEADERS"
    exit 1
fi
echo "   ✓ No new broad mediation headers under internal/ios"

echo ""
echo "=== Check 12: Test ABI contamination - IX_* wrapper macros ==="
IX_WRAPPERS=$(rg -n '^\s*#define\s+IX_' IXLandSystemLinuxKernelTests/*.m IXLandSystemHostBridgeTests/*.m 2>/dev/null || true)
if [ -n "$IX_WRAPPERS" ]; then
    echo "FAIL: IX_* wrapper macros found in test files:"
    echo "$IX_WRAPPERS"
    echo "Use semantic test helpers from LinuxUAPITestSupport.h instead."
    exit 1
fi
echo "   ✓ No IX_* wrapper macros in tests"

echo ""
echo "=== Check 13: Test ABI contamination - linux_* accessor soup ==="
LINUX_ACCESSORS=$(rg -n '\blinux_[a-z0-9_]+\(\)' IXLandSystemLinuxKernelTests/*.m IXLandSystemHostBridgeTests/*.m 2>/dev/null || true)
if [ -n "$LINUX_ACCESSORS" ]; then
    echo "FAIL: linux_*() accessor soup found in Objective-C test files:"
    echo "$LINUX_ACCESSORS"
    echo "Use semantic test helpers (ixland_test_uapi_*) instead."
    exit 1
fi
echo "   ✓ No linux_* accessor soup in Objective-C tests"

echo ""
echo "=== Check 14: Test ABI contamination - raw Linux constants ==="
RAW_CONSTANTS=$(rg -n '\b0x54[0-9a-fA-F]{2}\b|\b0x[0-9a-fA-F]+\s*\/\*\s*TIOC' IXLandSystemLinuxKernelTests/*.m IXLandSystemHostBridgeTests/*.m 2>/dev/null || true)
if [ -n "$RAW_CONSTANTS" ]; then
    echo "FAIL: Raw Linux ABI constants found in test files:"
    echo "$RAW_CONSTANTS"
    echo "Source constants from LinuxUAPITestSupport.c only."
    exit 1
fi
echo "   ✓ No raw Linux ABI constants in tests"

echo ""
echo "=== Check 15: Test ABI contamination - TEST_* raw constants ==="
TEST_CONSTANTS=$(rg -n '^\s*#define\s+TEST_(AT_|RENAME_|F_|FD_)' IXLandSystemLinuxKernelTests/*.m IXLandSystemLinuxKernelTests/*.c IXLandSystemHostBridgeTests/*.m IXLandSystemHostBridgeTests/*.c 2>/dev/null || true)
if [ -n "$TEST_CONSTANTS" ]; then
    echo "FAIL: TEST_* raw constants found in test files:"
    echo "$TEST_CONSTANTS"
    echo "Use semantic test helpers instead of #define TEST_* constants."
    exit 1
fi
echo "   ✓ No TEST_* raw constants in tests"

echo ""
echo "=== Check 15b: Test ABI contamination - ixland_test_uapi_at_* helpers ==="
IX_UAPI_AT_HELPERS=$(rg -n '\bixland_test_uapi_at_' IXLandSystemLinuxKernelTests/*.m IXLandSystemLinuxKernelTests/*.c IXLandSystemHostBridgeTests/*.m IXLandSystemHostBridgeTests/*.c 2>/dev/null || true)
if [ -n "$IX_UAPI_AT_HELPERS" ]; then
    echo "FAIL: ixland_test_uapi_at_* helper soup found in test files:"
    echo "$IX_UAPI_AT_HELPERS"
    echo "Move Linux UAPI constant usage to C contract files with canonical Linux names."
    exit 1
fi
echo "   ✓ No ixland_test_uapi_at_* helper soup in tests"

echo ""
echo "=== Check 15c: Test ABI contamination - ixland_test_uapi_f_* helpers ==="
IX_UAPI_F_HELPERS=$(rg -n '\bixland_test_uapi_f_' IXLandSystemLinuxKernelTests/*.m IXLandSystemLinuxKernelTests/*.c IXLandSystemHostBridgeTests/*.m IXLandSystemHostBridgeTests/*.c 2>/dev/null || true)
if [ -n "$IX_UAPI_F_HELPERS" ]; then
    echo "FAIL: ixland_test_uapi_f_* helper soup found in test files:"
    echo "$IX_UAPI_F_HELPERS"
    echo "Move Linux UAPI constant usage to C contract files with canonical Linux names."
    exit 1
fi
echo "   ✓ No ixland_test_uapi_f_* helper soup in tests"

echo ""
echo "=== Check 16: Bridge bag usage in Linux-facing tests ==="
BRIDGE_BAG=$(rg -n 'internal/ios/fs/backing_io\.h|internal/ios/fs/backing_io_decls\.h' IXLandSystemLinuxKernelTests/*.m 2>/dev/null || true)
if [ -n "$BRIDGE_BAG" ]; then
    echo "FAIL: Broad bridge bag headers found in Linux-facing tests:"
    echo "$BRIDGE_BAG"
    echo "Use narrow forward declarations instead of broad bridge bags."
    exit 1
fi
echo "   ✓ No broad bridge bag usage in Linux-facing tests"

echo ""
echo "=== Check 17: Broken host syscall errno rewriting ==="
BROKEN_HOST_ERRNO=$(rg -n 'errno[[:space:]]*=[[:space:]]*-ret|errno[[:space:]]*=[[:space:]]*\(int\)-ret' internal/ios/fs/path_host.c internal/ios/fs/backing_io.m 2>/dev/null || true)
if [ -n "$BROKEN_HOST_ERRNO" ]; then
    echo "FAIL: Broken host syscall errno rewriting found:"
    echo "$BROKEN_HOST_ERRNO"
    exit 1
fi
echo "   ✓ No broken host syscall errno rewriting"

echo ""
echo "=== Check 17b: Test Linux UAPI contamination aliases ==="
TEST_UAPI_CONTAMINATION=$(rg -n 'include/ixland/linux_uapi_constants\.h|\bIX_(AT_|F_)|\bTEST_(AT_|F_)|\bixland_test_uapi_(at_|f_)' IXLandSystemLinuxKernelTests IXLandSystemHostBridgeTests 2>/dev/null || true)
if [ -n "$TEST_UAPI_CONTAMINATION" ]; then
    echo "FAIL: Test Linux UAPI contamination aliases found:"
    echo "$TEST_UAPI_CONTAMINATION"
    exit 1
fi
echo "   ✓ No test Linux UAPI contamination aliases"

echo ""
echo "=== Check 17c: Unified host_fstat_impl contract ==="
HOST_FSTAT_CONTRACT=$(rg -n 'host_fstat_impl\s*\([^)]*struct stat\s*\*' internal/ios/fs 2>/dev/null || true)
if [ -n "$HOST_FSTAT_CONTRACT" ]; then
    echo "FAIL: host_fstat_impl declared with host struct stat:"
    echo "$HOST_FSTAT_CONTRACT"
    exit 1
fi
echo "   ✓ host_fstat_impl uses linux_stat contract"

echo ""
echo "=== Check 18: Darwin S_IS* used as Linux proof ==="
DARWIN_STAT=$(rg -n '\bS_ISDIR\s*\(|\bS_ISLNK\s*\(|\bS_ISREG\s*\(|\bS_ISCHR\s*\(' IXLandSystemLinuxKernelTests/*.m IXLandSystemHostBridgeTests/*.m 2>/dev/null | rg -v 'LinuxUAPITestSupport' || true)
if [ -n "$DARWIN_STAT" ]; then
    echo "FAIL: Darwin S_IS* macros used as Linux proof in tests:"
    echo "$DARWIN_STAT"
    echo "Use ixland_test_uapi_mode_is_* helpers instead."
    exit 1
fi
echo "   ✓ No Darwin S_IS* misuse in tests"

echo ""
echo "=== Check 18: project.yml - Linux UAPI include path contamination ==="
# Verify IXLandSystemLinuxKernelTests does not have global Linux UAPI include paths
KERNEL_GLOBAL_UAPI=$(grep -A10 'IXLandSystemLinuxKernelTests:' project.yml | grep -E 'LINUX_UAPI_INCLUDE_ROOT\s*}' || true)
if [ -n "$KERNEL_GLOBAL_UAPI" ]; then
    echo "FAIL: IXLandSystemLinuxKernelTests has global Linux UAPI include paths:"
    echo "$KERNEL_GLOBAL_UAPI"
    exit 1
fi
# Verify IXLandSystemHostBridgeTests does not have global Linux UAPI include paths
BRIDGE_GLOBAL_UAPI=$(grep -A10 'IXLandSystemHostBridgeTests:' project.yml | grep -E 'LINUX_UAPI_INCLUDE_ROOT\s*}' || true)
if [ -n "$BRIDGE_GLOBAL_UAPI" ]; then
    echo "FAIL: IXLandSystemHostBridgeTests has global Linux UAPI include paths:"
    echo "$BRIDGE_GLOBAL_UAPI"
    exit 1
fi
# Verify LinuxUAPITestSupport.c has per-source compilerFlags
LINUX_UAPI_FLAGS=$(grep -A5 'path: IXLandSystemLinuxKernelTests/LinuxUAPITestSupport.c' project.yml | grep 'LINUX_UAPI_INCLUDE_ROOT' || true)
if [ -z "$LINUX_UAPI_FLAGS" ]; then
    echo "FAIL: LinuxUAPITestSupport.c missing per-source Linux UAPI compilerFlags"
    exit 1
fi
echo "   ✓ Linux UAPI include paths scoped to approved C test support files"

echo ""
echo "=== Check 19: Linux UAPI headers included from .m test files ==="
UAPI_IN_M=$(grep -l '#include\s*<linux/' IXLandSystemLinuxKernelTests/*.m IXLandSystemHostBridgeTests/*.m 2>/dev/null || true)
UAPI_IN_M_ASM=$(grep -l '#include\s*<asm/' IXLandSystemLinuxKernelTests/*.m IXLandSystemHostBridgeTests/*.m 2>/dev/null || true)
if [ -n "$UAPI_IN_M" ] || [ -n "$UAPI_IN_M_ASM" ]; then
    echo "FAIL: Linux UAPI headers included from Objective-C test files:"
    echo "$UAPI_IN_M"
    echo "$UAPI_IN_M_ASM"
    echo "Linux UAPI headers must only be included by approved C support files."
    exit 1
fi
echo "   ✓ No Linux UAPI headers in Objective-C test files"

echo ""
echo "=== Check 20: Host syscall declarations in test support ==="
HOST_SYSCALL_DECLS=$(rg -n 'extern\s+int\s+(ioctl|open|close|snprintf)\s*\(' IXLandSystemLinuxKernelTests/*.c IXLandSystemLinuxKernelTests/*.m IXLandSystemHostBridgeTests/*.c IXLandSystemHostBridgeTests/*.m 2>/dev/null || true)
if [ -n "$HOST_SYSCALL_DECLS" ]; then
    echo "FAIL: Host syscall forward declarations in test support files:"
    echo "$HOST_SYSCALL_DECLS"
    echo "Use proper host headers or narrow test-only helpers."
    exit 1
fi
echo "   ✓ No host syscall forward declarations"

echo ""
echo "=== Check 21: snprintf in test support ==="
SNPRINTF_USAGE=$(rg -n 'snprintf' IXLandSystemLinuxKernelTests/*.c IXLandSystemLinuxKernelTests/*.m IXLandSystemHostBridgeTests/*.c IXLandSystemHostBridgeTests/*.m 2>/dev/null || true)
if [ -n "$SNPRINTF_USAGE" ]; then
    echo "FAIL: snprintf usage found in test support files:"
    echo "$SNPRINTF_USAGE"
    echo "snprintf is forbidden; use project-approved path helpers."
    exit 1
fi
echo "   ✓ No snprintf in test support"

echo ""
echo "=== Check 22: internal/ios includes from Linux kernel tests ==="
LINUX_TEST_IOS=$(rg -n 'internal/ios' IXLandSystemLinuxKernelTests/*.m IXLandSystemLinuxKernelTests/*.c 2>/dev/null || true)
if [ -n "$LINUX_TEST_IOS" ]; then
    echo "FAIL: internal/ios includes from Linux kernel tests:"
    echo "$LINUX_TEST_IOS"
    exit 1
fi
echo "   ✓ No internal/ios includes from Linux kernel tests"

echo ""
echo "=== Check 23: Test ABI contamination - IX_* wrapper macros ==="
IX_WRAPPERS=$(rg -n '^\s*int\s+ixland_test_host_(open|close|open_readonly)\s*\(' IXLandSystemLinuxKernelTests/*.m IXLandSystemLinuxKernelTests/*.c IXLandSystemHostBridgeTests/*.m IXLandSystemHostBridgeTests/*.c 2>/dev/null || true)
if [ -n "$IX_WRAPPERS" ]; then
    echo "FAIL: IX_* wrapper macros found in test files:"
    echo "$IX_WRAPPERS"
    echo "Remove fake wrapper vocabulary; use direct target-correct includes/calls."
    exit 1
fi
echo "   ✓ No IX_* wrapper macros in tests"

echo ""
echo "=== Check 24: Test ABI contamination - linux_* accessor soup ==="
LINUX_ACCESSORS=$(rg -n '^\s*int\s+linux_\w+\s*\(' IXLandSystemLinuxKernelTests/*.m IXLandSystemLinuxKernelTests/*.c IXLandSystemHostBridgeTests/*.m IXLandSystemHostBridgeTests/*.c 2>/dev/null || true)
if [ -n "$LINUX_ACCESSORS" ]; then
    echo "FAIL: linux_*() accessor soup found in Objective-C test files:"
    echo "$LINUX_ACCESSORS"
    echo "Remove fake accessor soup; use direct target-correct includes/calls."
    exit 1
fi
echo "   ✓ No linux_* accessor soup in tests"

echo ""
echo "=== Check 25: Test ABI contamination - TEST_* raw constants ==="
TEST_CONSTANTS=$(rg -n '^\s*#define\s+TEST_' IXLandSystemLinuxKernelTests/*.m IXLandSystemLinuxKernelTests/*.c IXLandSystemHostBridgeTests/*.m IXLandSystemHostBridgeTests/*.c 2>/dev/null || true)
if [ -n "$TEST_CONSTANTS" ]; then
    echo "FAIL: TEST_* raw constants found in test files:"
    echo "$TEST_CONSTANTS"
    echo "Remove fake TEST_* constants; use semantic helpers or direct UAPI."
    exit 1
fi
echo "   ✓ No TEST_* raw constants in tests"

echo ""
echo "=== Check 26: Bridge bag usage in Linux-facing tests ==="
BRIDGE_BAG_LINUX=$(rg -n 'backing_io\.h|backing_io_decls\.h' IXLandSystemLinuxKernelTests/*.m IXLandSystemLinuxKernelTests/*.c 2>/dev/null || true)
if [ -n "$BRIDGE_BAG_LINUX" ]; then
    echo "FAIL: Broad bridge bag headers found in Linux-facing tests:"
    echo "$BRIDGE_BAG_LINUX"
    echo "Use narrow subsystem-owned interfaces under internal/ios/** instead."
    exit 1
fi
echo "   ✓ No broad bridge bag usage in Linux-facing tests"

echo ""
echo "=== Check 27: Test gutted with 'omitted for brevity' ==="
GUTTED_TESTS=$(rg -n 'omitted for brevity\|Additional owner-only tests continue here' IXLandSystemLinuxKernelTests/*.m IXLandSystemLinuxKernelTests/*.c IXLandSystemHostBridgeTests/*.m IXLandSystemHostBridgeTests/*.c 2>/dev/null || true)
if [ -n "$GUTTED_TESTS" ]; then
    echo "FAIL: Gutted tests found:"
    echo "$GUTTED_TESTS"
    echo "Restore test coverage; do not leave stubs."
    exit 1
fi
echo "   ✓ No gutted tests"

echo ""
echo "=== All checks passed ==="
