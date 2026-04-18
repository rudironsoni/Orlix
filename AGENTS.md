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
**These paths MUST be C-only (No .m or .mm files):**
- `kernel/` - Linux kernel syscall implementations
- `fs/` - Filesystem operations (except `*_darwin.c/h/m/mm` and `host_*.c/h/m/mm`)
- `runtime/` - Native runtime support
- `include/` - Public headers (Linux-facing)

**These locations allow Objective-C/Objective-C++:**
- `IXLandSystemTests/**/*.(m|mm)` - Test code
- `arch/darwin/**/*.(m|mm)` - iOS/Darwin bridge implementations
- `fs/*_darwin.(m|mm)` - Filesystem Darwin-specific bridges
- `fs/host_*.(m|mm)` - Host filesystem mediation

**Forbidden in Linux-owner paths:**
- Foundation/UIKit imports (`#import <Foundation/Foundation.h>`)
- NS-prefixed types (`NSString`, `NSArray`, `NSDictionary`, `NSObject`, etc.)
- App container APIs (`NSFileManager`, `NSURL` bookmarks, document-picker)
- Run `scripts/lint_linux_surface.sh` before committing

### 4. Build Proof Standard
- Authoritative builds use `xcodegen` + `xcodebuild` ONLY
- `swift build` is NOT valid proof for iOS-targeted code
- Symbol verification via `nm -goU` on the static archive

### 5. Error Handling
- Use `ENOSYS` for unimplemented functionality
- Use `EINVAL` for invalid arguments
- Use `EPERM`/`EACCES` only when access is genuinely denied
- Do NOT use `EPERM` as a lazy placeholder for missing implementation
