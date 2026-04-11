# iOS Test App for libixland

This directory contains a test iOS application for validating libixland functionality on iOS Simulator or Device.

## Overview

The test app runs a series of tests to verify:
- Library initialization
- Version information
- getcwd/getpid/getppid
- chdir
- Basic file operations

Results are displayed both in the console and as log output.

## Prerequisites

- Xcode 14.0 or later
- iOS 16.0+ Simulator or Device
- libixland built for the target (see below)

## Setup Instructions

### Step 1: Build libixland

```bash
cd /path/to/a-shell-kernel

# Build for iOS Simulator
make ios-sim

# Or build for iOS Device
make ios-device
```

This creates:
- `libixland-sim.a` - for iOS Simulator
- `libixland-device.a` - for physical iOS devices

### Step 2: Create Xcode Project

**Option A: Manual Setup (Recommended)**

1. Open Xcode
2. Create new project: File → New → Project
3. Select "iOS" → "App"
4. Configure:
   - Name: `IOSTestApp`
   - Organization: (your choice)
   - Interface: `Storyboard` (or SwiftUI)
   - Language: `Objective-C`
   - Minimum iOS Version: `16.0`
5. Save to: `/path/to/a-shell-kernel/ios-test-app/IOSTestApp/`

### Step 3: Configure Project

1. **Replace main.m:**
   - Delete the auto-generated `main.m`
   - Copy `IOSTestApp/IOSTestApp/main.m` from this directory to your project

2. **Add libixland library:**
   - Drag `libixland-sim.a` (or `libixland-device.a`) into your project
   - Check "Copy items if needed" and add to target

3. **Add header search path:**
   - Select project → Build Settings
   - Search for "Header Search Paths"
   - Add: `$(SRCROOT)/../../include`
   - Add: `$(SRCROOT)/../../src/ixland/internal`

4. **Link binary with libraries:**
   - Select target → Build Phases → Link Binary With Libraries
   - Add:
     - `libixland-sim.a` (or `libixland-device.a`)
     - `libpthread.tbd` (optional, usually auto-linked)

5. **Set deployment target:**
   - Ensure iOS Deployment Target is 16.0+

### Step 4: Run Tests

1. Select iOS Simulator target (iPhone 17 Pro)
2. Build and run (Cmd+R)
3. Check console output for test results:

```
[libixland] ========================================
[libixland] libixland iOS Test Suite
[libixland] ========================================

[libixland] Test 1: Library Initialization
[libixland] ✓ Library is initialized

[libixland] Test 2: Version Check
[libixland] ✓ Version: 1.0.0

[libixland] Test 3: getcwd
[libixland] ✓ CWD: /Users/.../Documents

[libixland] Test 4: getpid
[libixland] ✓ PID: 1234

[libixland] Test 5: getppid
[libixland] ✓ PPID: 1

[libixland] Test 6: chdir
[libixland] ✓ chdir to /tmp succeeded, new CWD: /tmp

[libixland] ========================================
[libixland] Test Results: 6 passed, 0 failed
[libixland] ✓ ALL TESTS PASSED
[libixland] ========================================
```

## Troubleshooting

### Library not found
```
ld: library not found for -lixland-sim
```
**Solution:** Ensure library path is correct in Build Settings → Library Search Paths

### Header not found
```
'ixland/ixland.h' file not found
```
**Solution:** Add header search paths as described in Step 3

### Architecture mismatch
```
building for iOS-simulator, but linking in object file built for macOS
```
**Solution:** Use `libixland-sim.a` for simulator, `libixland-device.a` for device

### Crash on startup
If the app crashes immediately, check:
- Constructor is running (look for "ixland: constructor started" in console)
- HOME environment variable is available in iOS context
- File permissions are correct

## Adding More Tests

To add custom tests, modify `IOSTestApp/main.m`:

```objc
// Add new test method
+ (void)test_custom_feature {
    [self log:@"Test: Custom Feature"];
    
    // Your test code here
    int result = ixland_custom_function();
    
    if (result == 0) {
        [self log:@"✓ Custom test passed"];
    } else {
        [self log:@"✗ Custom test failed"];
    }
}
```

Then add to `runAllTests:`

## File Structure

```
ios-test-app/
├── README.md                 # This file
├── IOSTestApp/
│   └── IOSTestApp/
│       ├── main.m           # Test app source
│       └── Info.plist       # App configuration
└── create-xcode-project.sh  # (Optional) Setup script
```

## Notes

- **Do not run tests on macOS** - the library is iOS-only
- The test app uses a minimal UI just to launch tests
- All output goes to the Xcode console
- Tests run automatically on app launch

## Success Criteria

✓ All 6 tests pass
✓ No crashes or exceptions
✓ Console shows "ALL TESTS PASSED"

This confirms libixland is working correctly on iOS!
