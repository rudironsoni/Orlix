/* IXLandSystemTests/LinuxUAPITestSupport.h
 * Linux UAPI constants for testing
 *
 * This header declares Linux UAPI constants sourced from vendored headers.
 * Used by tests to validate Linux-facing behavior.
 */

#ifndef LINUX_UAPI_TEST_SUPPORT_H
#define LINUX_UAPI_TEST_SUPPORT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Stat mode test functions - Linux UAPI sourced */
int linux_s_isdir(unsigned int mode);
int linux_s_islnk(unsigned int mode);
int linux_s_isreg(unsigned int mode);
int linux_s_ischr(unsigned int mode);
int linux_s_isblk(unsigned int mode);
int linux_s_isfifo(unsigned int mode);

/* File type constants - Linux UAPI sourced */
unsigned int linux_s_ifmt(void);
unsigned int linux_s_ifdir(void);
unsigned int linux_s_iflnk(void);
unsigned int linux_s_ifreg(void);
unsigned int linux_s_ifchr(void);

/* TTY ioctl constants - Linux UAPI sourced */
unsigned long linux_tcgets(void);
unsigned long linux_tcsets(void);
unsigned long linux_tcsetsw(void);
unsigned long linux_tcsetsf(void);
unsigned long linux_tiocsctty(void);
unsigned long linux_tiocnotty(void);   /* Correct: 0x5422 */
unsigned long linux_tiocgpgrp(void);
unsigned long linux_tiocspgrp(void);
unsigned long linux_tiocgwinsz(void);
unsigned long linux_tiocswinsz(void);
unsigned long linux_fionread(void);
unsigned long linux_tiocgptn(void);
unsigned long linux_tiocsptlck(void);

/* Signal constants - Linux UAPI sourced */
int linux_sig_block(void);
int linux_sig_setmask(void);
int linux_sigint(void);
int linux_sigquit(void);
int linux_sigtstp(void);
int linux_sigwinch(void);

#ifdef __cplusplus
}
#endif

#endif /* LINUX_UAPI_TEST_SUPPORT_H */
