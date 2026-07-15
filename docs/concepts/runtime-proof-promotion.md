---
type: concept
tags:
  - architecture
  - guidance
updated: 2026-07-15
summary: "Runtime claims advance in order from kernel dependency proof through kselftest, libc, syscall and UAPI proof, a POSIX shell environment, then jq, curl, and zsh."
applies:
  - "[Orlix](../objects/product/orlix.md)"
---

# Runtime proof promotion

Runtime claims advance in order from kernel dependency proof through kselftest, libc, syscall and UAPI proof, a POSIX shell environment, then jq, curl, and zsh. Earlier evidence cannot stand in for later product runtime proof.
