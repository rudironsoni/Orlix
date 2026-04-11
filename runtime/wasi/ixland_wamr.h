/*
 * ixland_wamr.h - WAMR Integration Internal API
 *
 * WebAssembly Micro Runtime integration for libixland.
 * This is the internal implementation header; public contract is in ixland-wasm.
 */

#ifndef IXLAND_WAMR_H
#define IXLAND_WAMR_H

#include <stdbool.h>
#include <stdint.h>

/* Include public wasm contract types */
#include <ixland/wasm/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

/**
 * Initialize WAMR runtime
 * @return 0 on success, -1 on failure
 */
int ixland_wamr_init(void);

/**
 * Deinitialize WAMR runtime and cleanup resources
 */
void ixland_wamr_deinit(void);

/**
 * Check if WAMR runtime is initialized
 * @return true if initialized
 */
bool ixland_wamr_is_initialized(void);

/* ============================================================================
 * MODULE OPERATIONS
 * ============================================================================ */

/**
 * Load a WASM module from buffer
 * @param wasm_buf Pointer to WASM binary data
 * @param wasm_size Size of WASM data in bytes
 * @return 0 on success, -1 on failure
 */
int ixland_wamr_load_module(const uint8_t *wasm_buf, uint32_t wasm_size);

/**
 * Unload the current WASM module
 */
void ixland_wamr_unload_module(void);

/**
 * Validate WASM buffer (check magic number)
 * @param wasm_buf Pointer to WASM data
 * @param wasm_size Size of WASM data
 * @return true if valid WASM
 */
bool ixland_wamr_validate_wasm(const uint8_t *wasm_buf, uint32_t wasm_size);

/* ============================================================================
 * FUNCTION EXECUTION
 * ============================================================================ */

/**
 * Call a WASM function by name
 * @param func_name Name of the function to call
 * @param argc Number of arguments
 * @param argv Array of 32-bit arguments (also receives return values)
 * @return 0 on success, -1 on failure
 */
int ixland_wamr_call_function(const char *func_name, uint32_t argc, uint32_t argv[]);

/**
 * Lookup a function in the current module
 * @param func_name Function name
 * @return true if function exists
 */
bool ixland_wamr_function_exists(const char *func_name);

/* ============================================================================
 * MEMORY ACCESS
 * ============================================================================ */

/**
 * Get the total memory size of the current module
 * @return Memory size in bytes, 0 if no module loaded
 */
uint32_t ixland_wamr_get_memory_size(void);

/**
 * Read memory from WASM instance
 * @param offset Memory offset (WASM address)
 * @param buf Buffer to read into
 * @param len Number of bytes to read
 * @return 0 on success, -1 on failure
 */
int ixland_wamr_memory_read(uint32_t offset, void *buf, uint32_t len);

/**
 * Write memory to WASM instance
 * @param offset Memory offset (WASM address)
 * @param buf Buffer to write from
 * @param len Number of bytes to write
 * @return 0 on success, -1 on failure
 */
int ixland_wamr_memory_write(uint32_t offset, const void *buf, uint32_t len);

/* ============================================================================
 * EXPORT INTROSPECTION
 * ============================================================================ */

/**
 * Get list of exported function names
 * @param names Output: array of function name strings (must be freed by caller)
 * @param count Output: number of functions
 * @return 0 on success, -1 on failure
 */
int ixland_wamr_get_export_list(char ***names, uint32_t *count);

/**
 * Free the export list returned by ixland_wamr_get_export_list
 * @param names Array of names
 * @param count Number of names
 */
void ixland_wamr_free_export_list(char **names, uint32_t count);

/* ============================================================================
 * ERROR HANDLING
 * ============================================================================ */

/**
 * Get the last error message
 * @return Error string (valid until next WAMR call)
 */
const char *ixland_wamr_get_error(void);

/**
 * Get the backend kind for this WAMR runtime
 * @return Backend kind (always IXLAND_WASM_BACKEND_WAMR for this implementation)
 */
ixland_wasm_backend_kind_t ixland_wamr_get_backend_kind(void);

#ifdef __cplusplus
}
#endif

#endif /* IXLAND_WAMR_H */
