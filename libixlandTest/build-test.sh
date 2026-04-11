#!/bin/bash
#
# build-test.sh
# Build and run libixland tests on iOS Simulator using xcodebuild
#

set -e

PROJECT_NAME="IOSTestApp"
SCHEME_NAME="IOSTestApp"
DESTINATION="platform=iOS Simulator,name=iPhone 17 Pro"

cd "$(dirname "$0")"

echo "========================================"
echo "Building libixland iOS Test App"
echo "========================================"
echo ""

# Create a temporary Xcode project using xcodebuild
echo "Creating Xcode project..."

# Use xcodebuild to create a simple project
# First, let's check if we can use Swift Package Manager instead

# Create Package.swift for SPM-based build
cat > "$PROJECT_NAME/Package.swift" << 'EOF'
// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "IOSTestApp",
    platforms: [.iOS(.v16)],
    products: [
        .executable(name: "IOSTestApp", targets: ["IOSTestApp"])
    ],
    targets: [
        .executableTarget(
            name: "IOSTestApp",
            path: ".",
            exclude: ["Package.swift"],
            publicHeadersPath: ".",
            cSettings: [
                .headerSearchPath("../../include"),
                .headerSearchPath("../../src/ixland/internal"),
                .define("IXLAND_IOS_BUILD"),
                .define("IXLAND_SIMULATOR_BUILD")
            ],
            linkerSettings: [
                .linkedLibrary("ixland-sim", .when(platforms: [.iOS])),
                .linkedLibrary("pthread"),
                .unsafeFlags(["-L", "../.."])
            ]
        )
    ]
)
EOF

echo "Building with Swift Package Manager..."
cd "$PROJECT_NAME"

# Build for iOS Simulator
swift build --target IOSTestApp \
    -Xswiftc -sdk \
    -Xswiftc $(xcrun --sdk iphonesimulator --show-sdk-path) \
    -Xswiftc -target \
    -Xswiftc arm64-apple-ios16.0-simulator \
    2>&1 || {
        echo "SPM build failed, trying xcodebuild approach..."
        cd ..
        ./build-xcode.sh
        exit 0
    }

echo ""
echo "========================================"
echo "Build Complete"
echo "========================================"
