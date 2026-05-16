# Boot To Virtio Probe Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Prove Milestone 3 by carrying valid Orlix boot configs to a private boot handoff and preparing the generated Linux port so profile DTS data can drive upstream virtio-mmio probing.

**Architecture:** Keep the public API bootloader-shaped and private. Add a private boot handoff seam that proves valid configs reach the next layer without exposing raw Linux boot parameters. Extend durable `arch/orlix` profile DTS and profile configs so upstream OF/platform, virtio-mmio, and `virtio_blk` probe paths are present, while explicitly not claiming block-device creation, block I/O, root assembly, or userspace boot.

**Tech Stack:** C11 bootloader contract tests, Bash contract tests, GNU Make proof targets, upstream Linux 6.12 Kbuild, Orlix Linux overlay under `Linux/ports/orlix`.

---

## File Structure

- Modify: `tests/bootloader_contract.c` owns public API validation plus private handoff proof.
- Create: `boot/handoff.h` owns the private bootloader-to-next-layer handoff test seam.
- Create: `boot/handoff.c` owns the minimal private handoff implementation and test observability.
- Modify: `boot/loader.c` calls the private handoff after valid boot input preparation.
- Modify: `Makefile` compiles the new handoff file in the bootloader contract and adds `test-milestone3-boot-probe-contract`.
- Create: `tests/milestone3_boot_probe_contract.sh` owns generated-port and durable-input checks for boot-to-virtio-probe.
- Modify: `Linux/ports/orlix/overlay/arch/orlix/boot/dts/appstore.dts` adds virtio-mmio probe-shape nodes.
- Modify: `Linux/ports/orlix/overlay/arch/orlix/boot/dts/development.dts` adds virtio-mmio probe-shape nodes.
- Modify: `Linux/ports/orlix/overlay/arch/orlix/boot/dts/enterprise.dts` adds virtio-mmio probe-shape nodes.
- Modify: `Linux/ports/orlix/configs/appstore_defconfig` enables OF/platform and upstream virtio probe options and disables custom Orlix block skeletons.
- Modify: `Linux/ports/orlix/configs/development_defconfig` enables OF/platform and upstream virtio probe options and disables custom Orlix block skeletons.
- Modify: `Linux/ports/orlix/configs/enterprise_defconfig` keeps profile parity for the same probe-shape options.
- Modify: `Linux/ports/orlix/overlay/arch/orlix/Kconfig` selects architecture capabilities required by upstream OF and virtio-mmio Kconfig dependencies.
- Modify: `README.md` documents the Milestone 3 proof boundary.

## Task 1: Add A Failing Boot Handoff Contract

**Files:**
- Modify: `tests/bootloader_contract.c`
- Modify: `Makefile`

- [ ] **Step 1: Include the private handoff header in the bootloader contract**

In `tests/bootloader_contract.c`, add this include after `#include "boot/input.h"`:

```c
#include "boot/handoff.h"
```

- [ ] **Step 2: Assert valid boots reach the handoff seam**

In `tests/bootloader_contract.c`, replace the final valid `OrlixBoot` check:

```c
    if (OrlixBoot(&config) != ORLIX_BOOT_STATUS_UNAVAILABLE) {
        return 13;
    }
```

with:

```c
    OrlixBootResetHandoff();
    if (OrlixBoot(&config) != ORLIX_BOOT_STATUS_UNAVAILABLE) {
        return 13;
    }
    if (OrlixBootHandoffCount() != 1) {
        return 14;
    }
    if (!OrlixBootLastHandoff()) {
        return 15;
    }
    if (OrlixBootLastHandoff()->profile != ORLIX_BOOT_PROFILE_APPSTORE) {
        return 16;
    }
    if (expect_string(OrlixBootLastHandoff()->profile_dtb_path,
                      "arch/orlix/boot/dts/appstore.dtb") != 0) {
        return 17;
    }
    if (expect_string(OrlixBootLastHandoff()->kernel_cmdline,
                      "console=ttyS0 root=/dev/ram0 rw orlix.profile=appstore") != 0) {
        return 18;
    }
```

- [ ] **Step 3: Compile the bootloader contract with testing hooks and handoff implementation**

