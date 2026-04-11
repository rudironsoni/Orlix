#!/bin/bash
#
# run-simulator-test.sh
# Build libixland test app and run on iOS Simulator
#

set -e

cd "$(dirname "$0")"

# Configuration
APP_NAME="IOSTestApp"
BUNDLE_ID="com.libixland.test"
SIM_SDK=$(xcrun --sdk iphonesimulator --show-sdk-path)
SIM_ARCH="arm64"
TARGET="$SIM_ARCH-apple-ios16.0-simulator"

echo "========================================"
echo "Building libixland Test for iOS Simulator"
echo "========================================"
echo ""
echo "SDK: $SIM_SDK"
echo "Target: $TARGET"
echo ""

# Check that libraries exist
if [ ! -f "../libixland-sim.a" ]; then
    echo "Error: libixland-sim.a not found in parent directory"
    echo "Run: cd .. && make ios-sim"
    exit 1
fi

if [ ! -f "../build/wamr-simulator/libiwasm.a" ]; then
    echo "Error: libiwasm.a not found"
    echo "Run: ./build_wamr_ios_static.sh"
    exit 1
fi

echo "Libraries found:"
echo "  - libixland-sim.a"
echo "  - libiwasm.a"
echo ""

# Build the test executable
echo "Building test executable..."

# Create output directory
mkdir -p build

# Build the test app - link both libraries separately
clang \
    -target "$TARGET" \
    -isysroot "$SIM_SDK" \
    -fobjc-arc \
    -framework UIKit \
    -framework Foundation \
    -L.. \
    -L../build/wamr-simulator \
    -lixland-sim \
    -liwasm \
    -lpthread \
    -I../include \
    -I../src/ixland/internal \
    -DIXLAND_IOS_BUILD \
    -DIXLAND_SIMULATOR_BUILD \
    -o "build/$APP_NAME" \
    "$APP_NAME/main.m" \
    2>&1

echo "✓ Build successful"
echo ""

# Create app bundle
echo "Creating app bundle..."
mkdir -p "build/$APP_NAME.app"

# Copy executable
cp "build/$APP_NAME" "build/$APP_NAME.app/"

# Create Info.plist
cat > "build/$APP_NAME.app/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>$APP_NAME</string>
    <key>CFBundleIdentifier</key>
    <string>$BUNDLE_ID</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>$APP_NAME</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>LSRequiresIPhoneOS</key>
    <true/>
    <key>UILaunchStoryboardName</key>
    <string>LaunchScreen</string>
    <key>UIRequiredDeviceCapabilities</key>
    <array>
        <string>arm64</string>
    </array>
    <key>UISupportedInterfaceOrientations</key>
    <array>
        <string>UIInterfaceOrientationPortrait</string>
    </array>
</dict>
</plist>
EOF

echo ""
echo "========================================"
echo "Build Verification"
echo "========================================"
echo ""
echo "App bundle created at: build/$APP_NAME.app"
echo ""
echo "Executable info:"
file "build/$APP_NAME.app/$APP_NAME"
echo ""

# Check architecture
lipo -info "build/$APP_NAME.app/$APP_NAME" 2>/dev/null || echo "Single architecture"

# Show ixland symbols
echo ""
echo "libixland symbols in executable:"
nm "build/$APP_NAME.app/$APP_NAME" 2>/dev/null | grep -E "T ixland_" | head -10 || echo "Symbols stripped or not found"

echo ""
echo "========================================"
echo "Next Steps"
echo "========================================"
echo ""
echo "To run on iOS Simulator:"
echo "1. Open Xcode"
echo "2. Create new iOS App project"
echo "3. Set bundle ID: $BUNDLE_ID"
echo "4. Replace main.m with: $APP_NAME/main.m"
echo "5. Add libixland-sim.a and libiwasm.a to project"
echo "6. Build and run on iPhone simulator"
echo ""
echo "Or use XcodeBuildMCP to build and run programmatically"
echo ""
