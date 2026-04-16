# Building IXLandSystem for iOS

## Overview

**IXLandSystem** is an iOS Subsystem for Linux (iOS → Linux syscall translation layer). Building for macOS will result in runtime failures due to iOS-specific APIs and constraints.

## Prerequisites

- Xcode 14.0 or later
- iOS 16.0+ SDK
- iOS Simulator SDK
- XcodeGen: `brew install xcodegen`
- Command line tools: `xcode-select --install`

## Build Process (Authoritative)

IXLandSystem uses **XcodeGen** to generate the Xcode project from `project.yml`, then **xcodebuild** to compile for iOS.

### Step 1: Generate Xcode Project

```bash
xcodegen generate --project .
```

This creates `IXLandSystem.xcodeproj` from the spec in `project.yml`.

### Step 2: Build for iOS Simulator

```bash
xcodebuild -project IXLandSystem.xcodeproj \
  -scheme IXLandSystem-6.12-arm64 \
  -sdk iphonesimulator \
  -arch arm64 \
  -configuration Debug \
  build
```

### Step 3: Run Canonical Tests

```bash
xcodebuild test -project IXLandSystem.xcodeproj \
  -scheme IXLandSystem-6.12-arm64 \
  -sdk iphonesimulator \
  -configuration Debug \
  -destination 'platform=iOS Simulator,name=iPhone 17'
```

Do not add `-arch` to the test invocation when the simulator destination already defines the runtime/architecture.

**Output:** `build/Debug-iphonesimulator/libIXLandSystem.a`

### Step 3: Verify Build

```bash
# Check artifact type
file build/Debug-iphonesimulator/libIXLandSystem.a

# Expected output:
# build/Debug-iphonesimulator/libIXLandSystem.a: current ar archive

# Check exported symbols
nm -g build/Debug-iphonesimulator/libIXLandSystem.a | grep "_open"

# Expected output shows exported symbols like:
# 000000000000021c T _open
```

## Build Output

After successful build:
- `build/Debug-iphonesimulator/libIXLandSystem.a` - iOS Simulator static library
- `build/IXLandSystem.build/` - Build intermediates and object files

## Important Notes

1. **iOS Only**: This library cannot run on macOS. It uses iOS-specific APIs and assumes iOS constraints.

2. **No macOS Support**: Do not attempt to build or run on macOS. The library will crash at runtime.

3. **swift build is NOT authoritative**: `swift build` compiles for the host (macOS) and does not produce iOS binaries. It may be used as a smoke test but NEVER as authoritative proof.

4. **Tests**: Tests must be run on iOS Simulator or Device, not macOS.

5. **Deployment Target**: iOS 16.0+ is required.

## Troubleshooting

### "xcodegen: command not found"
Install XcodeGen:
```bash
brew install xcodegen
```

### "iOS SDK not found"
Install Xcode and ensure iOS SDK is available:
```bash
xcode-select --install
```

### "Command not found: clang"
Ensure Xcode command line tools are installed:
```bash
sudo xcode-select --reset
```

## Integration with Xcode

1. Generate project: `xcodegen generate --project .`
2. Open `IXLandSystem.xcodeproj`
3. Add `libIXLandSystem.a` to your target
4. Add header search path: `$(PROJECT_DIR)/include`
5. Link with `-lpthread`
6. Ensure deployment target is iOS 16.0+
