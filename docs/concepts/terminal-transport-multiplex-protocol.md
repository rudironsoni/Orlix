---
type: concept
tags:
  - architecture
  - terminal
  - protocol
updated: 2026-07-15
summary: "Orlix terminal input and resize control share one ordered, versioned, binary-safe SLIP-framed stream."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Terminal transport multiplex protocol

Orlix terminal input and terminal control use one total ordered byte stream between the app-facing OrlixOS transport and first-stage Linux userspace. Terminal geometry never travels through the kernel command line. The authoritative initial grid is the first resize event, and `/init` applies it with `TIOCSWINSZ` before starting the interactive process. Later resizes use the same event and dispatcher.

Each message is an RFC 1055 SLIP frame delimited by `0xc0`. Literal `0xc0` is encoded as `0xdb 0xdc`, and literal `0xdb` is encoded as `0xdb 0xdd`, so every terminal byte sequence is representable. Consecutive delimiters are empty synchronization frames and have no semantic event.

The unescaped frame is a fixed eight-byte header followed by its payload:

| Field | Type | Byte order | Rule |
| --- | --- | --- | --- |
| Version | `u8` | N/A | Version 1 is supported. Other versions emit a protocol error. |
| Type | `u8` | N/A | `1` is terminal data, `2` is resize, and other values emit an unsupported event. |
| Flags | `u16` | big-endian | Version 1 requires zero. |
| Payload length | `u32` | big-endian | At most 4096 bytes and exactly equal to the decoded payload. |
| Payload | bytes | type-specific | Data is arbitrary binary. Resize is exactly two big-endian `u16` values, rows then columns. |

Rows and columns range from 1 through 65535. Zero is invalid. A data payload may be empty. Frames are delivered synchronously in decode order, so `data("abc"), resize(40, 120), data("def")` reaches the dispatcher in that exact order. Duplicate resize messages remain ordered events.

The decoder retains at most 4104 decoded bytes and never allocates from a declared length. Partial reads retain bounded state. Partial writes are retried by the encoder-side transport until the HostAdapter input queue stops accepting bytes. Invalid escapes, invalid headers, invalid lengths, invalid flags, invalid geometry, and oversized frames emit structured protocol-error events and counters. Unsupported types emit a typed unsupported event. Neither condition writes diagnostic text or control bytes into the PTY.

Malformed or oversized frames are discarded through the next SLIP delimiter, which restores synchronization deterministically. Finishing an incomplete stream emits a protocol error and resets all retained state without forwarding buffered bytes. The protocol has no integrity field, so integrity-field validation does not apply.

The encoder is owned by `OrlixOS/Sources/Session/OrlixOS.swift`. The reusable streaming decoder is owned by `OrlixOS/Sources/init/terminal_mux.c`, and `/init` dispatches decoded data to the PTY and decoded resize events through the Linux TTY interface. HostAdapter preserves console source identity in both directions and transports bytes without interpreting this protocol or Linux console policy. The Linux virtio and serial console endpoints consume only their corresponding source queue.
