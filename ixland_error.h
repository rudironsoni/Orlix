//
//  error.h
//  shell_cmds_ios
//
//  Created by Nicolas Holzschuch on 16/06/2017.
//  Copyright © 2017 Nicolas Holzschuch. All rights reserved.
//

#ifndef ios_error_h
#define ios_error_h

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/signal.h>

/* #define errx compileError
#define err compileError
#define warn compileError
#define warnx compileError
#ifndef printf
#define printf(...) fprintf (thread_stdout, ##__VA_ARGS__)
#endif */

#define putchar(a) fputc(a, thread_stdout)
#define getchar() fgetc(thread_stdin)
// these functions are defined differently in C++. The #define approach breaks things.
#ifndef __cplusplus
  #define getwchar() fgetwc(thread_stdin)
  #define putwchar(a) fputwc(a, thread_stdout)
  // iswprint depends on the given locale, and setlocale() fails on iOS:
  #define iswprint(a) 1
  #define write ixland_write
  #define fwrite ixland_fwrite
  #define puts ixland_puts
  #define fputs ixland_fputs
  #define fputc ixland_fputc
  #define putw ixland_putw
  #define putp ixland_putp
  #define fflush ixland_fflush
  #define abort() ixland_exit(1)
#endif

// Thread-local input and output streams
extern __thread FILE* thread_stdin;
extern __thread FILE* thread_stdout;
extern __thread FILE* thread_stderr;

#define exit ixland_exit
#define _exit ixland_exit
#define kill ixland_killpid
#define _kill ixland_killpid
#define killpg ixland_killpid
#define popen ixland_popen
#define pclose fclose
#define system ixland_system
#define execv ixland_execv
#define execvp ixland_execv
#define execve ixland_execve
#define dup2 ixland_dup2
#define getenv ixland_getenv
#define setenv ixland_setenv
#define unsetenv ixland_unsetenv
#define putenv ixland_putenv
#define fchdir ixland_fchdir
#define signal ixland_signal

extern int ixland_executable(const char* cmd); // is this command part of the "shell" commands?
extern int ixland_system(const char* inputCmd); // execute this command (executable file or builtin command)
extern FILE *ixland_popen(const char *command, const char *type); // Execute this command and pipe the result
extern int ixland_kill(void); // kill the current running command
extern int ixland_killpid(pid_t pid, int sig); // kill the current running command

extern void ixland_exit(int errorCode) __dead2; // set error code and exits from the thread.
extern int ixland_execv(const char *path, char* const argv[]);
extern int ixland_execve(const char *path, char* const argv[], char** envlist);
extern int ixland_dup2(int fd1, int fd2);
extern char * ixland_getenv(const char *name);
extern int ixland_setenv(const char* variableName, const char* value, int overwrite);
int ixland_unsetenv(const char* variableName);
extern int ixland_putenv(char *string);
extern char** environmentVariables(pid_t pid);

extern int ixland_isatty(int fd);
extern pthread_t ixland_getLastThreadId(void);  // deprecated
extern pthread_t ixland_getThreadId(pid_t pid);
extern void ixland_storeThreadId(pthread_t thread);
extern void ixland_releaseThread(pthread_t thread);
extern void ixland_releaseThreadId(pid_t pid);
extern pid_t ixland_currentPid(void);
extern int ixland_getCommandStatus(void);
extern const char* ixland_progname(void);
extern pid_t ixland_fork(void);
extern void ixland_waitpid(pid_t pid);
extern pid_t ixland_full_waitpid(pid_t pid, int *stat_loc, int options);

// Catch signal definition:
extern int canSetSignal(void);
extern sig_t ixland_signal(int signal, sig_t function);


extern int ixland_fchdir(const int fd);
extern ssize_t ixland_write(int fildes, const void *buf, size_t nbyte);
extern size_t ixland_fwrite(const void *ptr, size_t size, size_t nitems, FILE *stream);
extern int ixland_puts(const char *s);
extern int ixland_fputs(const char* s, FILE *stream);
extern int ixland_fputc(int c, FILE *stream);
extern int ixland_putw(int w, FILE *stream);
extern int ixland_fflush(FILE *stream);
extern int ixland_getstdin(void);
extern int ixland_gettty(void);
extern int ixland_opentty(void);
extern void ixland_closetty(void);
extern void ixland_stopInteractive(void);
extern void ixland_startInteractive(void);
extern int ixland_storeInteractive(void);
// Communication between dash and ixland_system:
extern const char* ixland_expandtilde(const char *login);
extern void ixland_activateChildStreams(FILE** old_stdin, FILE** old_stdout,  FILE ** old_stderr);
extern const char* ixland_getBookmarkedVersion(const char* p);

#ifdef __cplusplus
}
#endif
#endif /* ios_error_h */
