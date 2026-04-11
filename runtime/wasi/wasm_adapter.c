/*
 * wasm_adapter.c - Internal Wasm Contract Adapter Implementation
 *
 * Maps current WAMR integration onto neutral ixland-wasm contract concepts.
 */

#include "wasm_adapter.h"

#include <ixland/wasm/engine.h>
#include <ixland/wasm/wasi.h>

#include <stdlib.h>
#include <string.h>

#include "ixland_wamr.h"

/* ============================================================================
 * ADAPTER STATE
 * ============================================================================ */

/* Internal engine implementation structure (opaque to contract) */
struct ixland_wasm_engine_impl {
    ixland_wasm_backend_kind_t backend;
    char debug_name[64];
};

static struct {
    bool initialized;
    struct ixland_wasm_engine_impl engine_impl; /* Concrete engine implementation */
    ixland_wasm_engine_config_t config;         /* Engine configuration (from engine.h) */
} adapter_state = {false,
                   {0},
                   {.backend = IXLAND_WASM_BACKEND_WAMR,
                    .stack_size = 8192,
                    .heap_size = 65536,
                    .heap_max = 0,       /* Unlimited */
                    .enable_wasi = true, /* WASI enabled by default */
                    .enable_threading = false,
                    .enable_simd = false,
                    .enable_bulk_memory = true,
                    .enable_reference_types = false,
                    .debug_name = "ixland-wasm-adapter"}};

/* ============================================================================
 * ADAPTER LIFECYCLE
 * ============================================================================ */

int ixland_wasm_adapter_init(void) {
    if (adapter_state.initialized) {
        return 0;
    }

    /* Initialize underlying WAMR runtime */
    if (ixland_wamr_init() != 0) {
        return -1;
    }

    adapter_state.initialized = true;
    return 0;
}

void ixland_wasm_adapter_deinit(void) {
    if (!adapter_state.initialized) {
        return;
    }

    ixland_wamr_deinit();
    adapter_state.initialized = false;
}

bool ixland_wasm_adapter_is_initialized(void) {
    return adapter_state.initialized;
}

/* ============================================================================
 * ENGINE ADAPTER
 * ============================================================================ */

ixland_wasm_engine_t *ixland_wasm_adapter_get_engine(void) {
    if (!adapter_state.initialized) {
        if (ixland_wasm_adapter_init() != 0) {
            return NULL;
        }
    }
    /* Cast our internal implementation to the opaque handle type from contract */
    return (ixland_wasm_engine_t *)&adapter_state.engine_impl;
}

ixland_wasm_backend_kind_t ixland_wasm_adapter_get_backend(void) {
    return IXLAND_WASM_BACKEND_WAMR;
}

/* ============================================================================
 * MODULE ADAPTER
 * ============================================================================ */

/* Static module info for the loaded module (from contract types.h) */
static ixland_wasm_module_info_t g_module_info = {0};

ixland_wasm_module_t *ixland_wasm_adapter_load_module(const uint8_t *wasm_buf, uint32_t wasm_size) {
    /* For now, delegate to WAMR implementation */
    if (ixland_wamr_load_module(wasm_buf, wasm_size) != 0) {
        return NULL;
    }

    /* Populate module info using contract types from engine.h */
    g_module_info.version = 0x01;    /* WASM MVP version */
    g_module_info.memories = 1;      /* WAMR single memory */
    g_module_info.tables = 1;        /* Default table */
    g_module_info.globals = 0;       /* Unknown until introspection */
    g_module_info.functions = 0;     /* Unknown until introspection */
    g_module_info.exports = 0;       /* Unknown until introspection */
    g_module_info.imports = 0;       /* Unknown until introspection */
    g_module_info.has_start = false; /* Unknown */
    g_module_info.is_wasi = adapter_state.config.enable_wasi;

    /* Return non-NULL to indicate success (actual handle managed by WAMR) */
    return (ixland_wasm_module_t *)1;
}

/* Get module info - uses engine.h contract type ixland_wasm_module_info_t */
ixland_wasm_error_t ixland_wasm_adapter_get_module_info(ixland_wasm_module_t *module,
                                                        ixland_wasm_module_info_t *out_info) {
    if (module == NULL || out_info == NULL) {
        return IXLAND_WASM_ERROR_INVALID;
    }
    *out_info = g_module_info;
    return IXLAND_WASM_OK;
}

