#!/bin/bash
#
# setup-xcode-project.sh
# Automated Xcode project setup for libixland testing
#

set -e

PROJECT_NAME="IOSTestApp"
BUNDLE_ID="com.libixland.test"
IOS_VERSION="16.0"

echo "Setting up Xcode project for libixland testing..."

# Navigate to project directory
cd "$(dirname "$0")"

# Create directories
mkdir -p "$PROJECT_NAME"

# Create Info.plist
cat > "$PROJECT_NAME/Info.plist" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>$(EXECUTABLE_NAME)</string>
    <key>CFBundleIdentifier</key>
    <string>com.libixland.test</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>$(PRODUCT_NAME)</string>
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
        <string>armv7</string>
    </array>
    <key>UISupportedInterfaceOrientations</key>
    <array>
        <string>UIInterfaceOrientationPortrait</string>
    </array>
    <key>MinimumOSVersion</key>
    <string>16.0</string>
</dict>
</plist>
EOF

# Create main.m
cat > "$PROJECT_NAME/main.m" << 'EOF'
//
// main.m - libixland Test App
//

#import <UIKit/UIKit.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// ixland public API
extern int ixland_init(void);
extern int ixland_is_initialized(void);
extern const char *ixland_version(void);
extern pid_t ixland_getpid(void);
extern pid_t ixland_getppid(void);
extern char *ixland_getcwd(char *buf, size_t size);
extern int ixland_open(const char *pathname, int flags, mode_t mode);
extern ssize_t ixland_read(int fd, void *buf, size_t count);
extern ssize_t ixland_write(int fd, const void *buf, size_t count);
extern int ixland_close(int fd);

