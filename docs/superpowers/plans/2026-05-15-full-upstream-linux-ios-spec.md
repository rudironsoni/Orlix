# Full Upstream Linux iOS Spec Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the full Orlix upstream-Linux iOS architecture specification as a sequence of auditable milestones.

**Architecture:** Keep upstream Linux as generated input, preserve Makefile-owned bootstrap/build/package flow, and implement behavior only through Linux-native extension points: `arch/orlix`, `drivers/orlix`, `boot/`, `OrlixHostAdapter`, Kconfig, configs, patches, and proof targets. Each milestone must end with an honest proof target and must not claim later runtime behavior before it exists.

**Tech Stack:** GNU Make, upstream Linux 6.12 Kbuild, Clang/LLVM, XcodeGen, Xcode static libraries/XCFrameworks, C, Objective-C for host adapter mechanics only.

---

## Milestones

### Task 1: Make `ARCH=orlix` Compile Beyond `defconfig`

**Files:**
- Modify: `Linux/ports/orlix/overlay/arch/orlix/Kconfig`
- Modify: `Linux/ports/orlix/overlay/arch/orlix/Makefile`
- Modify: `Linux/ports/orlix/overlay/arch/orlix/include/asm/Kbuild`
- Create/modify focused `Linux/ports/orlix/overlay/arch/orlix/include/asm/*.h` only when Linux Kbuild requires an architecture-owned header
- Modify: `Linux/ports/orlix/overlay/arch/orlix/kernel/setup.c`
- Modify: `Makefile`

**Purpose:** Move from `mrproper defconfig` proof to compiling the minimal upstream Linux object graph for `ARCH=orlix` far enough to expose real architecture obligations.

- [ ] Add a `build-linux-orlix-kernel-simulator` Makefile target that uses `ARCH=orlix LLVM=1` and attempts `vmlinux` or the nearest honest Kbuild object target if `vmlinux` is not yet possible.
- [ ] Run `make build-linux-orlix-kernel-simulator` and capture the first missing architecture contract.
- [ ] Add the smallest Linux-shaped `arch/orlix` header or source implementation required by that contract.
- [ ] Repeat until the target reaches a stable, documented stop point without local Linux-core rewrites.
- [ ] Update `docs/UPSTREAM_LINUX_IOS_PORT_SPEC.md` only if the proof boundary changes.
- [ ] Verify: `make lint`, `make build-linux-orlix-kernel-simulator`, `make package-orlixkernel-xcframework`.
- [ ] Commit: `linux: make arch/orlix compile minimal kernel target`.

### Task 2: Wire Bootloader Handoff Toward `arch/orlix`

**Files:**
- Modify: `OrlixKernel/include/OrlixKernel.h`
- Modify: `boot/loader.c`
- Modify: `boot/params.c`
- Modify: `boot/image.c`
- Modify: `boot/initrd.c`
- Modify: `boot/rootfs.c`
- Modify: `boot/dtb.c`
- Modify: `Linux/ports/orlix/overlay/arch/orlix/include/asm/boot.h`
- Modify: `Linux/ports/orlix/overlay/arch/orlix/boot/boot.c`

**Purpose:** Make the product boot API prepare a Linux-shaped boot contract and transfer to the architecture boot entry without adding runtime facade APIs.

- [ ] Align `struct boot_params` with the spec fields: `cmdline`, memory base/size, initrd base/size, dtb base/size, root device, console device, and flags.
- [ ] Keep exported symbols limited to `OrlixBoot` and strictly bootloader support helpers already present.
- [ ] Add validation for required boot fields with deterministic `-1` failures for invalid public inputs.
- [ ] Add a direct internal call path from `OrlixBoot` to an architecture boot handoff function compiled into the product skeleton.
- [ ] Verify no forbidden runtime product APIs exist with a search for `OrlixKernelSyscall|OrlixKernelOpen|OrlixKernelRead|OrlixKernelMount|OrlixKernelExecve`.
- [ ] Verify: `make lint`, `xcodegen generate --project .`, simulator Xcode build, `make package-orlixkernel-xcframework`.
- [ ] Commit: `boot: wire OrlixBoot to architecture handoff`.

