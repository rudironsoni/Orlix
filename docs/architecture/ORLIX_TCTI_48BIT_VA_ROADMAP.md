# Orlix TCTI 48-Bit VA Roadmap

This note is future-design scope for ADR 0022. It is not part of the first TCTI implementation checkpoint.

## Goal

Orlix TCTI must not be designed in a way that blocks ordinary AArch64 Linux userspace with large virtual-address expectations.

The first gate is still smaller:

```text
static /init -> first svc #0 -> Linux syscall path -> one console line
```

## Required Compatibility Directions

- Keep guest virtual addresses Linux-owned. TCTI must translate through OrlixKernel mm metadata, not HostAdapter policy.
- Preserve 48-bit guest VA assumptions where Linux userspace expects them.
- Treat high mmap hints as Linux policy. TCTI can reject only through Linux fault/mmap behavior.
- Model `MAP_NORESERVE` through Linux-owned lazy reservation behavior, not eager host mappings.
- Keep low guard pages meaningful for Linux faults.
- Plan for large VA reservations used by V8, Go, Rust, JVM, and similar runtimes.
- Keep futex behavior in upstream Linux. TCTI only executes the instructions that reach Linux syscalls or atomics.
- Add `sigaltstack` and AArch64 signal `ucontext` support through Linux signal machinery.
- Add atomics/exclusives after first syscall/console gates and focused tests.
- Add NEON/SIMD and crypto only from trace-led workload evidence and tests.

## Non-Goals

- No Node, Python, Go, Rust, JVM, OCI, package-manager, or shell hacks in the first implementation checkpoint.
- No HostAdapter-owned Linux process or memory policy.
- No guest executable host mappings.
