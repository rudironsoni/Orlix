---
type: source
tags:
  - provenance
updated: 2026-08-29
status: current
summary: "Canonical repository source for app source provenance."
---

# App source provenance

## Scope

This document records the immutable source and native artifact inputs for the Orlix application. It is an engineering provenance record, not a legal approval. Public distribution remains blocked on the legal and App Store gates in ADR 0024.

## Source import

- Upstream: `https://github.com/vivy-company/vvterm.git`
- Pinned upstream commit: `31120756133d22e526d01c630c680bd949dda730`
- Pinned upstream tree: `f0e529842c8f967a5f19748a63399d276215092e`
- Retrieval date: 2026-08-29
- Current Orlix fork path: `Orlix`
- Import method: three-way vendored source snapshot

The source snapshot is tracked as normal Orlix files. The upstream repository history is not part of Orlix history. The release record stores the exact upstream commit and tree instead.

`make vvterm-sync VVTERM_COMMIT=<full-reviewed-commit>` fetches the old and new snapshots into temporary repositories, applies the versioned branding policy to both, and performs a three-way merge with the tracked Orlix source. Reviewed conflict decisions live under `Orlix/make/vvterm-resolutions`. `make vvterm-sync-complete VVTERM_COMMIT=<full-reviewed-commit>` records the new immutable source inputs. A branch name alone is never a release input.

```sh
git clone --no-checkout https://github.com/vivy-company/vvterm.git /tmp/orlix-app-source
git -C /tmp/orlix-app-source fetch origin <full-reviewed-commit>
git -C /tmp/orlix-app-source checkout --detach <full-reviewed-commit>
```

The sync keeps legal attribution unchanged, keeps Orlix product identity as a versioned overlay, and keeps OrlixOS local-terminal integration as an explicit manual overlay. The upstream Xcode project is retained only as a baseline provenance reference. `project.yml` remains the authoritative Orlix project definition.

## Swift package baseline

The pinned baseline `Package.resolved` records:

| Package | Version or baseline declaration | Immutable revision |
| --- | --- | --- |
| `mlx-swift` | 0.29.1, latest compatible with iOS 16.1 | `072b684acaae80b6a463abab3a103732f33774bf` |
| `swift-cloudflared` | 0.1.2 | `1be78afe5dae7a20ce0837ce34085f03a77f7587` |
| `swift-et` | 0.1.5 | `2e43ccd70cd74cd46e18a92d20a0ea7547ba42ed` |
| `swift-mosh` | 0.1.8 | `9677768702727ba9094d4936062a452b9c544480` |
| `swift-numerics` | 1.1.1 | `0c0290ff6b24942dadb83a929ffaaa1481df04a2` |
| `tweetnacl-swiftwrap` | 1.1.0 | `f8fd111642bf2336b11ef9ea828510693106e954` |
| `ZIPFoundation` | 0.9.9 | `edbeaa39b426e54702194b0a601342322f01e400` |

Orlix release inputs use immutable revisions in `project.yml`. SwiftUmami is absent from the Orlix graph. The Orlix analytics adapter preserves the imported typed product events and properties through `OrlixTelemetry`.

## Authoritative release inputs

`docs/sources/release/orlix-app-release-inputs.json` is the machine-readable release-input record for the Orlix application. It records the imported source commit, every direct Swift package URL and full revision, pinned native source versions and archive hashes, committed native artifact hashes, and required engineering evidence files.

`make app-release-inputs-check` compares that record against the resolved XcodeGen package graph, the vendor Make rules, the Ghostty version marker, the committed archives, and the required evidence paths. `beta-prerequisites` runs the same check. Version-only or branch-only direct package inputs, changed native artifacts, changed source hashes, or missing evidence fail before beta build work starts.

This check proves engineering release-input integrity only. It does not approve public distribution or complete the separate capability, entitlement, provisioning, CloudKit production-schema, export-classification, privacy, or legal gates.

## Native source versions and rebuild entry points

- Ghostty fork repository: `https://github.com/wiedymi/ghostty.git`
- Ghostty source commit: `02af5158c76036291183e746d436eb8f15356662`
- OpenSSL: 3.2.0
- OpenSSL source archive SHA-256: `14c826f07c7e433706fb5c69fa9e25dab95684844b4c962a2cf1bf183eb4690e`
- libssh2: 1.11.1
- libssh2 source archive SHA-256: `d9ec76cbe34db98eec3539fe2c899d26b0c837cb3eb466a56b0f109cabf658f7`

