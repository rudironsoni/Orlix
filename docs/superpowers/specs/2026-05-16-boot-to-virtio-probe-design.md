# Boot To Virtio Probe Design

## Goal

Milestone 3 carries Orlix from prepared boot inputs to the first honest Linux probe boundary for virtio-mmio devices.

The proof target is that the boot path reaches architecture-owned Linux setup, consumes profile device tree data, enables normal Linux OF/platform population, and makes upstream `virtio_mmio` probing possible from Orlix profile DTS data.

## Dependency Sequence

The milestone sequence is dependency ordered:

1. Kbuild `vmlinux` proof proves upstream Linux builds for `ARCH=orlix`.
2. Boot Entrypoint proves the product API prepares Linux-shaped boot inputs and profile DTS contracts.
3. Boot To Virtio Probe proves boot reaches the Linux stage where profile DTS devices can become platform devices and upstream virtio-mmio probing can be attempted.
4. Virtio Root Disks proves upstream `virtio_blk` binding, `/dev/vda`, `/dev/vdb`, and host-backed block data paths.
5. Root Assembly consumes the root disks with initramfs and upstream OverlayFS.
6. Console adds early diagnostics and Linux console paths.
7. Remaining Virtio Devices add virtio-rng, virtio-net, and external directory mount transports.

No milestone may depend on behavior that an earlier milestone has not honestly proven.

## Non-Goals

Milestone 3 does not prove:

- `/dev/vda` or `/dev/vdb` exists.
- `virtio_blk` completes device binding.
- Virtqueue request processing.
- Host-backed disk read/write.
- Initramfs loading, OverlayFS root assembly, root switching, or userspace boot.
- Console interactivity beyond whatever is already needed for diagnostics.

## Architecture

The public API remains bootloader-shaped. `OrlixBoot()` still accepts only `OrlixBootConfig` with a closed profile and opaque resource identifiers.

The bootloader prepares private Linux-shaped boot input and hands it to `arch/orlix`. `arch/orlix` owns the transition from boot input into Linux setup. It must not expose raw Linux boot parameters through `OrlixKernel/include`.

Profile DTS files remain the durable machine-description source under `Linux/ports/orlix/overlay/arch/orlix/boot/dts`. Milestone 3 extends those DTS files with normal `virtio,mmio` device nodes for future block devices, but those nodes are only a probe-shape contract in this milestone.

The Linux port enables upstream OF and virtio-mmio configuration needed for normal Linux probing. Orlix-specific code under `drivers/orlix` may provide transport/backend seams only where upstream Linux needs an Orlix-owned implementation. Linux-visible device behavior remains owned by upstream `virtio` drivers.

## Components

### Bootloader

`boot/` keeps the public product API narrow and resolves selected profiles into private boot input. Valid boot configs should reach the architecture handoff path for the test/proof environment. Invalid configs must still fail before handoff.

### `arch/orlix`

`arch/orlix` consumes profile boot data and wires the earliest Linux device-tree setup. It should use upstream OF entry points where possible, such as early FDT scan and device-tree unflattening, rather than custom Orlix parsing.

### Profile Device Trees

Profile DTS files describe the machine shape needed by Linux. For this milestone they should include virtio-mmio-compatible nodes with stable register ranges and interrupts reserved for the future base and state disk devices.

### Configs

Profile defconfigs enable the minimal upstream options needed for OF/platform and virtio-mmio probe shape. `virtio_blk` may be enabled so the next milestone does not introduce a new Linux-visible block owner, but Milestone 3 must not claim block-device binding or I/O.

### `drivers/orlix`

Any Orlix code added under `drivers/orlix` should be transport/backend scaffolding, not a custom block driver. Existing custom block skeletons must not become the Linux-visible root disk path.

## Proof

Milestone 3 proof should include:

- A bootloader/architecture contract showing valid configs reach the architecture handoff path with profile boot input.
- A Linux port contract showing generated profile DTS files contain virtio-mmio probe-shape nodes.
- A config contract showing upstream OF/platform, virtio, virtio-mmio, and probe-relevant options are enabled for the target profiles.
- Kbuild proof that appstore and development profiles still produce `vmlinux`.
- Boundary checks proving no public runtime API, custom Orlix block-device API, or generated-tree-only change was introduced.

## Error Handling

Invalid boot configs fail in `boot/` before any architecture handoff.

Missing or invalid profile boot data should fail loudly in the relevant contract tests. Milestone 3 must not silently fall back to fake device data.

If Linux boot/probe cannot be executed fully in the host environment, the proof must say so and stop at the honest probe boundary rather than claiming a runtime device result.

## Testing

Tests should be contract-oriented and match the dependency sequence:

- Existing Milestone 1 and 2 proofs remain required.
- New Milestone 3 tests should fail first when boot-to-probe contracts are absent.
- Tests should inspect durable overlay inputs and generated port-tree materialization, not generated files as source of truth.
- Tests should reject custom Orlix block behavior as the root disk implementation path.

## Documentation Updates

`CONTEXT.md`, `docs/UPSTREAM_LINUX_IOS_PORT_SPEC.md`, ADR 0002, and the upstream-Linux port design spec must agree that Boot To Virtio Probe is Milestone 3 and Virtio Root Disks moves to Milestone 4.
