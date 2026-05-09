/* OrlixKernel/kernel/time_state.h
 * Private internal header for time subsystem struct definitions
 * 
 * This is PRIVATE internal state - NOT Linux UAPI.
 * Shared between time.c (public wrappers) and time_darwin.c (implementations).
 */

#ifndef KERNEL_TIME_INTERNAL_H
#define KERNEL_TIME_INTERNAL_H

#include <stddef.h>

#include <linux/time_types.h>

int kernel_clock_gettime(int clock_id, struct __kernel_timespec *tp);

#endif /* KERNEL_TIME_INTERNAL_H */
