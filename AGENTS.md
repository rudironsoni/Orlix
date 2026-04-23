# AGENTS.md - Development Rules for IXLandSystem

## Core Implementation Pattern: `_impl()` Suffix Convention

**RULE:** All internal implementation functions that back public syscalls MUST use the `_impl()` suffix pattern.

### The Pattern

```c
/* 1. Private implementation function with _impl suffix */
static int mkdir_impl(const char *path, mode_t mode) {
    /* implementation details */
}

/* 2. Public syscall wrapper calls _impl */
__attribute__((visibility("default"))) int mkdir(const char *path, mode_t mode) {
    return mkdir_impl(path, mode);
}
```

### Why This Matters

1. **Consistency**: The entire codebase uses `*_impl()` for internal implementations
2. **Clarity**: Distinguishes between public ABI and private implementation
3. **Debugging**: Makes stack traces and symbol tables readable
4. **Architecture**: Enforces clean separation between public contract and private mediation

### Forbidden Patterns

```c
/* WRONG: Using underscore-prefixed extern declarations */
extern int _mkdir(const char *, mode_t);
static int mkdir_impl(...) { return _mkdir(...); }

/* WRONG: No _impl separation */
__attribute__((visibility("default"))) int mkdir(...) {
    /* direct implementation, no helper */
}
```

### Correct Pattern

```c
/* CORRECT: Static _impl function, public wrapper */
static int mkdir_impl(const char *path, mode_t mode) {
    /* implementation */
}

__attribute__((visibility("default"))) int mkdir(const char *path, mode_t mode) {
    return mkdir_impl(path, mode);
}
```

## Additional Rules

### 1. Linux-Shaped Public ABI
- Public syscall names MUST be canonical Linux names (`mkdir`, `open`, `mount`, etc.)
- NO branded names like `ixland_mkdir` in the public ABI
- Darwin/BSD host headers MUST NOT dictate IXLandSystem's public contract

### 2. Host Mediation is Private
- iOS/Darwin syscalls stay behind `_impl()` functions
- Never expose Darwin-specific signatures in public headers
- When Darwin headers conflict with Linux ABI, isolate the conflict privately

### 3. Objective-C/Objective-C++ Boundary
**Linux-owner paths MUST be C-only:**
- `fs/` - Linux filesystem operations
- `kernel/` - Linux kernel syscall implementations
- `runtime/` - Native runtime support
- `include/` - Public headers (Linux-facing)

**Private iOS bridge boundary:**
- `internal/ios/**` - iOS/Darwin bridge implementations (C, ObjC, ObjC++, Foundation allowed)

**Test boundary:**
- `IXLandSystemTests/**` - Test code (ObjC/ObjC++ allowed)

**Forbidden in Linux-owner paths:**
- Objective-C files (.m, .mm)
- Foundation/UIKit imports (`#import <Foundation/Foundation.h>`)
- NS-prefixed types (`NSString`, `NSArray`, `NSDictionary`, `NSObject`, etc.)
- App container APIs (`NSFileManager`, `NSURL` bookmarks, document-picker)
- **Generic host wrapper families** (`kmutex_*`, `kcond_*`, `kthread_*`, `ix_mutex_*`, etc.)
- **Direct pthread types** (`pthread_mutex_t`, `pthread_cond_t`, etc.) - use Linux futex or subsystem-specific mediation
- **Direct pthread.h includes** - host mediation must go through `internal/ios/**` narrow interfaces
- Run `scripts/lint_linux_surface.sh` before committing

**Naming:** Private bridge files under `internal/ios/**` SHALL be named by role (e.g., `backing_io.m`, `wait.c`, `clock.c`), not by platform suffixes like `*_darwin` or `host_*`.

### 3b. Host Mediation Boundary Rules

**CRITICAL: Linux-owner code MUST NOT include generic host wrapper headers.**

Forbidden patterns in Linux-owner code (`fs/`, `kernel/`, `runtime/`, `include/`):
```c
/* FORBIDDEN: Generic host wrapper types leaking into Linux-owner code */
#include "internal/ios/fs/backing_io.h"  /* Only if using host_* file I/O functions */
typedef pthread_mutex_t kmutex_t;        /* Host type leakage */
kmutex_lock_impl(...);                   /* Generic wrapper family */
```

Required pattern for host mediation:
```c
/* CORRECT: Narrow subsystem-specific interface */
/* internal/ios/fs/vfs_backing.h - VFS-specific mediation only */
int vfs_backing_open(...);    /* VFS-specific, not generic */
int vfs_backing_stat(...);    /* VFS-specific, not generic */

/* fs/vfs.c uses narrow interface */
#include "internal/ios/fs/vfs_backing.h"  /* OK: narrow, subsystem-specific */
```

**Rule:** If Linux-owner code needs host functionality, it must go through a narrow, subsystem-shaped interface in `internal/ios/**`, NOT through generic wrapper families.

### 4. Build Proof Standard
- Authoritative builds use `xcodegen` + `xcodebuild` ONLY
- `swift build` is NOT valid proof for iOS-targeted code
- Symbol verification via `nm -goU` on the static archive

### 5. Error Handling
- Use `ENOSYS` for unimplemented functionality
- Use `EINVAL` for invalid arguments
- Use `EPERM`/`EACCES` only when access is genuinely denied
- Do NOT use `EPERM` as a lazy placeholder for missing implementation
