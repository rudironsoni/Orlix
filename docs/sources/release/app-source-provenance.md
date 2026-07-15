---
type: source
tags:
  - provenance
updated: 2026-07-15
status: current
summary: "Canonical repository source for app source provenance."
---

# App source provenance

## Scope

This document records the immutable source and native artifact inputs for the Orlix application. It is an engineering provenance record, not a legal approval. Public distribution remains blocked on the legal and App Store gates in ADR 0024.

## Source import

- Upstream: `https://github.com/vivy-company/vvterm.git`
- Pinned upstream commit: `791eebae946b0831ffff3ac839e0f2b75d076458`
- Retrieval date: 2026-07-13
- Current Orlix fork path: `Orlix/App`
- Import method: single-parent source snapshot
- Upstream tree: `ad7e13ae260293aa5aa2fcce6bcde240617eb383`
- Orlix snapshot commit: `a73e449406b757cba16aef9df20e65ae13733e9d`

The source snapshot was imported at `Orlix/VVTerm` and then fully renamed to `Orlix/App`. The snapshot tree exactly matches the pinned upstream tree, but the upstream repository commit ancestry is not part of Orlix history.

Updates must fetch an explicitly reviewed full commit, check it out in a temporary directory, replace the imported source files, and commit the result as a normal single-parent Orlix snapshot. A branch name alone is never a release input:

```sh
git clone --no-checkout https://github.com/vivy-company/vvterm.git /tmp/orlix-app-source
git -C /tmp/orlix-app-source fetch origin <full-reviewed-commit>
git -C /tmp/orlix-app-source checkout --detach <full-reviewed-commit>
```

Copy the reviewed working tree into `Orlix/App` without its `.git` directory, then compare the updated fork sources, resources, packages, entitlements, privacy manifests, extensions, unit tests, UI tests, and target settings against `project.yml`. The upstream Xcode project is retained only as a baseline provenance reference. `project.yml` remains the authoritative Orlix project definition.

## Swift package baseline

The pinned baseline `Package.resolved` records:

| Package | Version or baseline declaration | Immutable revision |
| --- | --- | --- |
| `mlx-swift` | 0.29.1, latest compatible with iOS 16.1 | `072b684acaae80b6a463abab3a103732f33774bf` |
| `swift-cloudflared` | 0.1.2 | `1be78afe5dae7a20ce0837ce34085f03a77f7587` |
| `swift-mosh` | 0.1.6 | `bb4eacdf65303b2ecce624a91e98298c4ee94fca` |
| `swift-numerics` | 1.1.1 | `0c0290ff6b24942dadb83a929ffaaa1481df04a2` |
| `swift-umami` | mutable `main` declaration in the source baseline | `e7a14c16d745ec1d7e4355f35407f8407808475a` in the baseline lockfile |
| `tweetnacl-swiftwrap` | current upstream `master` | `a7776eb5388467ec553b855846e24438288e2da5` |
| `ZIPFoundation` | 0.9.20 | `22787ffb59de99e5dc1fbfe80b19c97a904ad48d` |

Orlix release inputs must use immutable revisions in `project.yml`. SwiftUmami is absent from both the authoritative Orlix production graph and the renamed standalone project. The Orlix analytics adapter preserves the imported typed product events and properties through `OrlixTelemetry`; it performs no Umami networking. The pinned baseline dependency remains recorded here only so the pristine source graph remains auditable.

## Authoritative release inputs

`docs/sources/release/orlix-app-release-inputs.json` is the machine-readable release-input record for the Orlix application. It records the imported source commit, every direct Swift package URL and full revision, pinned native source versions and archive hashes, committed native artifact hashes, and required engineering evidence files.

`make app-release-inputs-check` compares that record against the resolved XcodeGen package graph, the vendor build script, the Ghostty version marker, the committed archives, and the required evidence paths. `beta-prerequisites` runs the same check. Version-only or branch-only direct package inputs, changed native artifacts, changed source hashes, or missing evidence fail before beta build work starts.

This check proves engineering release-input integrity only. It does not approve public distribution or complete the separate capability, entitlement, provisioning, CloudKit production-schema, export-classification, privacy, or legal gates.

## Native source versions and rebuild entry points

- Ghostty fork repository: `https://github.com/wiedymi/ghostty.git`
- Ghostty source commit: `268a0a9d761fb19673f05d28042488e2002300f2`
- OpenSSL: 3.2.0
- OpenSSL source archive SHA-256: `14c826f07c7e433706fb5c69fa9e25dab95684844b4c962a2cf1bf183eb4690e`
- libssh2: 1.11.0
- libssh2 source archive SHA-256: `3736161e41e2693324deb38c26cfdc3efe6209d634ba4258db1cecff6a5ad461`

