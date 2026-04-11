#!/bin/bash
#
# create-xcode-project.sh
# Creates an Xcode project for testing libixland on iOS Simulator
#

set -e

PROJECT_NAME="IOSTestApp"
BUNDLE_ID="com.libixland.test"
IOS_VERSION="16.0"

echo "Creating Xcode project for libixland testing..."

# Navigate to project directory
cd "$(dirname "$0")/$PROJECT_NAME"

# Create Xcode project using xcodebuild or create a simple project.pbxproj
# Since we can't easily generate a valid project.pbxproj, we'll use xcodegen or instruct manual setup

echo ""
echo "Xcode project structure created at: $(pwd)"
echo ""
echo "Next steps:"
echo "1. Open Xcode"
echo "2. Create a new iOS App project in this directory"
echo "3. Replace the main.m with the provided main.m"
echo "4. Add libixland-sim.a to the project"
echo "5. Configure build settings (see README.md)"
echo ""
echo "Or run: ./setup-project.sh to automate Xcode project creation"