The imported rebuild entry points are:

```console
make build type=vendor vendor=ghostty
make build type=vendor vendor=ssh
```

The Ghostty Make rule must default to the full pinned commit above, not the mutable `custom-io` branch. OpenSSL and libssh2 versions remain pinned in the owning Make rules. A rebuilt artifact may replace a committed archive only after its source inputs, command, toolchain, target SDK, architectures, and resulting hashes are recorded.

## Committed native artifact hashes

Hashes use SHA-256. The compatibility archives are byte copies of the exact current `GhosttyKit.xcframework` slices. A clean Ghostty rebuild requires Zig 0.16. The nine OpenSSL and libssh2 archives come from the reviewed vvterm source snapshot.

| Artifact | SHA-256 |
| --- | --- |
| `Vendor/libghostty/GhosttyKit.xcframework/ios-arm64-simulator/libghostty-internal.a` | `cbd282ed129307d339d5e683ba2991488b41c5b1101fada605717383487913de` |
| `Vendor/libghostty/GhosttyKit.xcframework/ios-arm64/libghostty-internal.a` | `55868b8e16e3c353f68781dc735eb96846d80e727ced0b7923e746684d475872` |
| `Vendor/libghostty/GhosttyKit.xcframework/macos-arm64_x86_64/ghostty-internal.a` | `9e99fa3d59f5880e74d75b30981aea3d4c08304fcb4e3044f6a85fb0cbbcad0c` |
| `Vendor/libghostty/ios-simulator/lib/libghostty.a` | `cbd282ed129307d339d5e683ba2991488b41c5b1101fada605717383487913de` |
| `Vendor/libghostty/ios/lib/libghostty.a` | `55868b8e16e3c353f68781dc735eb96846d80e727ced0b7923e746684d475872` |
| `Vendor/libghostty/lib/libghostty.a` | `9e99fa3d59f5880e74d75b30981aea3d4c08304fcb4e3044f6a85fb0cbbcad0c` |
| `Vendor/libssh2/ios-simulator/lib/libcrypto.a` | `c618efac6f2ac9be1835674d77a43bde6b68eb2260c0c9ee2271bcfc1cf407f3` |
| `Vendor/libssh2/ios-simulator/lib/libssh2.a` | `808a31d31ffc68a24a01eb4e46818016e4f7a04d76986a1a6711c1c818eda3c5` |
| `Vendor/libssh2/ios-simulator/lib/libssl.a` | `8c63c4680bb489f96c641fc9ed88521349199007e222a13f9970cd381800c64e` |
| `Vendor/libssh2/ios/lib/libcrypto.a` | `3c2e4ab5c97be967260ddcd59ff937f4bbf1cc46fae213cf752b26881a7ba69a` |
| `Vendor/libssh2/ios/lib/libssh2.a` | `bc6f1296c3259c450f2c0474b6ca53da8d75e4f9b77de1d929826b59a08490cc` |
| `Vendor/libssh2/ios/lib/libssl.a` | `770a4621bb427e9611a9aff86d3253be405b0236852dcaf7780dd8add3d98b3a` |
| `Vendor/libssh2/macos/lib/libcrypto.a` | `fc13163d3dfce34f74ff06feb057a9eae546c729aa3ce4c1e87cc8121fdbde03` |
| `Vendor/libssh2/macos/lib/libssh2.a` | `18e3a6157d62389311d713cdfb5b5d6c36fe4f43a9f3ede9296df2f55e4b515d` |
| `Vendor/libssh2/macos/lib/libssl.a` | `db8e9f2ce6f9aa1c1a5ae9a768048170784acc9e720f2c0b3e600896d5f3ac8d` |

## Licenses and notices

- Imported application license: `Orlix/LICENSE`, GPL-3.0.
- Imported dependency notices: `Orlix/THIRD_PARTY_NOTICES.md`.

Before public distribution, the capability and provenance gate must verify that source offers, modification notices, copyright notices, dependency licenses, App Store terms, export classification, and every statically linked dependency have written legal approval. Missing approval blocks the release.