The imported rebuild entry points are:

```sh
cd Orlix/App
./scripts/build.sh ghostty
./scripts/build.sh ssh
```

The Ghostty script must default to the full pinned commit above, not the mutable `custom-io` branch. OpenSSL and libssh2 versions remain pinned in the script. A rebuilt artifact may replace a committed archive only after its source inputs, command, toolchain, target SDK, architectures, and resulting hashes are recorded.

## Committed native artifact hashes

Hashes use SHA-256 and were recomputed after the Orlix identity correction. The six Ghostty archives received a length-preserving replacement of a stale embedded source-product identifier with `com.rudi.OrlixApp`. The pinned source rebuild script now patches Ghostty to the full `com.rudironsoni.Orlix` identifier, but a clean Ghostty source rebuild remains blocked on installing the Zig compiler. The nine OpenSSL and libssh2 archives were rebuilt from pinned OpenSSL 3.2.0 and libssh2 1.11.0 sources in the Orlix workspace so they no longer embed foreign developer workspace paths. The resulting archives passed format checks and the final application linked successfully.

| Artifact | SHA-256 |
| --- | --- |
| `Vendor/libghostty/GhosttyKit.xcframework/ios-arm64-simulator/libghostty-fat.a` | `988be2b71cd39268d6bd37ad837c4eefe7d303e628c97b2e1904c82a8e9f3434` |
| `Vendor/libghostty/GhosttyKit.xcframework/ios-arm64/libghostty-fat.a` | `be381b87be062209f3df3436ab6323a2ccaab90b4d7085285cb247b02d3c011e` |
| `Vendor/libghostty/GhosttyKit.xcframework/macos-arm64_x86_64/libghostty.a` | `24657ed0d641468e33adb4e94f57482a1c888373eb95530095e60f302aa9d961` |
| `Vendor/libghostty/ios-simulator/lib/libghostty.a` | `a6f574a9ae82841c5352040a250a088ea709666872b1632f7e4365ffd0932d4e` |
| `Vendor/libghostty/ios/lib/libghostty.a` | `174ba3a25d226c4e8d389be1414d525d2329beb97662efc45b4937f71ab9b9d4` |
| `Vendor/libghostty/lib/libghostty.a` | `4d7e2fde81f3b2a65510eebb7f4b5283cb6bed4fa640887d2d86aeca20b1d5ba` |
| `Vendor/libssh2/ios-simulator/lib/libcrypto.a` | `dfeea8d36da6f355a7c6fde456f4fddecc3bf0a744f54d72d56ee35283493b44` |
| `Vendor/libssh2/ios-simulator/lib/libssh2.a` | `903fb853fd3a89f237e38e48cab0487ae3470c7dcbaa655c008988ab15888eab` |
| `Vendor/libssh2/ios-simulator/lib/libssl.a` | `c674f7ab79f41e4d4cc37b5a1c24ec9ee7532b07093fe78f9bd2189b21c530b1` |
| `Vendor/libssh2/ios/lib/libcrypto.a` | `36c18281e9bd8a38a5f4398c1c928fd8b021622239beac3e01786819b86803f5` |
| `Vendor/libssh2/ios/lib/libssh2.a` | `77e1817bd3a5e30cb71ea7779e368df77e4e26cd8fed050edd14676b342f7cf1` |
| `Vendor/libssh2/ios/lib/libssl.a` | `d4772f6bb8c3e271f255b6ac16fbe72a9070537e28fa778b10adca6dc0d0fe70` |
| `Vendor/libssh2/macos/lib/libcrypto.a` | `5cf352407c36053b23b33c03c3873481c1201fee54b912cf3465388a7aa1107c` |
| `Vendor/libssh2/macos/lib/libssh2.a` | `ada64acbb596b5d22ae8a7d58c5b00499004dad8f636ea0e57770b688b463de7` |
| `Vendor/libssh2/macos/lib/libssl.a` | `783e6581ccd9cf57757b1cf011460688b759b2df20875ff01a6ff2186dcbc0f0` |

## Licenses and notices

- Imported application license: `Orlix/App/LICENSE`, GPL-3.0.
- Imported dependency notices: `Orlix/App/THIRD_PARTY_NOTICES.md`.

Before public distribution, the capability and provenance gate must verify that source offers, modification notices, copyright notices, dependency licenses, App Store terms, export classification, and every statically linked dependency have written legal approval. Missing approval blocks the release.