### Task 3: Establish Boot Parameter and Device Description Contract

**Files:**
- Modify: `boot/dtb.c`
- Modify: `boot/params.c`
- Modify: `Linux/ports/orlix/overlay/arch/orlix/include/asm/boot.h`
- Create: `Linux/ports/orlix/overlay/arch/orlix/include/asm/setup.h`
- Modify: `docs/UPSTREAM_LINUX_IOS_PORT_SPEC.md`

**Purpose:** Define the first durable machine description contract for `/chosen`, memory, Orlix profile, root block device, and console device.

- [ ] Define a stable in-memory boot description format that can later map to device tree without exposing host paths.
- [ ] Ensure root and console identities are Linux device names, not raw iOS paths.
- [ ] Teach `boot/dtb.c` to populate the description from `struct boot_params`.
- [ ] Teach `arch/orlix/kernel/setup.c` to consume the description enough to log command line, memory, root, console, and profile state.
- [ ] Verify: `make lint`, `make build-linux-orlix-kernel-simulator`, simulator Xcode build.
- [ ] Commit: `boot: define Orlix Linux boot description`.

### Task 4: Add Core `arch/orlix` Execution Substrate Skeletons

**Files:**
- Create: `Linux/ports/orlix/overlay/arch/orlix/kernel/process.c`
- Create: `Linux/ports/orlix/overlay/arch/orlix/kernel/signal.c`
- Create: `Linux/ports/orlix/overlay/arch/orlix/kernel/syscall.c`
- Create: `Linux/ports/orlix/overlay/arch/orlix/kernel/time.c`
- Create: `Linux/ports/orlix/overlay/arch/orlix/kernel/memory.c`
- Create: `Linux/ports/orlix/overlay/arch/orlix/kernel/uaccess.c`
- Create: `Linux/ports/orlix/overlay/arch/orlix/kernel/irq.c`
- Create: `Linux/ports/orlix/overlay/arch/orlix/kernel/traps.c`
- Create: `Linux/ports/orlix/overlay/arch/orlix/kernel/lifecycle.c`
- Create matching required `include/asm/*.h` files only when demanded by Kbuild.

**Purpose:** Provide the architecture-owned hooks Linux expects for task context, syscall entry, signal frames, user access, timer/IRQ, traps, memory permission, and lifecycle integration.

- [ ] Add each source file to `arch/orlix/kernel/Makefile` only when it satisfies an actual Kbuild obligation.
- [ ] Keep each implementation as a Linux-shaped architecture hook, not a Darwin wrapper.
- [ ] Return Linux-shaped unsupported errors where a hook is callable but not complete.
- [ ] Avoid local implementations of VFS, fd tables, wait, cgroups, namespaces, procfs, sysfs, sockets, or task lists.
- [ ] Verify: `make lint`, `make build-linux-orlix-kernel-simulator`.
- [ ] Commit: `linux: add arch/orlix execution substrate skeletons`.

### Task 5: Build `drivers/orlix` Block and Root Image Foundation

**Files:**
- Modify: `Linux/ports/orlix/overlay/drivers/orlix/Kconfig`
- Modify: `Linux/ports/orlix/overlay/drivers/orlix/Makefile`
- Modify: `Linux/ports/orlix/overlay/drivers/orlix/block/Kconfig`
- Modify: `Linux/ports/orlix/overlay/drivers/orlix/block/Makefile`
- Modify: `Linux/ports/orlix/overlay/drivers/orlix/block/file.c`
- Modify: `Linux/ports/orlix/overlay/drivers/orlix/block/image.c`
- Modify: `OrlixHostAdapter/fs/backing_io.m`
- Modify: `OrlixHostAdapter/fs/backing_paths.m`

**Purpose:** Expose app-private root and writable image backing as Linux block devices while keeping host paths private.

