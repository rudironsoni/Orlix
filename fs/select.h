#ifndef SELECT_H
#define SELECT_H

/*
 * IXLandSystem poll/select internal header
 * 
 * Uses standard POSIX/Linux types for internal coherence.
 * Darwin kqueue implementation details stay in .c file.
 */

#include <stddef.h>
#include <stdint.h>
#include <sys/select.h>

/* Forward declarations for internal readiness helpers */
struct timespec;

#endif /* SELECT_H */
