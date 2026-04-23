/* IXLandSystem/kernel/time_internal.h
 * Private internal header for time subsystem struct definitions
 * 
 * This is PRIVATE internal state - NOT Linux UAPI.
 * Shared between time.c (public wrappers) and time_darwin.c (implementations).
 */

#ifndef IXLAND_KERNEL_TIME_INTERNAL_H
#define IXLAND_KERNEL_TIME_INTERNAL_H

#include "../internal/ios/fs/backing_io.h"

struct timeval;
struct timezone;
struct itimerval;

#endif /* IXLAND_KERNEL_TIME_INTERNAL_H */
