# WAMR Vendor Headers

This directory contains the WebAssembly Micro Runtime (WAMR) SDK headers.

**IMPORTANT:** These are external/vendor headers from the WAMR project.
They are NOT part of the iXland public API surface.

## Source

- **Original Location:** `ixland-system/include/wamr/`
- **Current Location:** `ixland-system/vendor/wamr/include/`
- **Upstream:** Intel WebAssembly Micro Runtime (WAMR)

## Public API Contract

The public Wasm contract is defined in `ixland-wasm/` - NOT in these headers.
These headers are for internal use by the ixland-system WAMR runtime only.

## Contents

- `wasm_export.h` - WAMR runtime APIs
- `wasm_c_api.h` - C API for WASM modules
- `aot_export.h` - Ahead-of-Time compilation exports
- `aot_comp_option.h` - AOT compilation options
- `gc_export.h` - Garbage collection exports
- `lib_export.h` - Library export macros

## Do Not Include Directly

Code outside ixland-system should NOT include these headers.
Use the public `ixland-wasm` headers instead.
