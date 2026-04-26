/* IXLandSystemTests/LinuxUAPITestSupport.c
 * Linux UAPI constants for testing
 *
 * This file sources Linux UAPI constants from vendored headers
 * and exposes them for test use. Compiled as part of test target.
 */

/* Include vendored Linux UAPI headers */
#include <linux/stat.h>
#include <asm-generic/ioctls.h>
#include <asm-generic/signal.h>
#include <asm-generic/termbits.h>

/* Stat mode test functions - Linux UAPI sourced */
int linux_s_isdir(unsigned int mode) { return S_ISDIR(mode); }
int linux_s_islnk(unsigned int mode) { return S_ISLNK(mode); }
int linux_s_isreg(unsigned int mode) { return S_ISREG(mode); }
int linux_s_ischr(unsigned int mode) { return S_ISCHR(mode); }
int linux_s_isblk(unsigned int mode) { return S_ISBLK(mode); }
int linux_s_isfifo(unsigned int mode) { return S_ISFIFO(mode); }

/* File type constants - Linux UAPI sourced */
unsigned int linux_s_ifmt(void) { return S_IFMT; }
unsigned int linux_s_ifdir(void) { return S_IFDIR; }
unsigned int linux_s_iflnk(void) { return S_IFLNK; }
unsigned int linux_s_ifreg(void) { return S_IFREG; }
unsigned int linux_s_ifchr(void) { return S_IFCHR; }

/* TTY ioctl constants - Linux UAPI sourced */
unsigned long linux_tcgets(void) { return TCGETS; }
unsigned long linux_tcsets(void) { return TCSETS; }
unsigned long linux_tcsetsw(void) { return TCSETSW; }
unsigned long linux_tcsetsf(void) { return TCSETSF; }
unsigned long linux_tiocsctty(void) { return TIOCSCTTY; }
unsigned long linux_tiocnotty(void) { return TIOCNOTTY; }  /* Correct: 0x5422 */
unsigned long linux_tiocgpgrp(void) { return TIOCGPGRP; }
unsigned long linux_tiocspgrp(void) { return TIOCSPGRP; }
unsigned long linux_tiocgwinsz(void) { return TIOCGWINSZ; }
unsigned long linux_tiocswinsz(void) { return TIOCSWINSZ; }
unsigned long linux_fionread(void) { return FIONREAD; }
unsigned long linux_tiocgptn(void) { return TIOCGPTN; }
unsigned long linux_tiocsptlck(void) { return TIOCSPTLCK; }

/* Signal constants - Linux UAPI sourced */
int linux_sig_block(void) { return SIG_BLOCK; }
int linux_sig_setmask(void) { return SIG_SETMASK; }
int linux_sigint(void) { return SIGINT; }
int linux_sigquit(void) { return SIGQUIT; }
int linux_sigtstp(void) { return SIGTSTP; }
int linux_sigwinch(void) { return SIGWINCH; }

/* Termios lflag constants - Linux UAPI sourced */
unsigned int linux_lflag_isig(void) { return ISIG; }
unsigned int linux_lflag_icanon(void) { return ICANON; }
unsigned int linux_lflag_echo(void) { return ECHO; }
unsigned int linux_lflag_tostop(void) { return TOSTOP; }

/* Termios c_cc indices - Linux UAPI sourced */
int linux_cc_vintr(void) { return VINTR; }
int linux_cc_vquit(void) { return VQUIT; }
int linux_cc_verase(void) { return VERASE; }
int linux_cc_vkill(void) { return VKILL; }
int linux_cc_veof(void) { return VEOF; }
int linux_cc_vtime(void) { return VTIME; }
int linux_cc_vmin(void) { return VMIN; }
int linux_cc_vsusp(void) { return VSUSP; }
