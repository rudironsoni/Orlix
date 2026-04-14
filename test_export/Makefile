CC = clang
CFLAGS = -target arm64-apple-ios16.0 -O2 -fPIC -Wall
LDFLAGS = -shared -Wl,-undefined,error

all: test_export

test_export: alias_test.c
	$(CC) $(CFLAGS) -c alias_test.c -o alias_test.o 2>&1 | tee compile.log
	$(CC) $(CFLAGS) $(LDFLAGS) alias_test.o -o libtest.dylib 2>&1 | tee -a compile.log
	nm -g libtest.dylib 2>&1 | tee symbols.log

clean:
	rm -f alias_test.o libtest.dylib compile.log symbols.log

.PHONY: all clean
