# Orlix

Orlix is an iOS-hosted upstream Linux port. The product goal is to boot and package upstream Linux as `OrlixKernel.xcframework`, using Linux-native extension points instead of rewriting Linux core subsystems locally.

Think of the app as the host container. It does not become Linux and it does not manage Linux through a custom runtime API. It starts a bootloader, the bootloader prepares Linux-shaped boot inputs, and Linux owns Linux after boot.

## ELI5 Start

Orlix has three important source areas:

- `Linux/upstream/linux-6.12` is generated upstream Linux source. Treat it as read-only input.
- `Linux/ports/orlix` is where durable Orlix Linux port inputs live.
- `OrlixHostAdapter` is where private iOS and Darwin mechanics live.

The old local kernel implementation under `OrlixKernel/fs`, `OrlixKernel/kernel`, and `OrlixKernel/runtime` is not the target architecture. Do not add new Linux subsystem behavior there. Useful behavior should be migrated by ownership into upstream Linux-native paths, then those directories should disappear.

## Milestone 1 Proof Commands

Bootstrapping upstream Linux is the first source step:

```bash
make bootstrap-linux-upstream
```

Milestone 1 build work must make this the generated port-tree step:

```bash
make prepare-orlixkernel-port PROFILE=appstore
```

Milestone 1 build work must make these the first honest Linux proof targets:

```bash
make build-linux-kernel PROFILE=appstore
make build-linux-kernel PROFILE=development
```

`PROFILE=appstore` is the default profile. Pass another profile only when you intentionally need it.

Milestone 2 boot-entrypoint proof is intentionally narrower than booting Linux. It verifies that profile DTS sources are materialized into the generated Linux port tree.

```bash
make prepare-orlixkernel-port PROFILE=appstore
```

Milestone 2 does not prove QEMU execution, iOS execution, task switching, MMU behavior, userspace access, device binding, or root filesystem assembly.

Milestone 3 boot-to-virtio-probe proof keeps the dependency chain honest. It verifies that generated profile DTS files describe virtio-mmio probe-shape devices, that the selected generated profile defconfig enables upstream OF, virtio-mmio, and virtio-block probe paths, and that Orlix architecture boot handoff state is covered by a KUnit suite.

Build Orlix kselftest artifacts with Linux's kselftest build shape:

```bash
make prepare-orlixkernel-port PROFILE=appstore
make -C Build/OrlixKernel/linux-6.12-port/tools/testing/selftests TARGETS=orlix
```

Build Orlix KUnit artifacts with Linux Kbuild and the Orlix KUnit config:

```bash
cd Build/OrlixKernel/linux-6.12-port
make O=../kunit-orlix ARCH=orlix defconfig
scripts/kconfig/merge_config.sh -m -O ../kunit-orlix ../kunit-orlix/.config arch/orlix/.kunitconfig
make O=../kunit-orlix ARCH=orlix olddefconfig arch/orlix/boot/boot_test.o
```

Do not run kselftest or KUnit on Darwin and do not use a VM as product proof. Test execution belongs inside the Orlix Linux instance once the iOS-hosted boot path can run Linux userspace and kernel test output.

Milestone 3 does not prove `/dev/vda`, `/dev/vdb`, virtio-block request I/O, host-backed disk persistence, initramfs loading, OverlayFS root assembly, or userspace boot.

## Generated Trees

The pristine upstream source is generated at:

```text
Linux/upstream/linux-6.12
```

The disposable upstream-plus-Orlix port tree is generated at:

```text
Build/OrlixKernel/linux-6.12-port
```

If a change should survive regeneration, put it in `Linux/ports/orlix`, not in a generated tree.

## Port Inputs

Durable Orlix Linux port inputs live under:

```text
Linux/ports/orlix/
  overlay/
  patches/
  configs/
```

`overlay` contains files copied into Linux-native paths such as `arch/orlix` and `drivers/orlix`.

`patches` contains minimal upstream-tree deltas that cannot be represented as overlay files.

`configs` contains product profile defconfigs such as `appstore_defconfig` and `development_defconfig`.

## Product Surface

The public product surface is bootloader-shaped. It should expose a minimal boot entrypoint such as `OrlixBoot` with an app-level boot config. It must not expose syscall, file, mount, exec, task, cgroup, or runtime management APIs.

## Current Milestone

The current milestone is Kbuild `vmlinux` proof for `ARCH=orlix`.

Success means Kbuild builds a real upstream Linux target for the selected Orlix profile. Boot-stub XCFramework packaging is not product proof and must not masquerade as `OrlixKernel.xcframework` success.

## Device Direction

Orlix is virtio-first where Linux already has upstream device classes.

Use upstream Linux behavior for Linux-visible devices:

- `virtio_blk` for root disks
- `virtio_console` for the main console path
- `virtio-rng` for entropy
- `virtio_net` for networking
- virtio-fs first, or 9p over virtio if needed, for external directory mounts

Orlix-specific code supplies transport and backend mechanics under `drivers/orlix`, shaped as close to Linux virtio conventions as possible.

## Read Deeper

The canonical architecture specification is:

```text
docs/UPSTREAM_LINUX_IOS_PORT_SPEC.md
```

Architecture decisions are recorded under:

```text
docs/adr/
```

Glossary terms resolved during design live in:

```text
CONTEXT.md
```