In `Makefile`, update the `test-bootloader-contract` compile command so it becomes:

```make
	$(CC) -std=c11 -Wall -Wextra -Werror \
		-DORLIX_BOOT_TESTING \
		-I. \
		-IOrlixKernel/include \
		boot/loader.c \
		boot/params.c \
		boot/handoff.c \
		tests/bootloader_contract.c \
		-o "$$build_dir/bootloader_contract"; \
```

- [ ] **Step 4: Run the bootloader contract and verify RED**

Run:

```bash
rtk make test-bootloader-contract
```

Expected: FAIL with a compiler error containing `boot/handoff.h` because the private handoff seam does not exist yet.

- [ ] **Step 5: Commit the failing handoff contract**

Run:

```bash
rtk git add Makefile tests/bootloader_contract.c
rtk git commit -m "test: define boot handoff contract"
```

## Task 2: Implement Private Boot Handoff Seam

**Files:**
- Create: `boot/handoff.h`
- Create: `boot/handoff.c`
- Modify: `boot/loader.c`

- [ ] **Step 1: Create the private handoff header**

Create `boot/handoff.h` with:

```c
#ifndef ORLIX_BOOT_HANDOFF_H
#define ORLIX_BOOT_HANDOFF_H

#include "boot/input.h"

__attribute__((visibility("hidden"))) int OrlixBootHandoff(
    const struct OrlixBootInput *input);

#if defined(ORLIX_BOOT_TESTING)
void OrlixBootResetHandoff(void);
int OrlixBootHandoffCount(void);
const struct OrlixBootInput *OrlixBootLastHandoff(void);
#endif

#endif
```

- [ ] **Step 2: Create the private handoff implementation**

Create `boot/handoff.c` with:

```c
#include "boot/handoff.h"

#if defined(ORLIX_BOOT_TESTING)
static struct OrlixBootInput last_handoff;
static int handoff_count;
static int has_last_handoff;
#endif

__attribute__((visibility("hidden"))) int OrlixBootHandoff(
    const struct OrlixBootInput *input)
{
    if (!input) {
        return ORLIX_BOOT_STATUS_INVALID_CONFIG;
    }

#if defined(ORLIX_BOOT_TESTING)
    last_handoff = *input;
    has_last_handoff = 1;
    handoff_count++;
#else
    (void)input;
#endif

    return ORLIX_BOOT_STATUS_UNAVAILABLE;
}

#if defined(ORLIX_BOOT_TESTING)
void OrlixBootResetHandoff(void)
{
    has_last_handoff = 0;
    handoff_count = 0;
}

int OrlixBootHandoffCount(void)
{
    return handoff_count;
}

const struct OrlixBootInput *OrlixBootLastHandoff(void)
{
    return has_last_handoff ? &last_handoff : 0;
}
#endif
```

- [ ] **Step 3: Update `OrlixBoot` to call the handoff seam**

Replace `boot/loader.c` with:

```c
#include "boot/handoff.h"

int OrlixBoot(const struct OrlixBootConfig *config) {
    struct OrlixBootInput input;

    if (OrlixPrepareBootInput(config, &input) != ORLIX_BOOT_STATUS_OK) {
        return ORLIX_BOOT_STATUS_INVALID_CONFIG;
    }

    return OrlixBootHandoff(&input);
}
```

- [ ] **Step 4: Run the bootloader contract and verify GREEN**

Run:

```bash
rtk make test-bootloader-contract
```

Expected: PASS with output ending in `make: ok`.

- [ ] **Step 5: Commit the handoff implementation**

Run:

```bash
rtk git add boot/handoff.h boot/handoff.c boot/loader.c
rtk git commit -m "boot: hand valid configs to private boot seam"
```

## Task 3: Add A Failing Milestone 3 Probe Contract

**Files:**
- Create: `tests/milestone3_boot_probe_contract.sh`
- Modify: `Makefile`

- [ ] **Step 1: Create the failing Milestone 3 shell contract**

Create `tests/milestone3_boot_probe_contract.sh` with:

```bash
#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

MAKE_BIN="${MAKE_BIN:-make}"
PORT_DIR="Build/OrlixKernel/linux-6.12-port"

fail() {
    printf 'FAIL: %s\n' "$1" >&2
    exit 1
}

expect_file_contains() {
    local file="$1"
    local expected="$2"

    [ -f "$file" ] || fail "missing file: $file"
    grep -F "$expected" "$file" >/dev/null || fail "expected $file to contain: $expected"
}

expect_file_not_contains() {
    local file="$1"
    local unexpected="$2"

    [ -f "$file" ] || fail "missing file: $file"
    if grep -F "$unexpected" "$file" >/dev/null; then
        fail "expected $file not to contain: $unexpected"
    fi
}

expect_profile_probe_shape() {
    local profile="$1"
    local dts="$PORT_DIR/arch/orlix/boot/dts/$profile.dts"

    expect_file_contains "$dts" 'virtio_base: virtio@10001000 {'
    expect_file_contains "$dts" 'virtio_state: virtio@10001200 {'
    expect_file_contains "$dts" 'compatible = "virtio,mmio";'
    expect_file_contains "$dts" 'reg = <0x0 0x10001000 0x0 0x200>;'
    expect_file_contains "$dts" 'reg = <0x0 0x10001200 0x0 0x200>;'
    expect_file_contains "$dts" 'interrupts = <32>;'
    expect_file_contains "$dts" 'interrupts = <33>;'
}

expect_profile_config() {
    local config="$1"

    expect_file_contains "$config" 'CONFIG_OF=y'
    expect_file_contains "$config" 'CONFIG_VIRTIO=y'
    expect_file_contains "$config" 'CONFIG_VIRTIO_MMIO=y'
    expect_file_contains "$config" 'CONFIG_VIRTIO_BLK=y'
    expect_file_not_contains "$config" 'CONFIG_ORLIX_BLOCK=y'
}

"$MAKE_BIN" prepare-orlixkernel-port PROFILE=appstore >/dev/null

expect_profile_probe_shape appstore
expect_profile_probe_shape development
expect_profile_probe_shape enterprise

expect_profile_config Linux/ports/orlix/configs/appstore_defconfig
expect_profile_config Linux/ports/orlix/configs/development_defconfig
expect_profile_config Linux/ports/orlix/configs/enterprise_defconfig

expect_file_contains "$PORT_DIR/arch/orlix/Kconfig" 'select OF'
expect_file_contains "$PORT_DIR/arch/orlix/Kconfig" 'select HAS_IOMEM'
expect_file_contains "$PORT_DIR/arch/orlix/Kconfig" 'select HAS_DMA'

if grep -R 'OrlixBootHandoff' OrlixKernel/include >/dev/null; then
    fail 'private boot handoff must not be public product API'
fi
```

- [ ] **Step 2: Make the shell contract executable**

Run:

```bash
chmod +x tests/milestone3_boot_probe_contract.sh
```

Expected: no output and exit 0.

- [ ] **Step 3: Add the Makefile target**

In `Makefile`, update `.PHONY` to include `test-milestone3-boot-probe-contract`:

```make
.PHONY: bootstrap-linux-upstream validate-orlix-profile prepare-orlixkernel-port build-linux-kernel test-bootloader-contract test-milestone1-contract test-milestone2-boot-contract test-milestone3-boot-probe-contract
```

Add this target after `test-milestone2-boot-contract`:

```make
test-milestone3-boot-probe-contract: test-milestone2-boot-contract
	@MAKE_BIN="$(MAKE)" tests/milestone3_boot_probe_contract.sh
```

- [ ] **Step 4: Run the Milestone 3 contract and verify RED**

Run:

```bash
rtk make test-milestone3-boot-probe-contract
```

Expected: FAIL with output containing `expected Build/OrlixKernel/linux-6.12-port/arch/orlix/boot/dts/appstore.dts to contain: virtio_base: virtio@10001000 {`.

- [ ] **Step 5: Commit the failing Milestone 3 contract**

Run:

```bash
rtk git add Makefile tests/milestone3_boot_probe_contract.sh
rtk git commit -m "test: add boot to virtio probe contract"
```

## Task 4: Add Virtio Probe Shape To Profile DTS And Configs

