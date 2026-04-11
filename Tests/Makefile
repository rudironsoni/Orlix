# Test Makefile for a-shell-kernel

CC = clang
CFLAGS = -I. -I../include -Wall -Werror
LDFLAGS = -framework Foundation

TESTS = test_vfs test_process test_file

.PHONY: all clean test

all: $(TESTS)

test_vfs: test_vfs.c ../src/vfs.c
	$(CC) $(CFLAGS) -o $@ $^

test_process: test_process.c
	$(CC) $(CFLAGS) -DA_SHELL_NO_COMPAT_MACROS -o $@ $< $(LDFLAGS)

test_file: test_file.c ../src/vfs.c ../src/syscalls/file.c
	$(CC) $(CFLAGS) -DA_SHELL_NO_COMPAT_MACROS -o $@ $^

test: $(TESTS)
	@echo "Running tests..."
	@for test in $(TESTS); do \
		echo "\n========================================"; \
		echo "Running $$test"; \
		echo "========================================"; \
		./$$test || exit 1; \
	done
	@echo "\n========================================"
	@echo "All tests passed!"
	@echo "========================================"

clean:
	rm -f $(TESTS)
