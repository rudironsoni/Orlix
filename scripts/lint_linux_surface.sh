#!/bin/sh
set -eu

echo "=== Check 1: Objective-C files outside allowed paths ==="
OBJC_FILES=$(find fs kernel runtime include -type f \( -name '*.m' -o -name '*.mm' \) 2>/dev/null || true)
if [ -n "$OBJC_FILES" ]; then
    echo "FAIL: Objective-C files found in Linux-owner paths:"
    echo "$OBJC_FILES"
    exit 1
fi
echo "   ✓ No stray .m/.mm files in Linux-owner paths"

echo ""
echo "=== Check 2: Foundation/UIKit leakage outside internal/ios ==="
echo "   Checking fs/, kernel/, runtime/, include/..."
LEAK=$(rg -l '#(import|include)\s*<(Foundation|UIKit)/' fs kernel runtime include 2>/dev/null || true)
if [ -n "$LEAK" ]; then
    echo "FAIL: Foundation/UIKit includes found outside internal/ios:"
    echo "$LEAK"
    exit 1
fi
echo "   ✓ No Foundation/UIKit leakage in Linux-owner paths"

echo ""
echo "=== Check 3: Forbidden host includes in Linux-owner paths ==="
echo "   Checking for pthread.h, unistd.h, etc. in fs/, kernel/, runtime/, include/..."
FORBIDDEN=$(rg -n '^\s*#include\s*<(pthread\.h|signal\.h|unistd\.h|sys/wait\.h|sys/types\.h|time\.h|fcntl\.h|mach/.+|Foundation/.+|UIKit/.+|TargetConditionals\.h|os/log\.h)>' fs kernel runtime include 2>/dev/null || true)
if [ -n "$FORBIDDEN" ]; then
    echo "FAIL: Forbidden host includes in Linux-owner paths:"
    echo "$FORBIDDEN"
    exit 1
fi
echo "   ✓ No forbidden host includes"

echo ""
echo "=== Check 4: Forbidden host APIs/tokens ==="
echo "   Checking for dlsym, RTLD_NEXT, syscall, pthread, etc. ..."
TOKENS=$(rg -n -e '\b(dlsym|RTLD_NEXT|RTLD_DEFAULT|dlopen|syscall)\b' -e '\bpthread_\w+\b' -e '\bos_log\b' -e '\b__(APPLE|MACH)__\b' -e '\bTARGET_OS_\w+\b' -g '!include/ixland/clangd_owner_policy.h' fs kernel runtime include 2>/dev/null || true)
if [ -n "$TOKENS" ]; then
    echo "FAIL: Forbidden host APIs/tokens in Linux-owner paths:"
    echo "$TOKENS"
    exit 1
fi
echo "   ✓ No forbidden host APIs"

echo ""
echo "=== Check 5: Hand-defined Linux ABI constants ==="
echo "   Checking for FUTEX_, AT_, SA_, SIG*, O_*, etc. ..."
HANDDEFINED=$(rg -n '^\s*#define\s+(FUTEX_|AT_|SA_|SIG[A-Z0-9_]+|O_[A-Z0-9_]+|F_[A-Z0-9_]+|RENAME_[A-Z0-9_]+)' fs kernel runtime include 2>/dev/null | rg -v -e 'IX_' -e 'TEST_' -e '_IMPL' || true)
if [ -n "$HANDDEFINED" ]; then
    echo "FAIL: Hand-defined Linux ABI constants found:"
    echo "$HANDDEFINED"
    exit 1
fi
echo "   ✓ No hand-defined ABI constants"

echo ""
echo "=== Check 6: Filename sludge in Linux-owner paths ==="
echo "   Checking for host_*, *_darwin, *_storage in fs/, kernel/, runtime/, include..."
SLUDGE_COUNT=0
for dir in fs kernel runtime include; do
    if [ -d "$dir" ]; then
        while IFS= read -r file; do
            case "$file" in
                host_*|*_darwin.*|*_storage.*)
                    echo "  Found: $dir/$file"
                    SLUDGE_COUNT=$((SLUDGE_COUNT + 1))
                    ;;
            esac
        done < <(ls -1 "$dir" 2>/dev/null || true)
    fi
done
if [ "$SLUDGE_COUNT" -gt 0 ]; then
    echo "FAIL: Filename sludge found in Linux-owner paths (count: $SLUDGE_COUNT)"
    exit 1
fi
echo "   ✓ No filename sludge in Linux-owner paths"

echo ""
echo "=== Check 7: Filename sludge in internal/ios ==="
echo "   Checking for new host_*, *_darwin, *_storage patterns in internal/ios..."
SLUDGE_IOS=0
for subdir in internal/ios/fs internal/ios/kernel; do
    if [ -d "$subdir" ]; then
        while IFS= read -r file; do
            case "$file" in
                host_*|*_darwin.*|*_storage.*)
                    echo "  Found: $subdir/$file"
                    SLUDGE_IOS=$((SLUDGE_IOS + 1))
                    ;;
            esac
        done < <(ls -1 "$subdir" 2>/dev/null || true)
    fi
done
if [ "$SLUDGE_IOS" -gt 0 ]; then
    echo "FAIL: New filename sludge in internal/ios (count: $SLUDGE_IOS)"
    echo "Rename to role-based names (e.g., backing_io.m, wait.c, clock.c)"
    exit 1
fi
echo "   ✓ No filename sludge in internal/ios"

echo ""
echo "=== All checks passed ==="