- [ ] Replace module-style placeholders with minimal Linux block driver registration that compiles under Kbuild.
- [ ] Define the private host seam needed to resolve app-private image storage.
- [ ] Ensure Linux-visible device identity is a Linux block device, not an iOS path.
- [ ] Add a Kconfig option for root image support in `drivers/orlix/block`.
- [ ] Verify: `make lint`, `make build-linux-orlix-kernel-simulator`.
- [ ] Commit: `linux: add Orlix block image driver foundation`.

### Task 6: Build TTY, PTY, and Console Foundation

**Files:**
- Modify: `Linux/ports/orlix/overlay/drivers/orlix/tty/Kconfig`
- Modify: `Linux/ports/orlix/overlay/drivers/orlix/tty/Makefile`
- Modify: `Linux/ports/orlix/overlay/drivers/orlix/tty/console.c`
- Create: `Linux/ports/orlix/overlay/drivers/orlix/tty/pty.c`

**Purpose:** Connect Linux TTY/PTY semantics to the Orlix app terminal frontend without owning job-control semantics outside Linux.

- [ ] Register a minimal Linux console device through the Linux TTY/console APIs.
- [ ] Add a PTY driver foundation only through Linux TTY APIs.
- [ ] Ensure termios, window size, and job-control notes stay Linux-owned.
- [ ] Verify: `make lint`, `make build-linux-orlix-kernel-simulator`.
- [ ] Commit: `linux: add Orlix tty foundation`.

### Task 7: Build External/Documents Filesystem Driver Foundation

**Files:**
- Create: `Linux/ports/orlix/overlay/drivers/orlix/fs/Kconfig`
- Create: `Linux/ports/orlix/overlay/drivers/orlix/fs/Makefile`
- Create: `Linux/ports/orlix/overlay/drivers/orlix/fs/external.c`
- Create: `Linux/ports/orlix/overlay/drivers/orlix/fs/documents.c`
- Modify: `OrlixHostAdapter/fs/backing_dir.c`
- Modify: `OrlixHostAdapter/fs/backing_stat_translate.c`

**Purpose:** Represent external and Documents access as explicit Linux mounts with Linux-shaped errors.

- [ ] Add driver Kconfig and Makefile wiring under `drivers/orlix/fs`.
- [ ] Implement minimal mount registration stubs using Linux filesystem APIs.
- [ ] Model access states as mount state: active, inactive, stale, revoked, unavailable.
- [ ] Map host access failures to Linux errors without exposing URLs or host paths.
- [ ] Verify: `make lint`, `make build-linux-orlix-kernel-simulator`.
- [ ] Commit: `linux: add Orlix external filesystem foundation`.

### Task 8: Build Networking, Loopback, and Transport Foundation

**Files:**
- Create: `Linux/ports/orlix/overlay/drivers/orlix/net/Kconfig`
- Create: `Linux/ports/orlix/overlay/drivers/orlix/net/Makefile`
- Create: `Linux/ports/orlix/overlay/drivers/orlix/net/device.c`
- Create: `Linux/ports/orlix/overlay/drivers/orlix/net/loopback.c`
- Create: `Linux/ports/orlix/overlay/drivers/orlix/net/transport.c`

**Purpose:** Provide Orlix virtual network devices while making clear that host routes and interfaces are not controlled.

- [ ] Add Kconfig and Makefile wiring for `drivers/orlix/net`.
- [ ] Register a virtual Orlix network device through Linux networking APIs.
- [ ] Provide loopback foundation inside the Orlix network namespace.
- [ ] Add transport seam placeholders that compile but do not claim host route/interface mutation.
- [ ] Verify: `make lint`, `make build-linux-orlix-kernel-simulator`.
- [ ] Commit: `linux: add Orlix network driver foundation`.

### Task 9: Add Lifecycle and Random Character Device Foundation

**Files:**
- Modify: `Linux/ports/orlix/overlay/drivers/orlix/char/Kconfig`
- Modify: `Linux/ports/orlix/overlay/drivers/orlix/char/Makefile`
- Modify: `Linux/ports/orlix/overlay/drivers/orlix/char/random.c`
- Create: `Linux/ports/orlix/overlay/drivers/orlix/char/lifecycle.c`
- Modify: `OrlixHostAdapter/kernel/random.c`

