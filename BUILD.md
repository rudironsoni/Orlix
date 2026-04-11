# Building libixland for iOS

## Overview

**libixland** is designed exclusively for iOS. Building for macOS will result in runtime failures due to iOS-specific APIs and constraints.

## Prerequisites

- Xcode 14.0 or later
- iOS 16.0+ SDK
- iOS Simulator SDK
- Command line tools: `xcode-select --install`

## Build Targets

### iOS Simulator (for development)
```bash
make ios-sim
```

Builds `libixland-sim.a` targeting iOS Simulator.
- Architecture: arm64 (Apple Silicon Macs) or x86_64 (Intel Macs)
- Target: `arm64-apple-ios16.0-simulator` or `x86_64-apple-ios16.0-simulator`

### iOS Device
```bash
make ios-device
```

Builds `libixland-device.a` targeting physical iOS devices.
- Architecture: arm64
- Target: `arm64-apple-ios16.0`

### Clean Build Artifacts
```bash
make clean
```

### Show Build Configuration
```bash
make info
```

## Build Output

After successful build:
- `libixland-sim.a` - iOS Simulator static library (~213KB)
- `libixland-device.a` - iOS Device static library (~213KB)
- `src/ixland/*/*.o` - Object files (cleaned by `make clean`)

## iOS SDK Detection

The Makefile automatically detects:
- iOS Device SDK: `xcrun --sdk iphoneos --show-sdk-path`
- iOS Simulator SDK: `xcrun --sdk iphonesimulator --show-sdk-path`

If SDKs are not found, the build will fail with an error message.

## Compiler Flags

### iOS Simulator
- `-target arm64-apple-ios16.0-simulator` (or x86_64)
- `-isysroot <iPhoneSimulator.sdk>`
- `-DIXLAND_IOS_BUILD -DIXLAND_SIMULATOR_BUILD`

### iOS Device
- `-target arm64-apple-ios16.0`
- `-isysroot <iPhoneOS.sdk>`
- `-DIXLAND_IOS_BUILD`

## Important Notes

1. **iOS Only**: This library cannot run on macOS. It uses iOS-specific APIs and assumes iOS constraints.

2. **No macOS Support**: Do not attempt to build or run on macOS. The library will crash at runtime.

3. **Tests**: Tests must be run on iOS Simulator or Device, not macOS. Create an iOS test app target in Xcode to run tests.

4. **Deployment Target**: iOS 16.0+ is required.

## Troubleshooting

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

### Build Warnings
All warnings have been fixed. If you see new warnings, please report them.

## Integration with Xcode

1. Build the library: `make ios-sim` or `make ios-device`
2. Add `libixland-sim.a` or `libixland-device.a` to your Xcode project
3. Add header search path: `$(PROJECT_DIR)/include`
4. Link with `-lpthread`
5. Ensure deployment target is iOS 16.0+

## Test Suite

Tests are located in `tests/` directory but **cannot run on macOS**.

To run tests:
1. Create an iOS test app target in Xcode
2. Link against `libixland-sim.a` (Simulator) or `libixland-device.a` (Device)
3. Include test files from `tests/`
4. Build and run on iOS Simulator or Device

See `tests/ixland_test.h` for the test framework API.