void ixland_wasm_adapter_unload_module(ixland_wasm_module_t *module) {
    (void)module;
    memset(&g_module_info, 0, sizeof(g_module_info));
    ixland_wamr_unload_module();
}

/* ============================================================================
 * INSTANCE ADAPTER
 * ============================================================================ */

ixland_wasm_instance_t *ixland_wasm_adapter_instantiate(ixland_wasm_module_t *module) {
    (void)module;
    /* WAMR instance is created automatically on module load */
    return (ixland_wasm_instance_t *)1;
}

void ixland_wasm_adapter_destroy_instance(ixland_wasm_instance_t *instance) {
    (void)instance;
    /* WAMR cleanup handled by module unload */
}

/* ============================================================================
 * HOST SERVICES ADAPTER
 * ============================================================================ */

int ixland_wasm_adapter_register_host(ixland_wasm_module_t *module,
                                      const ixland_wasm_host_import_t *imports,
                                      uint32_t import_count) {
    (void)module;
    /* The ixland_wasm_host_import_t type comes from host.h contract */
    /* WASI preopens configuration comes from wasi.h */
    static ixland_wasi_preopen_t default_preopens[] = {
        {0, "/",
         IXLAND_WASI_RIGHT_PATH_OPEN | IXLAND_WASI_RIGHT_FD_READ | IXLAND_WASI_RIGHT_FD_WRITE, 0},
        {1, ".",
         IXLAND_WASI_RIGHT_PATH_OPEN | IXLAND_WASI_RIGHT_FD_READ | IXLAND_WASI_RIGHT_FD_WRITE, 0}};
    (void)default_preopens;
    (void)imports;
    (void)import_count;
    /* TODO: Map to WAMR native registration when implemented */
    return 0;
}

/* WASI errno conversion - uses wasi.h contract */
ixland_wasi_errno_t ixland_wasm_adapter_convert_errno(int system_errno) {
    /* Basic errno mapping using WASI contract definitions from wasi.h */
    switch (system_errno) {
    case 0:
        return IXLAND_WASI_ESUCCESS;
    case 2: /* ENOENT */
        return IXLAND_WASI_ENOENT;
    case 5: /* EIO */
        return IXLAND_WASI_EIO;
    case 12: /* ENOMEM */
        return IXLAND_WASI_ENOMEM;
    case 13: /* EACCES */
        return IXLAND_WASI_EACCES;
    case 17: /* EEXIST */
        return IXLAND_WASI_EEXIST;
    case 20: /* ENOTDIR */
        return IXLAND_WASI_ENOTDIR;
    case 21: /* EISDIR */
        return IXLAND_WASI_EISDIR;
    case 22: /* EINVAL */
        return IXLAND_WASI_EINVAL;
    case 28: /* ENOSPC */
        return IXLAND_WASI_ENOSPC;
    case 36: /* ENAMETOOLONG */
        return IXLAND_WASI_ENAMETOOLONG;
    default:
        return IXLAND_WASI_EINVAL;
    }
}

int ixland_wasm_adapter_call(ixland_wasm_instance_t *instance, const char *func_name,
                             const ixland_wasm_value_t *args, uint32_t argc,
                             ixland_wasm_value_t *results, uint32_t result_count) {
    (void)instance;
    (void)args;
    (void)argc;
    (void)results;
    (void)result_count;

    /* Convert arguments to WAMR format and call */
    uint32_t argv[16] = {0};
    uint32_t argc_wamr = (argc > 16) ? 16 : argc;

    for (uint32_t i = 0; i < argc_wamr; i++) {
        switch (args[i].kind) {
        case IXLAND_WASM_VALUE_I32:
            argv[i] = (uint32_t)args[i].value.i32;
            break;
        case IXLAND_WASM_VALUE_I64:
            /* Cannot pass i64 in single argv slot */
            argv[i] = 0;
            break;
        default:
            argv[i] = 0;
            break;
        }
    }

    return ixland_wamr_call_function(func_name, argc_wamr, argv);
}
