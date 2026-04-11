/*
 * ixland_kernel.h - iXland Kernel Internal Umbrella Header
 *
 * This header includes the internal kernel headers needed by kernel
 * implementation code. It is NOT a public API header - use <ixland/ixland.h>
 * for public APIs.
 *
 * This header provides:
 * - Kernel version information
 * - VFS layer headers
 * - Internal type definitions
 * - Kernel initialization functions
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
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

/* ============================================================================
 * Public ixland types (required for kernel implementation)
 * ============================================================================ */

#include "ixland/ixland_types.h"

/* ============================================================================
 * VFS Layer
 * ============================================================================ */

#include "fdtable.h"
#include "vfs.h"

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

/**
 * @brief Initialize the iXland kernel
 *
 * @param prefix Path prefix for VFS path translation
 * @return int 0 on success, -1 on error
 */
extern int ixland_kernel_init(const char *prefix);

/**
 * @brief Get the kernel prefix path
 *
 * @return const char* The prefix path, or NULL if not initialized
 */
extern const char *ixland_kernel_get_prefix(void);

/**
 * @brief Check if the kernel is initialized
 *
 * @return int 1 if initialized, 0 otherwise
 */
extern int ixland_kernel_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif /* IXLAND_KERNEL_H */
