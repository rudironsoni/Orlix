//
//  main.m
//  libixlandTest - iOS Test Suite
//

#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>

// Forward declarations for comprehensive tests
@interface ComprehensiveTestRunner : NSObject
- (void)runAllTests;
@property (nonatomic, strong, readonly) NSMutableArray *results;
@property (nonatomic, assign, readonly) int passedCount;
@property (nonatomic, assign, readonly) int failedCount;
@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@property (strong, nonatomic) UILabel *statusLabel;
@property (strong, nonatomic) UITextView *resultsTextView;
- (void)updateStatus:(NSString *)message;
- (void)showFinalResults:(int)passed failed:(int)failed total:(int)total;
@end

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Create UI first
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    UIViewController *vc = [[UIViewController alloc] init];
    vc.view.backgroundColor = [UIColor systemBackgroundColor];
    
    // Status label at top
    self.statusLabel = [[UILabel alloc] initWithFrame:CGRectMake(20, 60, 350, 40)];
    self.statusLabel.text = @"Running libixland Tests...";
    self.statusLabel.font = [UIFont boldSystemFontOfSize:18];
    self.statusLabel.textAlignment = NSTextAlignmentCenter;
    [vc.view addSubview:self.statusLabel];
    
    // Results text view
    self.resultsTextView = [[UITextView alloc] initWithFrame:CGRectMake(20, 110, 350, 500)];
    self.resultsTextView.font = [UIFont fontWithName:@"Menlo" size:11];
    self.resultsTextView.editable = NO;
    self.resultsTextView.backgroundColor = [UIColor secondarySystemBackgroundColor];
    [vc.view addSubview:self.resultsTextView];
    
    self.window.rootViewController = vc;
    [self.window makeKeyAndVisible];
    
    // Run tests after a short delay to let UI render
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        [self runComprehensiveTests];
    });
    
    return YES;
}

- (void)runComprehensiveTests {
    NSLog(@"========================================");
    NSLog(@"Starting Comprehensive Test Suite");
    NSLog(@"========================================");
    
    ComprehensiveTestRunner *runner = [[ComprehensiveTestRunner alloc] init];
    [runner runAllTests];
    
    int passed = runner.passedCount;
    int failed = runner.failedCount;
    int total = (int)runner.results.count;
    
    [self showFinalResults:passed failed:failed total:total];
}

- (void)updateStatus:(NSString *)message {
    dispatch_async(dispatch_get_main_queue(), ^{
        self.statusLabel.text = message;
        NSLog(@"[Test Status] %@", message);
    });
}

- (void)showFinalResults:(int)passed failed:(int)failed total:(int)total {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString *summary = [NSString stringWithFormat:@"Tests: %d/%d passed", passed, total];
        
        if (failed == 0) {
            self.statusLabel.text = @"✓ ALL TESTS PASSED";
            self.statusLabel.textColor = [UIColor systemGreenColor];
        } else {
            self.statusLabel.text = [NSString stringWithFormat:@"✗ %d TEST(S) FAILED", failed];
            self.statusLabel.textColor = [UIColor systemRedColor];
        }
        
        NSMutableString *results = [NSMutableString string];
        [results appendString:@"libixland Comprehensive Test Results\n"];
        [results appendString:@"=====================================\n\n"];
        [results appendFormat:@"Total: %d\n", total];
        [results appendFormat:@"Passed: %d ✓\n", passed];
        [results appendFormat:@"Failed: %d ✗\n\n", failed];
        
        if (failed > 0) {
            [results appendString:@"Failed Tests:\n"];
            // Would iterate through results here if we had access
            [results appendString:@"(See console log for details)\n"];
        } else {
            [results appendString:@"All syscall categories tested:\n"];
            [results appendString:@"• Process Management (16)\n"];
            [results appendString:@"• File Operations (20)\n"];
            [results appendString:@"• Filesystem (13)\n"];
            [results appendString:@"• Environment (5)\n"];
            [results appendString:@"• Signal Handling (5)\n"];
            [results appendString:@"• Memory Management (3)\n"];
            [results appendString:@"• Time (4)\n"];
            [results appendString:@"• TTY (3)\n"];
            [results appendString:@"• Network (20+)\n"];
        }
        
        self.resultsTextView.text = results;
        
        NSLog(@"\n========================================");
        NSLog(@"%@", summary);
        NSLog(@"========================================");
    });
}

@end

int main(int argc, char * argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