// Simple test runner
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    NSLog(@"Running test: %s", #name); \
    test_##name(); \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (cond) { \
        tests_passed++; \
        NSLog(@"✓ %s:%d - %s", __FILE__, __LINE__, #cond); \
    } else { \
        tests_failed++; \
        NSLog(@"✗ %s:%d - Assertion failed: %s", __FILE__, __LINE__, #cond); \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))
#define ASSERT_GT(a, b) ASSERT_TRUE((a) > (b))

TEST(init) {
    ASSERT_TRUE(ixland_is_initialized());
    const char *ver = ixland_version();
    ASSERT_NE(ver, NULL);
    ASSERT_GT(strlen(ver), 0);
    NSLog(@"libixland version: %s", ver);
}

TEST(getpid) {
    pid_t pid = ixland_getpid();
    ASSERT_GT(pid, 0);
    NSLog(@"PID: %d", pid);
}

TEST(getppid) {
    pid_t ppid = ixland_getppid();
    ASSERT_GT(ppid, 0);
    NSLog(@"PPID: %d", ppid);
}

TEST(getcwd) {
    char cwd[1024];
    char *result = ixland_getcwd(cwd, sizeof(cwd));
    ASSERT_NE(result, NULL);
    ASSERT_GT(strlen(cwd), 0);
    NSLog(@"CWD: %s", cwd);
}

TEST(file_io) {
    // Create a test file
    const char *test_path = "/tmp/ixland_test_file.txt";
    const char *test_data = "Hello from libixland!";
    
    // Write
    int fd = ixland_open(test_path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    ASSERT_GT(fd, 0);
    
    ssize_t written = ixland_write(fd, test_data, strlen(test_data));
    ASSERT_EQ(written, strlen(test_data));
    
    ixland_close(fd);
    
    // Read back
    fd = ixland_open(test_path, O_RDONLY, 0);
    ASSERT_GT(fd, 0);
    
    char buf[256];
    ssize_t read_bytes = ixland_read(fd, buf, sizeof(buf));
    ASSERT_EQ(read_bytes, strlen(test_data));
    buf[read_bytes] = '\0';
    ASSERT_TRUE(strcmp(buf, test_data) == 0);
    
    ixland_close(fd);
    unlink(test_path);
    
    NSLog(@"File I/O test passed");
}

// App delegate
@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Run tests
    NSLog(@"========================================");
    NSLog(@"libixland iOS Test Suite");
    NSLog(@"========================================");
    
    RUN_TEST(init);
    RUN_TEST(getpid);
    RUN_TEST(getppid);
    RUN_TEST(getcwd);
    RUN_TEST(file_io);
    
    NSLog(@"========================================");
    NSLog(@"Results: %d passed, %d failed", tests_passed, tests_failed);
    if (tests_failed == 0) {
        NSLog(@"ALL TESTS PASSED ✓");
    } else {
        NSLog(@"SOME TESTS FAILED ✗");
    }
    NSLog(@"========================================");
    
    // Create a simple UI to show results
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    UIViewController *rootViewController = [[UIViewController alloc] init];
    rootViewController.view.backgroundColor = [UIColor whiteColor];
    
    UILabel *label = [[UILabel alloc] initWithFrame:CGRectMake(20, 100, 300, 200)];
    label.numberOfLines = 0;
    label.textAlignment = NSTextAlignmentCenter;
    
    if (tests_failed == 0) {
        label.text = [NSString stringWithFormat:@"libixland Tests\n\n✓ All %d tests passed!", tests_passed];
        label.textColor = [UIColor greenColor];
    } else {
        label.text = [NSString stringWithFormat:@"libixland Tests\n\n✗ %d failed, %d passed", tests_failed, tests_passed];
        label.textColor = [UIColor redColor];
    }
    
    [rootViewController.view addSubview:label];
    self.window.rootViewController = rootViewController;
    [self.window makeKeyAndVisible];
    
    return YES;
}

@end

int main(int argc, char *argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
EOF

# Create LaunchScreen.storyboard (minimal)
cat > "$PROJECT_NAME/LaunchScreen.storyboard" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<document type="com.apple.InterfaceBuilder3.CocoaTouch.Storyboard.XIB" version="3.0" toolsVersion="21701" targetRuntime="iOS.CocoaTouch" propertyAccessControl="none" useAutolayout="YES" launchScreen="YES" useTraitCollections="YES" useSafeAreas="YES" colorMatched="YES" initialViewController="01J-lp-oVM">
    <device id="retina6_12" orientation="portrait" appearance="light"/>
    <dependencies>
        <deployment identifier="iOS"/>
        <plugIn identifier="com.apple.InterfaceBuilder.IBCocoaTouchPlugin" version="21679"/>
        <capability name="Safe area layout guides" minToolsVersion="9.0"/>
        <capability name="documents saved in the Xcode 8 format" minToolsVersion="8.0"/>
    </dependencies>
    <scenes>
        <scene sceneID="EHf-IW-A2E">
            <objects>
                <viewController id="01J-lp-oVM" sceneMemberID="viewController">
                    <view key="view" contentMode="scaleToFill" id="Ze5-6b-2t3">
                        <rect key="frame" x="0.0" y="0.0" width="393" height="852"/>
                        <autoresizingMask key="autoresizingMask" widthSizable="YES" heightSizable="YES"/>
                        <subviews>
                            <label opaque="NO" clipsSubviews="YES" userInteractionEnabled="NO" contentMode="left" horizontalHuggingPriority="251" verticalHuggingPriority="251" text="libixland Test" textAlignment="center" lineBreakMode="middleTruncation" baselineAdjustment="alignBaselines" minimumFontSize="18" translatesAutoresizingMaskIntoConstraints="NO" id="GJd-Yh-RWb">
                                <rect key="frame" x="0.0" y="263.66666666666669" width="393" height="43"/>
                                <fontDescription key="fontDescription" type="boldSystem" pointSize="36"/>
                                <color key="textColor" red="0.0" green="0.0" blue="0.0" alpha="1" colorSpace="custom" customColorSpace="sRGB"/>
                                <nil key="highlightedColor"/>
                            </label>
                        </subviews>
                        <color key="backgroundColor" red="1" green="1" blue="1" alpha="1" colorSpace="custom" customColorSpace="sRGB"/>
                        <constraints>
                            <constraint firstItem="GJd-Yh-RWb" firstAttribute="centerY" secondItem="Ze5-6b-2t3" secondAttribute="centerY" id="gaa-gc-qlf"/>
                            <constraint firstAttribute="trailing" secondItem="GJd-Yh-RWb" secondAttribute="trailing" id="h8H-Jk-nxq"/>
                            <constraint firstItem="GJd-Yh-RWb" firstAttribute="leading" secondItem="Ze5-6b-2t3" secondAttribute="leading" id="p71-Hg-PDj"/>
                        </constraints>
                        <viewLayoutGuide key="safeArea" id="Bcu-3y-fUS"/>
                    </view>
                </viewController>
                <placeholder placeholderIdentifier="IBFirstResponder" id="iYj-Kq-Ea1" userLabel="First Responder" sceneMemberID="firstResponder"/>
            </objects>
            <point key="canvasLocation" x="53" y="375"/>
        </scene>
    </scenes>
</document>
EOF

echo ""
echo "========================================"
echo "Xcode project files created"
echo "========================================"
echo ""
echo "Files created in: $(pwd)/$PROJECT_NAME/"
echo ""
ls -la "$PROJECT_NAME/"
echo ""
echo "Next steps:"
echo "1. Open Xcode"
echo "2. Create new iOS App project: File > New > Project > iOS App"
echo "3. Set project name: IOSTestApp"
echo "4. Set bundle ID: com.libixland.test"
echo "5. Choose location: $(pwd)/$PROJECT_NAME"
echo "6. Replace main.m with the generated main.m"
echo "7. Add libixland-sim.a to project (drag from parent directory)"
echo "8. Set deployment target to iOS 16.0"
echo "9. Build and run on iOS Simulator"
echo ""
echo "Or use XcodeBuildMCP to build programmatically"
