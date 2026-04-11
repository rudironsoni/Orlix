# IXLandSystem Tests

Unit and integration tests for IXLandSystem.

## Contents

- `unit/` - Core unit tests (C)
- `iOS/` - iOS-specific tests
- `harness/` - Test harness utilities
- `fixtures/` - Test fixtures and data

## Running Tests

Tests are built and run through the Xcode project:

```bash
cd IXLand
xcodebuild -project IXLand.xcodeproj -scheme IXLandTests -sdk iphonesimulator -destination 'platform=iOS Simulator,name=iPhone 17 Pro' test
```

## Test Organization

- Unit tests: Low-level component testing
- iOS tests: Platform-specific integration testing
- Harness: Shared test utilities

## Notes

Tests use XCTest framework and require iOS Simulator or device.
