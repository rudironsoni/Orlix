#!/bin/bash
# Package libixland and libiwasm as XCFrameworks for iOS distribution
# Creates: LibIXLAND.xcframework and LibIWasm.xcframework

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="$PROJECT_ROOT/build/xcframeworks"

echo "========================================"
echo "Creating XCFrameworks for iOS"
echo "========================================"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Check for required libraries
if [ ! -f "$PROJECT_ROOT/lib/libixland-device.a" ] || [ ! -f "$PROJECT_ROOT/lib/libixland-sim.a" ]; then
    echo "Error: libixland libraries not found. Run: make ixland-device && make ixland-sim"
    exit 1
fi

if [ ! -f "$PROJECT_ROOT/build/wamr-device/libiwasm.a" ] || [ ! -f "$PROJECT_ROOT/build/wamr-simulator/libiwasm.a" ]; then
    echo "Error: libiwasm libraries not found. Run: ./build_wamr_ios_static.sh all"
    exit 1
fi

echo "Packaging LibIXLAND.xcframework..."

# Create LibIXLAND.xcframework
xcodebuild -create-xcframework \
    -library "$PROJECT_ROOT/lib/libixland-device.a" \
    -headers "$PROJECT_ROOT/include" \
    -library "$PROJECT_ROOT/lib/libixland-sim.a" \
    -headers "$PROJECT_ROOT/include" \
    -output "$OUTPUT_DIR/LibIXLAND.xcframework"

echo "✓ Created LibIXLAND.xcframework"
echo ""

echo "Packaging LibIWasm.xcframework..."

# Create LibIWasm.xcframework
xcodebuild -create-xcframework \
    -library "$PROJECT_ROOT/build/wamr-device/libiwasm.a" \
    -headers "$PROJECT_ROOT/deps/wamr/core/iwasm/include" \
    -library "$PROJECT_ROOT/build/wamr-simulator/libiwasm.a" \
    -headers "$PROJECT_ROOT/deps/wamr/core/iwasm/include" \
    -output "$OUTPUT_DIR/LibIWasm.xcframework"

echo "✓ Created LibIWasm.xcframework"
echo ""

# Verify frameworks
echo "Verifying frameworks..."
ls -lh "$OUTPUT_DIR"/*.xcframework
echo ""

# Create distribution package
echo "Creating distribution package..."
DIST_DIR="$OUTPUT_DIR/libixland-ios-sdk"
mkdir -p "$DIST_DIR"

# Copy frameworks
cp -R "$OUTPUT_DIR/LibIXLAND.xcframework" "$DIST_DIR/"
cp -R "$OUTPUT_DIR/LibIWasm.xcframework" "$DIST_DIR/"

# Copy headers
cp -R "$PROJECT_ROOT/include" "$DIST_DIR/"

# Create README
cat > "$DIST_DIR/README.md" << 'EOF'
# libixland iOS SDK

## Contents

- **LibIXLAND.xcframework** - iOS Subsystem for Linux (syscalls, VFS, networking)
- **LibIWasm.xcframework** - WebAssembly Micro Runtime (WAMR)
- **include/** - Public headers

## Supported Platforms

- iOS 16.0+ (arm64 device)
- iOS Simulator (arm64/x86_64)

## Integration

### Xcode

1. Drag both `.xcframework` files into your project
2. Add to your target's **Frameworks, Libraries, and Embedded Content**
3. Import headers:
   ```objc
   #include <ixland/ixland.h>
   #include <ixland/ixland_wamr.h>
   ```

### Swift Package Manager

Add to `Package.swift`:
```swift
.binaryTarget(
    name: "LibIXLAND",
    path: "LibIXLAND.xcframework"
),
.binaryTarget(
    name: "LibIWasm",
    path: "LibIWasm.xcframework"
)
```

## API Usage

### Basic Syscalls
```c
#include <ixland/ixland.h>

pid_t pid = ixland_getpid();
char cwd[1024];
ixland_getcwd(cwd, sizeof(cwd));
ixland_chdir("/tmp");
```

### WebAssembly
```c
#include <ixland/ixland_wamr.h>

ixland_wamr_init();
ixland_wamr_module_t *mod = ixland_wamr_load_module(wasm_data, wasm_size);
int32_t result = ixland_wamr_call_function(mod, "main", NULL, 0);
ixland_wamr_unload_module(mod);
ixland_wamr_deinit();
```

## Architecture

- **LibIXLAND** - Kernel interposition layer providing Linux syscall compatibility
- **LibIWasm** - WebAssembly runtime with WASI support
- **App** - Your application links both frameworks

## Notes

- Do not use raw WAMR APIs directly
- Use only `ixland_*` and `ixland_wamr_*` functions
- Both frameworks must be linked together
EOF

# Create version file
echo "1.0.0" > "$DIST_DIR/VERSION"

# Create zip archive
cd "$OUTPUT_DIR"
zip -r "libixland-ios-sdk-1.0.0.zip" "libixland-ios-sdk"

echo ""
echo "========================================"
echo "Packaging Complete!"
echo "========================================"
echo ""
echo "Output: $OUTPUT_DIR/libixland-ios-sdk-1.0.0.zip"
echo ""
echo "Contents:"
ls -lh "$DIST_DIR/"
echo ""
echo "To use in Xcode:"
echo "1. Unzip libixland-ios-sdk-1.0.0.zip"
echo "2. Drag LibIXLAND.xcframework and LibIWasm.xcframework to your project"
echo "3. Add -lpthread to Other Linker Flags"
