# a-shell-kernel Tests

## Overview

Unit tests for the a-shell-kernel framework using XCTest framework.

## Test Files

- `a_shell_env_tests.m` - Environment variable management tests
- `a_shell_session_tests.m` - Session management tests
- `a_shell_cmd_tests.m` - Command registration and execution tests

## Running Tests

### Option 1: Xcode GUI (Recommended)

1. Open `a-shell-kernel/a_shell_system.xcodeproj` in Xcode
2. Select the a_shell_system target
3. File > New > Target > iOS Unit Testing Bundle
4. Name it "a_shell_systemTests"
5. Add the three test .m files to the test target
6. Product > Test (Cmd+U)

### Option 2: Command Line

```bash
cd a-shell-kernel

# Create test bundle (one-time setup)
# This requires manual Xcode project editing

# Run tests
xcodebuild test \
  -project a_shell_system.xcodeproj \
  -scheme a_shell_systemTests \
  -destination 'platform=iOS Simulator,name=iPhone 17 Pro'
```

### Option 3: Makefile

```bash
make test
```

## Test Coverage

### Environment Tests
- Environment variable set/get/unset
- PATH manipulation (append, prepend, remove)
- PATH array operations
- Environment initialization

### Session Tests
- Session registration/unregistration
- Session lookup and existence
- Session acquire/release
- Session stats and validation
- Multiple session handling

### Command Tests
- Command registration/unregistration
- Command lookup
- Command info retrieval
- Command listing
- Command execution (built-ins)

## API Documentation

Tests use the public API from `a_shell_system/a_shell_system.h`:

### Environment
- `ashell_env_initialize()`
- `ios_setenv(name, value, overwrite)`
- `ios_getenv(name)`
- `ios_unsetenv(name)`
- `ashell_env_path_append(dir)`
- `ashell_env_path_prepend(dir)`
- `ashell_env_path_remove(dir)`

### Session
- `ashell_session_register(id, params)`
- `ashell_session_unregister(id)`
- `ashell_session_get(id)`
- `ashell_session_exists(id)`
- `ashell_session_acquire(id)`
- `ashell_session_release(id)`
- `ashell_session_stats(&stats)`

### Commands
- `ashell_register_command(name, type, ...)`
- `ashell_unregister_command(name)`
- `ashell_command_exists(name)`
- `ashell_get_command_info(name, &info)`
- `ashell_list_commands(names, max)`
- `ashell_execute_command(name, argc, argv)`

## Notes

- Tests require XCTest framework (iOS testing bundle)
- Tests link against the built a_shell_system.framework
- Some tests use built-in commands (pwd, echo) for integration testing