**Purpose:** Integrate host entropy and app lifecycle as Linux-visible device/event sources.

- [ ] Replace random placeholder with a Linux random integration hook that compiles under Kbuild.
- [ ] Add lifecycle character device foundation for suspend/background/relaunch events.
- [ ] Keep host lifecycle vocabulary private to `OrlixHostAdapter` and expose Linux-shaped events.
- [ ] Verify: `make lint`, `make build-linux-orlix-kernel-simulator`.
- [ ] Commit: `linux: add Orlix lifecycle and random devices`.

### Task 10: Add Rootfs, Initrd, and Bundled Userspace Boot Proof

**Files:**
- Modify: `boot/rootfs.c`
- Modify: `boot/initrd.c`
- Modify: `boot/params.c`
- Modify: `Linux/ports/orlix/configs/appstore_defconfig`
- Modify: `Linux/ports/orlix/configs/development_defconfig`
- Modify: `Linux/ports/orlix/configs/enterprise_defconfig`
- Add generated/ignored proof fixture paths only under `Build/`

**Purpose:** Prove the boot configuration can describe a bundled root image and initrd without claiming full runtime userspace until Linux reaches the required boot point.

- [ ] Add Makefile proof target for a bundled-rootfs boot configuration artifact.
- [ ] Keep fixture artifacts generated under `Build/`, not committed.
- [ ] Ensure config profiles enable initrd/rootfs support needed by the proof.
- [ ] Verify: `make lint`, rootfs proof target, `make package-orlixkernel-xcframework`.
- [ ] Commit: `boot: add rootfs and initrd proof target`.

### Task 11: Add App Store Profile and Executable-Memory Policy Foundation

**Files:**
- Modify: `Linux/ports/orlix/configs/appstore_defconfig`
- Modify: `Linux/ports/orlix/configs/development_defconfig`
- Modify: `Linux/ports/orlix/configs/enterprise_defconfig`
- Modify: `Linux/ports/orlix/overlay/arch/orlix/kernel/memory.c`
- Modify: `Linux/ports/orlix/overlay/arch/orlix/mm/mmap.c`

**Purpose:** Encode JIT-less and executable-memory policy at the profile and architecture boundary.

- [ ] Add or update profile config comments and symbols for App Store executable-memory constraints.
- [ ] Add architecture-side policy hooks for executable mapping decisions where Linux expects architecture input.
- [ ] Return Linux-shaped errors for unsupported writable/executable mappings.
- [ ] Verify: `make lint`, `make build-linux-orlix-kernel-simulator`.
- [ ] Commit: `linux: add Orlix executable memory policy foundation`.

### Task 12: Add Linux Behavior Proof Suites and Final Packaging Proof

**Files:**
- Create subsystem-owned C proof files under `OrlixKernelTests/` only if the test target is restored to current build truth
- Modify: `Makefile`
- Modify: `project.yml`
- Modify: `docs/UPSTREAM_LINUX_IOS_PORT_SPEC.md`

**Purpose:** Turn milestone capabilities into authoritative proof targets while keeping host adapter tests separate from Linux behavior proof.

- [ ] Add explicit proof target names for each completed milestone.
- [ ] Restore or create only the test harness needed for Linux-facing proof; do not revive old local-rewrite tests wholesale.
- [ ] Keep `OrlixHostAdapter` tests scoped to host mechanics.
- [ ] Run final proof: `make lint`, all milestone proof targets, `xcodegen generate --project .`, simulator Xcode build, `make package-orlixkernel-xcframework`.
- [ ] Commit: `test: add full Orlix Linux proof targets`.

## Self-Review

This plan intentionally does not put all full-spec behavior into one commit. The spec spans independent kernel architecture, drivers, boot, storage, networking, lifecycle, package policy, App Store policy, and proof systems. Each milestone must land as a separately verified tranche. The plan avoids `Linux/scripts/*.sh`, preserves `OrlixKernel.xcodeproj`, keeps product API bootloader-shaped, and forbids Linux-core rewrites.
