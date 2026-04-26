/* IXLandSystemTests/LinuxUAPITestSupport.c
 * Semantic test helpers for Linux UAPI-sensitive assertions
 *
 * This file implements semantic test helpers that internally use vendored
 * Linux UAPI headers with canonical names (S_ISDIR, TIOCNOTTY, SIGINT, etc.).
 *
 * The Objective-C tests call behavior-level helpers, not renamed constants.
 *
 * NOTE: This file includes ONLY Linux UAPI headers, NOT Darwin headers.
 */

/* Include vendored Linux UAPI headers - canonical names used internally */
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <asm-generic/ioctls.h>
#include <asm-generic/signal.h>
#include <asm-generic/siginfo.h>
#include <asm-generic/signal-defs.h>
#include <asm-generic/termbits.h>

/* ============================================================================
 * Stat mode semantic test helpers
 * ============================================================================ */

int ixland_test_uapi_mode_is_directory(unsigned int mode) {
    return S_ISDIR(mode);
}

int ixland_test_uapi_mode_is_symlink(unsigned int mode) {
    return S_ISLNK(mode);
}

int ixland_test_uapi_mode_is_regular(unsigned int mode) {
    return S_ISREG(mode);
}

int ixland_test_uapi_mode_is_char_device(unsigned int mode) {
    return S_ISCHR(mode);
}

int ixland_test_uapi_mode_is_block_device(unsigned int mode) {
    return S_ISBLK(mode);
}

int ixland_test_uapi_mode_is_fifo(unsigned int mode) {
    return S_ISFIFO(mode);
}

/* ============================================================================
 * Termios semantic test helpers
 * ============================================================================ */

int ixland_test_termios_has_isig(unsigned int lflag) {
    return (lflag & ISIG) != 0;
}

int ixland_test_termios_has_icanon(unsigned int lflag) {
    return (lflag & ICANON) != 0;
}

int ixland_test_termios_has_echo(unsigned int lflag) {
    return (lflag & ECHO) != 0;
}

int ixland_test_termios_has_tostop(unsigned int lflag) {
    return (lflag & TOSTOP) != 0;
}

int ixland_test_termios_cc_vmin_index(void) {
    return VMIN;
}

int ixland_test_termios_cc_vtime_index(void) {
    return VTIME;
}

/* ============================================================================
 * AT_* flag helpers for namei operations
 * ============================================================================ */

int ixland_test_at_symlink_nofollow(void) {
    return AT_SYMLINK_NOFOLLOW;
}

int ixland_test_at_eaccess(void) {
    return AT_EACCESS;
}

int ixland_test_at_removedir(void) {
    return AT_REMOVEDIR;
}

/* ============================================================================
 * renameat2 flag helpers
 * These are hardcoded because linux/fs.h conflicts with Darwin headers
 * ============================================================================ */

int ixland_test_rename_noreplace(void) {
    return 1;  /* RENAME_NOREPLACE = (1 << 0) */
}

int ixland_test_rename_exchange(void) {
    return 2;  /* RENAME_EXCHANGE = (1 << 1) */
}

int ixland_test_rename_whiteout(void) {
    return 4;  /* RENAME_WHITEOUT = (1 << 2) */
}
