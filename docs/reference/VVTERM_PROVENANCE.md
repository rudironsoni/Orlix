# VVTerm Import Provenance

## Scope

This document records the immutable source and native artifact inputs for the VVTerm-derived Orlix application. It is an engineering provenance record, not a legal approval. Public distribution remains blocked on the legal and App Store gates in ADR 0024.

## Source import

- Upstream: `https://github.com/vivy-company/vvterm.git`
- Pinned upstream commit: `791eebae946b0831ffff3ac839e0f2b75d076458`
- Retrieval date: 2026-07-13
- Original subtree import path: `Orlix/VVTerm`
- Current Orlix fork path: `Orlix/App`
- Import method: non-squashed Git subtree
- Orlix subtree merge commit: `63bcb1230fa739ac6fbc34d873b349ffee566453`
- Subtree trailer: `git-subtree-split: 791eebae946b0831ffff3ac839e0f2b75d076458`

The import command was:

```sh
git remote add vvterm-upstream https://github.com/vivy-company/vvterm.git
git fetch vvterm-upstream 791eebae946b0831ffff3ac839e0f2b75d076458
git subtree add --prefix=Orlix/VVTerm vvterm-upstream 791eebae946b0831ffff3ac839e0f2b75d076458
```

The Orlix fork was then fully renamed and moved from `Orlix/VVTerm` to
`Orlix/App`. The final integration commit renews the subtree metadata at the
current path with `git-subtree-dir: Orlix/App` and the same immutable
`git-subtree-split` revision.

Updates must fetch an explicitly reviewed full commit and use a non-squashed subtree pull. A branch name alone is never a release input:

```sh
git fetch vvterm-upstream <full-reviewed-commit>
git subtree pull --prefix=Orlix/App vvterm-upstream <full-reviewed-commit>
```

After every update, compare the imported sources, resources, packages, entitlements, privacy manifests, extensions, unit tests, UI tests, and target settings against `project.yml`. The imported Xcode project is retained as an upstream baseline and provenance reference. `project.yml` remains the authoritative Orlix project definition.

## Swift package baseline

The pinned upstream `Package.resolved` records:

| Package | Version or upstream declaration | Immutable revision |
| --- | --- | --- |
| `mlx-swift` | 0.29.1 | `072b684acaae80b6a463abab3a103732f33774bf` |
| `swift-cloudflared` | 0.1.2 | `1be78afe5dae7a20ce0837ce34085f03a77f7587` |
| `swift-mosh` | 0.1.6 | `bb4eacdf65303b2ecce624a91e98298c4ee94fca` |
| `swift-numerics` | 1.1.1 | `0c0290ff6b24942dadb83a929ffaaa1481df04a2` |
| `swift-umami` | mutable `main` declaration in upstream | `e7a14c16d745ec1d7e4355f35407f8407808475a` in the baseline lockfile |
| `tweetnacl-swiftwrap` | 1.1.0 | `f8fd111642bf2336b11ef9ea828510693106e954` |
| `ZIPFoundation` | 0.9.9 | `edbeaa39b426e54702194b0a601342322f01e400` |

Orlix release inputs must use immutable revisions in `project.yml`. SwiftUmami is absent from both the authoritative Orlix production graph and the renamed standalone fork project. The Orlix analytics adapter preserves the imported typed product events and properties through `OrlixTelemetry`; it performs no Umami networking. The pinned upstream dependency remains recorded here only so the pristine source graph remains auditable.

## Native source versions and rebuild entry points

- Ghostty fork repository: `https://github.com/wiedymi/ghostty.git`
- Ghostty source commit: `268a0a9d761fb19673f05d28042488e2002300f2`
- OpenSSL: 3.2.0
- libssh2: 1.11.0

The imported rebuild entry points are:

```sh
cd Orlix/App
./scripts/build.sh ghostty
./scripts/build.sh ssh
```

The Ghostty script must default to the full pinned commit above, not the upstream mutable `custom-io` branch. OpenSSL and libssh2 versions remain pinned in the script. A rebuilt artifact may replace a committed archive only after its source inputs, command, toolchain, target SDK, architectures, and resulting hashes are recorded.

## Committed native artifact hashes

Hashes use SHA-256 and were recomputed after the Orlix fork identity correction. The six Ghostty archives received a length-preserving replacement of the stale embedded `app.vivy.VivyTerm` value with `com.rudi.OrlixApp`. The pinned source rebuild script now patches Ghostty to the full `com.rudironsoni.Orlix` identifier, but a clean Ghostty source rebuild remains blocked on installing the Zig compiler. The nine OpenSSL and libssh2 archives were rebuilt from pinned OpenSSL 3.2.0 and libssh2 1.11.0 sources in the Orlix workspace so they no longer embed the upstream developer's absolute `VivyTerm` build paths. The resulting archives passed format checks and the final application linked successfully.

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
- Upstream App Store terms: `Orlix/App/UPSTREAM-APPSTORE-BINARY-LICENSE.md`. Orlix does not assume these terms grant Orlix distribution rights.
- Imported dependency notices: `Orlix/App/THIRD_PARTY_NOTICES.md`.

Before public distribution, the capability and provenance gate must verify that source offers, modification notices, copyright notices, dependency licenses, App Store terms, export classification, and every statically linked dependency have written legal approval. Missing approval blocks the release.
