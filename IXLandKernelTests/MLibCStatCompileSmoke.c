/*
 * MLibCStatCompileSmoke.c
 *
 * PACKAGE-FACING STAT HEADER COMPILE TEST
 *
 * This file proves that Linux-oriented stat-facing declarations resolve
 * through the IXLandMLibC bootstrap surface instead of through Darwin SDK
 * headers. It is a pure C compile-smoke test.
 */

#include <sys/stat.h>

#ifndef STATX_TYPE
#error "STATX_TYPE must resolve through the package-facing sys/stat.h owner"
#endif

#ifndef S_IFMT
#error "S_IFMT must resolve through the package-facing sys/stat.h owner"
#endif

static unsigned int mlibc_stat_compile_smoke_mask(unsigned int mask) {
    return mask & (STATX_TYPE | STATX_MODE | STATX_SIZE);
}

static unsigned short mlibc_stat_compile_smoke_mode(struct statx *stx) {
    return stx->stx_mode & S_IFMT;
}

__attribute__((unused)) static void (*volatile mlibc_stat_smoke_refs[])(void) = {
    (void (*)(void))mlibc_stat_compile_smoke_mask,
    (void (*)(void))mlibc_stat_compile_smoke_mode,
};
