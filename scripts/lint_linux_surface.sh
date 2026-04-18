#!/bin/sh
set -eu

OWNER_PATHS='^(kernel|fs|runtime|include)/'
BRIDGE_EXCLUDES='^(arch/darwin|fs/.*_darwin\.(c|h|m|mm)$|fs/host_.*\.(c|h|m|mm)$)'

echo "Checking Objective-C/Objective-C++ leakage in Linux-owner paths..."
find kernel fs runtime include -type f \( -name '*.m' -o -name '*.mm' \) 2>/dev/null \
  | grep -vE '^fs/.*_darwin\.(m|mm)$|^fs/host_.*\.(m|mm)$|^arch/darwin/' \
  && { echo "FAIL: ObjC/ObjC++ leaked into Linux-owner paths"; exit 1; } || true

echo "Checking Foundation/UIKit leakage outside bridges/tests..."
rg -n \
  '^[[:space:]]*#(import|include)[[:space:]]*<(Foundation|UIKit)/|\bNS[A-Z][A-Za-z0-9_]*\b|\bNSURL\b|\bNSFileManager\b' \
  kernel fs runtime include \
  | rg -v '^arch/darwin/|^fs/.*_darwin\.(c|h|m|mm):|^fs/host_.*\.(c|h|m|mm):' \
  && { echo "FAIL: Foundation/UIKit leakage outside private bridges"; exit 1; } || true

echo "Checking forbidden host includes in Linux-owner paths..."
rg -n \
  '^[[:space:]]*#include[[:space:]]*<(pthread\.h|signal\.h|unistd\.h|sys/wait\.h|sys/types\.h|time\.h|fcntl\.h|mach/.*|Foundation/.*|UIKit/.*|TargetConditionals\.h|os/log\.h)>' \
  kernel fs runtime include \
  | rg -v "$BRIDGE_EXCLUDES" \
  && { echo "FAIL: forbidden host includes leaked into Linux-owner paths"; exit 1; } || true

echo "Checking forbidden host APIs/tokens in Linux-owner paths..."
rg -n \
  '\b(dlsym|RTLD_NEXT|RTLD_DEFAULT|dlopen|syscall|pthread_|NSFileManager|NSURL|Foundation|UIKit|os_log|__APPLE__|__MACH__|TARGET_OS_)\\b' \
  kernel fs runtime include \
  | rg -v "$BRIDGE_EXCLUDES" \
  && { echo "FAIL: forbidden host APIs/tokens leaked into Linux-owner paths"; exit 1; } || true

echo "Checking hand-defined Linux ABI constants in owner paths..."
rg -n \
  '^[[:space:]]*#define[[:space:]]+(FUTEX_|AT_|SA_|SIG[A-Z0-9_]+|O_[A-Z0-9_]+|F_[A-Z0-9_]+|RENAME_[A-Z0-9_]+)' \
  kernel fs runtime include \
  | rg -v "$BRIDGE_EXCLUDES|IX_|TEST_" \
  && { echo "FAIL: hand-defined Linux ABI constants found in owner paths"; exit 1; } || true

echo "Linux surface lint passed."