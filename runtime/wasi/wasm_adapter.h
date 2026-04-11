/*
 * wasm_adapter.h - Internal Wasm Contract Adapter
 *
 * Maps current WAMR integration onto neutral ixland-wasm contract concepts.
 * This is an internal adapter layer, not a public boundary.
 */

#ifndef IXLAND_WASM_ADAPTER_H
#define IXLAND_WASM_ADAPTER_H

#include <ixland/wasm/types.h>
#include <ixland/wasm/engine.h>
#include <ixland/wasm/host.h>
#include <ixland/wasm/wasi.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * ADAPTER LIFECYCLE
 * ============================================================================ */

/* Initialize the adapter layer (idempotent) */
int ixland_wasm_adapter_init(void);

/* Deinitialize the adapter layer */
void ixland_wasm_adapter_deinit(void);

/* Check if adapter is initialized */
bool ixland_wasm_adapter_is_initialized(void);

/* ============================================================================
 * ENGINE ADAPTER
 * ============================================================================ */

/* Get the current engine handle (creates if needed) */
ixland_wasm_engine_t *ixland_wasm_adapter_get_engine(void);

/* Get the backend kind for current implementation */
ixland_wasm_backend_kind_t ixland_wasm_adapter_get_backend(void);

/* ============================================================================
 * MODULE ADAPTER
 * ============================================================================ */

/* Load a module through the adapter */
ixland_wasm_module_t *ixland_wasm_adapter_load_module(
    const uint8_t *wasm_buf,
    uint32_t wasm_size
);

/* Unload a module through the adapter */
void ixland_wasm_adapter_unload_module(ixland_wasm_module_t *module);

/* Get module info using engine.h contract type ixland_wasm_module_info_t */
ixland_wasm_error_t ixland_wasm_adapter_get_module_info(
    ixland_wasm_module_t *module,
    ixland_wasm_module_info_t *out_info
);

/* ============================================================================
 * INSTANCE ADAPTER
 * ============================================================================ */

/* Create an instance from a module */
ixland_wasm_instance_t *ixland_wasm_adapter_instantiate(
    ixland_wasm_module_t *module
);

/* Destroy an instance */
void ixland_wasm_adapter_destroy_instance(ixland_wasm_instance_t *instance);

/* ============================================================================
 * HOST SERVICES ADAPTER
 * ============================================================================ */

/* Register host functions for a module */
int ixland_wasm_adapter_register_host(
    ixland_wasm_module_t *module,
    const ixland_wasm_host_import_t *imports,
    uint32_t import_count
);

/* Call a function in an instance */
int ixland_wasm_adapter_call(
    ixland_wasm_instance_t *instance,
    const char *func_name,
    const ixland_wasm_value_t *args,
    uint32_t argc,
    ixland_wasm_value_t *results,
    uint32_t result_count
);

/* ============================================================================
 * WASI ADAPTER HELPERS
 * ============================================================================ */

/* Convert system errno to WASI errno - uses wasi.h contract definitions */
ixland_wasi_errno_t ixland_wasm_adapter_convert_errno(int system_errno);

#ifdef __cplusplus
}
#endif

#endif /* IXLAND_WASM_ADAPTER_H */
