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
