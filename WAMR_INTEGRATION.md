# WAMR Integration for libixland

## Overview

libixland integrates WebAssembly Micro Runtime (WAMR) on iOS through a **libixland-owned adapter layer** and an **externally built upstream runtime**.

This repository follows these rules:

- `deps/wamr/` is upstream source and must remain read-only
- WAMR is built **out-of-tree** for iOS device and simulator
- libixland and libiwasm are separate artifacts
- app customers must use **`ixland_wamr_*` only**

## Artifact Model

Expected build outputs:

- `lib/libixland-sim.a`
- `lib/libixland-device.a`
- `build/wamr-simulator/libiwasm.a`
- `build/wamr-device/libiwasm.a`

App/test targets link both libraries explicitly.

## Ownership Boundary

### libixland-owned

- `include/ixland/ixland_wamr.h`
- `src/ixland/wamr/ixland_wamr_simple.c`
- any WASI-to-libixland bridge code
- all iOS toolchain/build policy outside `deps/wamr`

### upstream WAMR-owned

- runtime implementation under `deps/wamr/`
- upstream headers and internal APIs

## Building WAMR

Use the repository-owned wrapper script:

```bash
./build_wamr_ios_static.sh simulator
./build_wamr_ios_static.sh device
```

Or build both:

```bash
./build_wamr_ios_static.sh all
```

This script configures upstream WAMR with iOS settings **without editing the submodule**.

## Building libixland and WAMR Together

```bash
make sdk-sim
make sdk-device
make sdk
```

## Public API

Applications should use only these adapter APIs:

- `ixland_wamr_init()`
- `ixland_wamr_deinit()`
- `ixland_wamr_is_initialized()`
- `ixland_wamr_load_module()`
- `ixland_wamr_unload_module()`
- `ixland_wamr_validate_wasm()`
- `ixland_wamr_call_function()`
- `ixland_wamr_function_exists()`
- `ixland_wamr_get_memory_size()`
- `ixland_wamr_memory_read()`
- `ixland_wamr_memory_write()`
- `ixland_wamr_get_export_list()`
- `ixland_wamr_free_export_list()`
- `ixland_wamr_get_error()`

## Unsupported Consumer Usage

The following are not part of the supported app-facing contract:

- direct `wasm_runtime_*` usage
- direct inclusion of upstream WAMR headers in app code
- linking apps as though WAMR were the primary SDK surface

## iOS Constraints

- interpreter mode only
- no JIT
- no AOT/JIT runtime code generation
- sandbox-aware file and WASI behavior
- validate all crossings between WASI and libixland
