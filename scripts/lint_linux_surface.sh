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
echo "=== Check 10: Host-truth misuse in Linux-facing tests ==="
HOST_TRUTH_TESTS=$(rg -n -e '__APPLE__|__MACH__|TARGET_OS_[A-Z0-9_]+' -e '#\s*if\s+defined\((__APPLE__|__MACH__)\)' IXLandSystemTests 2>/dev/null || true)
if [ -n "$HOST_TRUTH_TESTS" ]; then
    echo "FAIL: Host-truth assertions found in Linux-facing tests:"
    echo "$HOST_TRUTH_TESTS"
    exit 1
fi
echo "   ✓ No host-truth misuse in Linux-facing tests"

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
IX_WRAPPERS=$(rg -n '^\s*#define\s+IX_' IXLandSystemTests/*.m 2>/dev/null || true)
if [ -n "$IX_WRAPPERS" ]; then
    echo "FAIL: IX_* wrapper macros found in test files:"
    echo "$IX_WRAPPERS"
    echo "Use semantic test helpers from LinuxUAPITestSupport.h instead."
    exit 1
fi
echo "   ✓ No IX_* wrapper macros in tests"

echo ""
echo "=== Check 13: Test ABI contamination - linux_* accessor soup ==="
LINUX_ACCESSORS=$(rg -n '\blinux_[a-z0-9_]+\(\)' IXLandSystemTests/*.m 2>/dev/null || true)
if [ -n "$LINUX_ACCESSORS" ]; then
    echo "FAIL: linux_*() accessor soup found in Objective-C test files:"
    echo "$LINUX_ACCESSORS"
    echo "Use semantic test helpers (ixland_test_uapi_*) instead."
    exit 1
fi
echo "   ✓ No linux_* accessor soup in Objective-C tests"

echo ""
echo "=== Check 14: Test ABI contamination - raw Linux constants ==="
RAW_CONSTANTS=$(rg -n '\b0x54[0-9a-fA-F]{2}\b|\b0x[0-9a-fA-F]+\s*\/\*\s*TIOC' IXLandSystemTests/*.m 2>/dev/null || true)
if [ -n "$RAW_CONSTANTS" ]; then
    echo "FAIL: Raw Linux ABI constants found in test files:"
    echo "$RAW_CONSTANTS"
    echo "Source constants from LinuxUAPITestSupport.c only."
    exit 1
fi
echo "   ✓ No raw Linux ABI constants in tests"

echo ""
echo "=== Check 15: Test ABI contamination - TEST_* raw constants ==="
TEST_CONSTANTS=$(rg -n '^\s*#define\s+TEST_(AT_|RENAME_|F_|FD_)' IXLandSystemTests/*.m IXLandSystemTests/*.c 2>/dev/null || true)
if [ -n "$TEST_CONSTANTS" ]; then
    echo "FAIL: TEST_* raw constants found in test files:"
    echo "$TEST_CONSTANTS"
    echo "Use semantic test helpers instead of #define TEST_* constants."
    exit 1
fi
echo "   ✓ No TEST_* raw constants in tests"

echo ""
echo "=== Check 16: Bridge bag usage in Linux-facing tests ==="
BRIDGE_BAG=$(rg -n 'internal/ios/fs/backing_io\.h|internal/ios/fs/backing_io_decls\.h' IXLandSystemTests/*.m 2>/dev/null || true)
if [ -n "$BRIDGE_BAG" ]; then
    echo "FAIL: Broad bridge bag headers found in Linux-facing tests:"
    echo "$BRIDGE_BAG"
    echo "Use narrow forward declarations instead of broad bridge bags."
    exit 1
fi
echo "   ✓ No broad bridge bag usage in Linux-facing tests"

echo ""
echo "=== Check 17: Darwin S_IS* used as Linux proof ==="
DARWIN_STAT=$(rg -n '\bS_ISDIR\s*\(|\bS_ISLNK\s*\(|\bS_ISREG\s*\(|\bS_ISCHR\s*\(' IXLandSystemTests/*.m 2>/dev/null | rg -v 'LinuxUAPITestSupport' || true)
if [ -n "$DARWIN_STAT" ]; then
    echo "FAIL: Darwin S_IS* macros used as Linux proof in tests:"
    echo "$DARWIN_STAT"
    echo "Use ixland_test_uapi_mode_is_* helpers instead."
    exit 1
fi
echo "   ✓ No Darwin S_IS* misuse in tests"

echo ""
echo "=== All checks passed ==="