**Files:**
- Modify: `Linux/ports/orlix/overlay/arch/orlix/boot/dts/appstore.dts`
- Modify: `Linux/ports/orlix/overlay/arch/orlix/boot/dts/development.dts`
- Modify: `Linux/ports/orlix/overlay/arch/orlix/boot/dts/enterprise.dts`
- Modify: `Linux/ports/orlix/configs/appstore_defconfig`
- Modify: `Linux/ports/orlix/configs/development_defconfig`
- Modify: `Linux/ports/orlix/configs/enterprise_defconfig`
- Modify: `Linux/ports/orlix/overlay/arch/orlix/Kconfig`

- [ ] **Step 1: Add virtio-mmio nodes to App Store DTS**

In `Linux/ports/orlix/overlay/arch/orlix/boot/dts/appstore.dts`, insert these nodes after the `serial0` node and before the root closing brace:

```dts

	virtio_base: virtio@10001000 {
		compatible = "virtio,mmio";
		reg = <0x0 0x10001000 0x0 0x200>;
		interrupts = <32>;
		status = "okay";
	};

	virtio_state: virtio@10001200 {
		compatible = "virtio,mmio";
		reg = <0x0 0x10001200 0x0 0x200>;
		interrupts = <33>;
		status = "okay";
	};
```

- [ ] **Step 2: Add virtio-mmio nodes to Development DTS**

In `Linux/ports/orlix/overlay/arch/orlix/boot/dts/development.dts`, insert these nodes after the `serial0` node and before the root closing brace:

```dts

	virtio_base: virtio@10001000 {
		compatible = "virtio,mmio";
		reg = <0x0 0x10001000 0x0 0x200>;
		interrupts = <32>;
		status = "okay";
	};

	virtio_state: virtio@10001200 {
		compatible = "virtio,mmio";
		reg = <0x0 0x10001200 0x0 0x200>;
		interrupts = <33>;
		status = "okay";
	};
```

- [ ] **Step 3: Add virtio-mmio nodes to Enterprise DTS**

In `Linux/ports/orlix/overlay/arch/orlix/boot/dts/enterprise.dts`, insert these nodes after the `serial0` node and before the root closing brace:

```dts

	virtio_base: virtio@10001000 {
		compatible = "virtio,mmio";
		reg = <0x0 0x10001000 0x0 0x200>;
		interrupts = <32>;
		status = "okay";
	};

	virtio_state: virtio@10001200 {
		compatible = "virtio,mmio";
		reg = <0x0 0x10001200 0x0 0x200>;
		interrupts = <33>;
		status = "okay";
	};
```

- [ ] **Step 4: Update the App Store defconfig**

In `Linux/ports/orlix/configs/appstore_defconfig`, remove:

```text
CONFIG_ORLIX_BLOCK=y
```

Add these config lines after `CONFIG_ORLIX=y`:

```text
CONFIG_OF=y
CONFIG_VIRTIO=y
CONFIG_VIRTIO_MMIO=y
CONFIG_VIRTIO_BLK=y
```

- [ ] **Step 5: Update the Development defconfig**

In `Linux/ports/orlix/configs/development_defconfig`, remove:

```text
CONFIG_ORLIX_BLOCK=y
```

Add these config lines after `CONFIG_ORLIX=y`:

```text
CONFIG_OF=y
CONFIG_VIRTIO=y
CONFIG_VIRTIO_MMIO=y
CONFIG_VIRTIO_BLK=y
```

- [ ] **Step 6: Update the Enterprise defconfig**

In `Linux/ports/orlix/configs/enterprise_defconfig`, remove:

```text
CONFIG_ORLIX_BLOCK=y
```

Add these config lines after `CONFIG_ORLIX=y`:

```text
CONFIG_OF=y
CONFIG_VIRTIO=y
CONFIG_VIRTIO_MMIO=y
CONFIG_VIRTIO_BLK=y
```

- [ ] **Step 7: Select architecture capabilities for OF and virtio-mmio**

In `Linux/ports/orlix/overlay/arch/orlix/Kconfig`, update `config ORLIX` so it includes:

```kconfig
	select OF
	select HAS_IOMEM
	select HAS_DMA
```

The resulting block should be:

