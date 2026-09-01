#include <stdint.h>

extern int ghostty_init(uintptr_t argc, char **argv);
extern const char *libssh2_version(int required_version);
extern const char *OpenSSL_version(int type);

__attribute__((used))
static const uintptr_t orlix_native_link_symbols[] = {
    (uintptr_t)&ghostty_init,
    (uintptr_t)&libssh2_version,
    (uintptr_t)&OpenSSL_version,
};

int orlix_native_link_smoke(void) {
    return (int)(orlix_native_link_symbols[0] ^
                 orlix_native_link_symbols[1] ^
                 orlix_native_link_symbols[2]);
}
