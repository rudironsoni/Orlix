# VVTerm Import Provenance

## Scope

This document records the immutable source and native artifact inputs for the VVTerm-derived Orlix application. It is an engineering provenance record, not a legal approval. Public distribution remains blocked on the legal and App Store gates in ADR 0024.

## Source import

- Upstream: `https://github.com/vivy-company/vvterm.git`
- Pinned upstream commit: `791eebae946b0831ffff3ac839e0f2b75d076458`
- Retrieval date: 2026-07-13
- Imported path: `Orlix/VVTerm`
- Import method: non-squashed Git subtree
- Orlix subtree merge commit: `63bcb1230fa739ac6fbc34d873b349ffee566453`
- Subtree trailer: `git-subtree-split: 791eebae946b0831ffff3ac839e0f2b75d076458`

The import command was:

```sh
git remote add vvterm-upstream https://github.com/vivy-company/vvterm.git
git fetch vvterm-upstream 791eebae946b0831ffff3ac839e0f2b75d076458
git subtree add --prefix=Orlix/VVTerm vvterm-upstream 791eebae946b0831ffff3ac839e0f2b75d076458
```

Updates must fetch an explicitly reviewed full commit and use a non-squashed subtree pull. A branch name alone is never a release input:

```sh
git fetch vvterm-upstream <full-reviewed-commit>
git subtree pull --prefix=Orlix/VVTerm vvterm-upstream <full-reviewed-commit>
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

Orlix release inputs must use immutable revisions in `project.yml`. The upstream Umami transport and endpoint are not Orlix product dependencies. They are removed or neutralized during the identity adaptation. The pinned baseline is retained here so the pristine source graph remains auditable.

## Native source versions and rebuild entry points

- Ghostty fork repository: `https://github.com/wiedymi/ghostty.git`
- Ghostty source commit: `268a0a9d761fb19673f05d28042488e2002300f2`
- OpenSSL: 3.2.0
- libssh2: 1.11.0

The imported rebuild entry points are:

```sh
cd Orlix/VVTerm
./scripts/build.sh ghostty
./scripts/build.sh ssh
```

The Ghostty script must default to the full pinned commit above, not the upstream mutable `custom-io` branch. OpenSSL and libssh2 versions remain pinned in the script. A rebuilt artifact may replace a committed archive only after its source inputs, command, toolchain, target SDK, architectures, and resulting hashes are recorded.

## Committed native artifact hashes

Hashes use SHA-256 and were computed from subtree commit `63bcb1230fa739ac6fbc34d873b349ffee566453`.

| Artifact | SHA-256 |
| --- | --- |
| `Vendor/libghostty/GhosttyKit.xcframework/ios-arm64-simulator/libghostty-fat.a` | `910e49b35289f6da0fb6f87c805a45cd9609ff6fe7c261d9df782efcebf1f2ef` |
| `Vendor/libghostty/GhosttyKit.xcframework/ios-arm64/libghostty-fat.a` | `077fe2b18e8e42672429e1f770cfc17b1dc896489109eb709d0e3aead44495c5` |
| `Vendor/libghostty/GhosttyKit.xcframework/macos-arm64_x86_64/libghostty.a` | `9ddcdbb460a03c061231e156bb86707ac431cc611558d2728e14dae8dc5ed6cd` |
| `Vendor/libghostty/ios-simulator/lib/libghostty.a` | `744b4b3b09ccf65f11817c0f92580a9f3953394d2a2f778670f4751cbe428c6f` |
| `Vendor/libghostty/ios/lib/libghostty.a` | `b6ca359aa73668f38c1c6dd9c3ec3470a6ece86e0ccd866df00e0e3108c78e8f` |
| `Vendor/libghostty/lib/libghostty.a` | `17d459bf5d1b837b1fd4598f464a3ade4c1c3a1f8fdb8d75fe6d09f41e864692` |
| `Vendor/libssh2/ios-simulator/lib/libcrypto.a` | `f8b2fea8cde077af6e13d1984d212eb728c65a1d16b285fb915b7bef9fd49fd8` |
| `Vendor/libssh2/ios-simulator/lib/libssh2.a` | `c743b2b8ba531d6667149523cdb92e99f3a1733739719ef6735eb5fc1b40a0bc` |
| `Vendor/libssh2/ios-simulator/lib/libssl.a` | `e70cdfcca95ccd3251ac8775ccaa4e36af0947c1ad58dcb37f16248e84f3f45b` |
| `Vendor/libssh2/ios/lib/libcrypto.a` | `c798b2c7b634d9c5ba5d195281417e19a8bb320e8cd99286e45928cc03d86d50` |
| `Vendor/libssh2/ios/lib/libssh2.a` | `33dbb9a126a1e1d3c40e53ba0cbbe2119068c1db2b9b416aa08f05f81ba7df15` |
| `Vendor/libssh2/ios/lib/libssl.a` | `e019ce7c0193e6adf7a43cc15b2eefd6d6f3ed65ce7b96a0f46e2464e6d284a8` |
| `Vendor/libssh2/macos/lib/libcrypto.a` | `6c9f1eb37cbc2e8c1890d9d05ba917163c0ed66e3efa4cee189e707522a54935` |
| `Vendor/libssh2/macos/lib/libssh2.a` | `98b4b6b6cbac4bed2cc213c9580d798cda9008e90ada70fba26324da7c0e705a` |
| `Vendor/libssh2/macos/lib/libssl.a` | `8af58ebf85d375f7ecf95a7ecd708fbaa6db5f52d55f0861cde55a330c68e60f` |

## Licenses and notices

- Imported application license: `Orlix/VVTerm/LICENSE`, GPL-3.0.
- Upstream App Store terms: `Orlix/VVTerm/LICENSE-APPSTORE.md`. Orlix does not assume these terms grant Orlix distribution rights.
- Imported dependency notices: `Orlix/VVTerm/THIRD_PARTY_NOTICES.md`.

Before public distribution, the capability and provenance gate must verify that source offers, modification notices, copyright notices, dependency licenses, App Store terms, export classification, and every statically linked dependency have written legal approval. Missing approval blocks the release.