```kconfig
config ORLIX
	bool "Orlix architecture"
	default y
	select GENERIC_IRQ_SHOW
	select HAVE_PAGE_SIZE_4KB
	select OF
	select HAS_IOMEM
	select HAS_DMA
```

- [ ] **Step 8: Run the Milestone 3 contract and verify GREEN**

Run:

```bash
rtk make test-milestone3-boot-probe-contract
```

Expected: PASS with output ending in `make: ok`.

- [ ] **Step 9: Commit probe-shape DTS and config changes**

Run:

```bash
rtk git add Linux/ports/orlix/overlay/arch/orlix/boot/dts Linux/ports/orlix/configs Linux/ports/orlix/overlay/arch/orlix/Kconfig
rtk git commit -m "linux: add boot to virtio probe shape"
```

## Task 5: Document Milestone 3 Proof Boundary

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Add Milestone 3 proof documentation**

In `README.md`, after the Milestone 2 proof paragraph, add:

````markdown
Milestone 3 boot-to-virtio-probe proof keeps the dependency chain honest. It verifies that valid boot configs reach a private boot handoff, that profile DTS files describe virtio-mmio probe-shape devices, and that profile configs enable upstream OF, virtio-mmio, and virtio-block probe paths:

```bash
make test-milestone3-boot-probe-contract
```

Milestone 3 does not prove `/dev/vda`, `/dev/vdb`, virtio-block request I/O, host-backed disk persistence, initramfs loading, OverlayFS root assembly, or userspace boot.
````

- [ ] **Step 2: Run docs-adjacent contracts**

Run:

```bash
rtk make test-milestone1-contract
rtk make test-milestone2-boot-contract
rtk make test-milestone3-boot-probe-contract
```

Expected: all pass with `make: ok`.

- [ ] **Step 3: Commit README update**

Run:

```bash
rtk git add README.md
rtk git commit -m "docs: document boot to virtio probe proof"
```

## Task 6: Final Verification And Review

**Files:**
- No planned source edits.

- [ ] **Step 1: Run required build proofs**

Run:

```bash
rtk make prepare-orlixkernel-port PROFILE=appstore
rtk make build-linux-kernel PROFILE=appstore
rtk make build-linux-kernel PROFILE=development
```

Expected: all exit 0. The appstore and development build outputs produce `Build/OrlixKernel/build/appstore/vmlinux` and `Build/OrlixKernel/build/development/vmlinux`.

- [ ] **Step 2: Run all milestone contracts**

Run:

```bash
rtk make test-milestone1-contract
rtk make test-bootloader-contract
rtk make test-milestone2-boot-contract
rtk make test-milestone3-boot-probe-contract
```

Expected: all pass with `make: ok`.

- [ ] **Step 3: Run diff hygiene**

Run:

```bash
rtk git diff --check
```

Expected: no output and exit 0.

- [ ] **Step 4: Request spec compliance review**

Dispatch a reviewer with this scope:

```text
Review the Boot To Virtio Probe milestone changes for spec compliance only. Confirm the milestone sequence remains dependency ordered, the public API remains bootloader-shaped, Milestone 3 does not claim block I/O or root assembly, profile DTS files contain virtio-mmio probe-shape nodes, profile configs enable upstream OF/virtio probe paths, and no custom Orlix block driver becomes the Linux-visible root disk path.
```

Expected: no spec blockers.

- [ ] **Step 5: Request code quality review**

Dispatch a reviewer with this scope:

```text
Review the Boot To Virtio Probe milestone changes for code quality and likely build/test correctness. Focus on boot/handoff.h, boot/handoff.c, boot/loader.c, bootloader tests, Milestone 3 shell contract, Makefile target wiring, profile DTS changes, profile defconfigs, and arch/orlix Kconfig changes. Distinguish true blockers from acceptable proof scaffolding.
```

Expected: no important issues requiring code changes before finishing.

- [ ] **Step 6: Commit targeted review fixes if needed**

If reviewers find issues, apply only targeted fixes, rerun Steps 1-3, and commit with a focused message. If reviewers find no required fixes, do not create an empty commit.

Expected final state: branch contains committed Boot To Virtio Probe proof changes and a clean worktree.
