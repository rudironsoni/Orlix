#!/bin/bash
# Fix Frameworks build phase to include Network.framework

PROJECT_FILE="libixlandTest/libixlandTest/libixlandTest.xcodeproj/project.pbxproj"

# Add Network.framework reference
NETWORK_REF="A00100001D4B5C0000F1A2C3"
NETWORK_FILE="A00100001D4B5C0000F1A2C4"

# Insert Network.framework into PBXFileReference section
sed -i '' "/^\/\* Begin PBXFileReference section \*\/$/a\
\t\t${NETWORK_FILE} \/* Network.framework *\/ = {isa = PBXFileReference; lastKnownFileType = wrapper.framework; name = Network.framework; path = System\/Library\/Frameworks\/Network.framework; sourceTree = SDKROOT; };" \
    "$PROJECT_FILE"

# Insert Network.framework into Frameworks group
sed -i '' "/A00100001D4B5C0000F1A2B8 \/\* include \*\/,$/a\
\t\t\t\t${NETWORK_FILE} \/* Network.framework *\/," \
    "$PROJECT_FILE"

# Add to PBXFrameworksBuildPhase
sed -i '' "/libiwasm.a in Frameworks.*,/{n;s/,/,\n\t\t\t\t${NETWORK_REF} \/* Network.framework in Frameworks *\/,/}" \
    "$PROJECT_FILE"

echo "Added Network.framework to project"
