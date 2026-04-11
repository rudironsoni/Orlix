/*
 * ixland_wamr_simple.c - Simplified WAMR integration for libixland
 *
 * Basic WebAssembly runtime support without full WASI
 * Minimal implementation that works with current WAMR API
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* WAMR includes */
#include <wasm_export.h>

/* ixland includes */
#include "ixland_wamr.h"

/* ============================================================================
 * WAMR STATE
 * ============================================================================ */

typedef struct {
    wasm_module_t module;
    wasm_module_inst_t instance;
    wasm_exec_env_t exec_env;
    bool initialized;
} ixland_wamr_state_t;

static ixland_wamr_state_t g_wamr_state = {0};
static char g_error_buf[256] = {0};

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

int ixland_wamr_init(void) {
    if (g_wamr_state.initialized) {
        return 0;  // Already initialized
    }

    /* Initialize WAMR runtime - no arguments needed */
    if (!wasm_runtime_init()) {
        strncpy(g_error_buf, "Failed to initialize WAMR runtime", sizeof(g_error_buf));
        return -1;
    }

    g_wamr_state.initialized = true;

    if (getenv("IXLAND_DEBUG")) {
        fprintf(stderr, "ixland_wamr: Initialized successfully\n");
    }

    return 0;
}

void ixland_wamr_deinit(void) {
    if (!g_wamr_state.initialized) {
        return;
    }

    /* Cleanup any loaded module */
    if (g_wamr_state.exec_env) {
        wasm_runtime_destroy_exec_env(g_wamr_state.exec_env);
        g_wamr_state.exec_env = NULL;
    }

    if (g_wamr_state.instance) {
        wasm_runtime_deinstantiate(g_wamr_state.instance);
        g_wamr_state.instance = NULL;
    }

    if (g_wamr_state.module) {
        wasm_runtime_unload(g_wamr_state.module);
        g_wamr_state.module = NULL;
    }

    /* Destroy runtime */
    wasm_runtime_destroy();

    g_wamr_state.initialized = false;

    if (getenv("IXLAND_DEBUG")) {
        fprintf(stderr, "ixland_wamr: Deinitialized\n");
    }
}

bool ixland_wamr_is_initialized(void) {
    return g_wamr_state.initialized;
}

/* ============================================================================
 * MODULE LOADING
 * ============================================================================ */

int ixland_wamr_load_module(const uint8_t *wasm_buf, uint32_t wasm_size) {
    if (!g_wamr_state.initialized) {
        strncpy(g_error_buf, "Runtime not initialized", sizeof(g_error_buf));
        return -1;
    }

    /* Cleanup previous module if any */
    if (g_wamr_state.exec_env) {
        wasm_runtime_destroy_exec_env(g_wamr_state.exec_env);
        g_wamr_state.exec_env = NULL;
    }

    if (g_wamr_state.instance) {
        wasm_runtime_deinstantiate(g_wamr_state.instance);
        g_wamr_state.instance = NULL;
    }

    if (g_wamr_state.module) {
        wasm_runtime_unload(g_wamr_state.module);
        g_wamr_state.module = NULL;
    }

    /* Load WASM module */
    g_wamr_state.module =
        wasm_runtime_load((uint8_t *)wasm_buf, wasm_size, g_error_buf, sizeof(g_error_buf));

    if (!g_wamr_state.module) {
        return -1;
    }

    /* Instantiate module */
    g_wamr_state.instance = wasm_runtime_instantiate(g_wamr_state.module, 64 * 1024, 64 * 1024,
                                                     g_error_buf, sizeof(g_error_buf));
    if (!g_wamr_state.instance) {
        wasm_runtime_unload(g_wamr_state.module);
        g_wamr_state.module = NULL;
        return -1;
    }

    /* Create execution environment */
    g_wamr_state.exec_env = wasm_runtime_create_exec_env(g_wamr_state.instance, 64 * 1024);
    if (!g_wamr_state.exec_env) {
        strncpy(g_error_buf, "Failed to create exec env", sizeof(g_error_buf));
        wasm_runtime_deinstantiate(g_wamr_state.instance);
        wasm_runtime_unload(g_wamr_state.module);
        g_wamr_state.instance = NULL;
        g_wamr_state.module = NULL;
        return -1;
    }

    if (getenv("IXLAND_DEBUG")) {
        fprintf(stderr, "ixland_wamr: Module loaded successfully\n");
    }

    return 0;
}

