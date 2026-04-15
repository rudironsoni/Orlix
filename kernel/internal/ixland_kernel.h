/*
 * kernel.h - IXLand Kernel Internal Umbrella Header
 *
 * Includes the internal kernel headers needed by kernel
 * implementation code. NOT a public API header.
 *
 * For public APIs, use <ixland/ixland.h>.
 */

#ifndef IXLAND_KERNEL_H
#define IXLAND_KERNEL_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * System headers (for types, constants, structs)
 * ============================================================================ */

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

/* ============================================================================
 * Internal kernel headers (NOT host signal.h)
 * ============================================================================ */

#include "../../fs/fdtable.h"
#include "../../fs/vfs.h"
#include "../../include/ixland/ixland_types.h"
#include "../signal.h"

/* ============================================================================
 * Version Information
 * ============================================================================ */

#define IXLAND_KERNEL_VERSION_MAJOR 1
#define IXLAND_KERNEL_VERSION_MINOR 0
#define IXLAND_KERNEL_VERSION_PATCH 0

#define IXLAND_KERNEL_VERSION "1.0.0"

/* ============================================================================
 * Kernel Initialization
 * ============================================================================ */

extern int kernel_init(const char *prefix);
extern const char *kernel_get_prefix(void);
extern int kernel_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* IXLAND_KERNEL_H */