void ixland_wamr_unload_module(void) {
    if (g_wamr_state.exec_env) {
        wasm_runtime_destroy_exec_env(g_wamr_state.exec_env);
        g_wamr_state.exec_env = NULL;
    }

    if (g_wamr_state.instance) {
        wasm_runtime_deinstantiate(g_wamr_state.instance);
        g_wamr_state.instance = NULL;
    }

    if (g_wamr_state.module) {
        wasm_runtime_unload(g_wamr_state.module);
        g_wamr_state.module = NULL;
    }
}

/* ============================================================================
 * FUNCTION EXECUTION
 * ============================================================================ */

int ixland_wamr_call_function(const char *func_name, uint32_t argc, uint32_t argv[]) {
    if (!g_wamr_state.initialized || !g_wamr_state.instance || !g_wamr_state.exec_env) {
        strncpy(g_error_buf, "No module loaded", sizeof(g_error_buf));
        return -1;
    }

    /* Lookup function */
    wasm_function_inst_t func = wasm_runtime_lookup_function(g_wamr_state.instance, func_name);
    if (!func) {
        snprintf(g_error_buf, sizeof(g_error_buf), "Function '%s' not found", func_name);
        return -1;
    }

    /* Call function - 4 arguments: exec_env, function, argc, argv */
    bool success = wasm_runtime_call_wasm(g_wamr_state.exec_env, func, argc, argv);

    if (!success) {
        const char *exception = wasm_runtime_get_exception(g_wamr_state.instance);
        snprintf(g_error_buf, sizeof(g_error_buf), "Function call failed: %s",
                 exception ? exception : "unknown error");
        return -1;
    }

    if (getenv("IXLAND_DEBUG")) {
        fprintf(stderr, "ixland_wamr: Function '%s' called successfully\n", func_name);
    }

    return 0;
}

bool ixland_wamr_function_exists(const char *func_name) {
    if (!g_wamr_state.initialized || !g_wamr_state.instance) {
        return false;
    }

    wasm_function_inst_t func = wasm_runtime_lookup_function(g_wamr_state.instance, func_name);
    return (func != NULL);
}

/* ============================================================================
 * VALIDATION
 * ============================================================================ */

bool ixland_wamr_validate_wasm(const uint8_t *wasm_buf, uint32_t wasm_size) {
    if (!wasm_buf || wasm_size == 0) {
        return false;
    }

    /* Check WASM magic number: \0asm */
    if (wasm_size < 4) {
        return false;
    }

    return (wasm_buf[0] == 0x00 && wasm_buf[1] == 0x61 && wasm_buf[2] == 0x73 &&
            wasm_buf[3] == 0x6d);
}

/* ============================================================================
 * MEMORY ACCESS
 * ============================================================================ */

int ixland_wamr_memory_read(uint32_t offset, void *buf, uint32_t len) {
    if (!g_wamr_state.initialized || !g_wamr_state.instance || !buf) {
        return -1;
    }

    uint8_t *memory = wasm_runtime_addr_app_to_native(g_wamr_state.instance, offset);
    if (!memory) {
        strncpy(g_error_buf, "Invalid memory offset", sizeof(g_error_buf));
        return -1;
    }

    memcpy(buf, memory, len);
    return 0;
}

int ixland_wamr_memory_write(uint32_t offset, const void *buf, uint32_t len) {
    if (!g_wamr_state.initialized || !g_wamr_state.instance || !buf) {
        return -1;
    }

    uint8_t *memory = wasm_runtime_addr_app_to_native(g_wamr_state.instance, offset);
    if (!memory) {
        strncpy(g_error_buf, "Invalid memory offset", sizeof(g_error_buf));
        return -1;
    }

    memcpy(memory, buf, len);
    return 0;
}

uint32_t ixland_wamr_get_memory_size(void) {
    /* Return default memory size - WAMR doesn't expose this directly */
    /* Each page is 64KB, default is usually 1 page */
    if (!g_wamr_state.instance) {
        return 0;
    }
    return 64 * 1024;  // 1 page
}

/* ============================================================================
 * EXPORT INTROSPECTION (Simplified)
 * ============================================================================ */

int ixland_wamr_get_export_list(char ***names, uint32_t *count) {
    /* Simplified implementation - just return empty list */
    /* Full implementation would need to iterate WASM exports */
    if (!names || !count) {
        return -1;
    }

    *names = NULL;
    *count = 0;
    return 0;
}

void ixland_wamr_free_export_list(char **names, uint32_t count) {
    (void)names;
    (void)count;
    /* Nothing to free in simplified implementation */
}

/* ============================================================================
 * ERROR HANDLING
 * ============================================================================ */

const char *ixland_wamr_get_error(void) {
    return g_error_buf;
}
