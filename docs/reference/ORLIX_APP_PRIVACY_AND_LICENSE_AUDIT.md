# Orlix Application Privacy and License Audit

## Scope

This record covers the Swift package resolution and application privacy manifest used by the Orlix iOS and iPadOS application at build 34. It is engineering evidence, not legal approval, App Review approval, or an exported-package privacy report.

The authoritative package resolution is:

- Path: `Orlix.xcodeproj/project.xcworkspace/xcshareddata/swiftpm/Package.resolved`
- SHA-256: `fbfb181219b355d4500e5279a4cd75cb7345532cab65e141f9c9f1f02c190f2d`
- Resolved pins: 30

## Privacy manifest union

The audit inspected every `PrivacyInfo.xcprivacy` file in the 30 resolved package checkouts under the dynamically resolved Xcode package cache. Fourteen dependency manifests were present.

- Swift Crypto, SwiftNIO SSL, and Swift Protobuf manifests declared no tracking, collected data, tracking domains, or required-reason API use.
- Three SwiftNIO manifests declared `NSPrivacyAccessedAPICategoryFileTimestamp` with reason `0A2A.1`: `NIOFS`, `NIOPosix`, and `_NIOFileSystem`.
- The Orlix application already declared `NSPrivacyAccessedAPICategoryUserDefaults` with reason `CA92.1` and its telemetry data categories.
- `Orlix/App/Orlix/PrivacyInfo.xcprivacy` now contains the union of the non-empty application and resolved-dependency required-reason declarations: UserDefaults `CA92.1` and File Timestamp `0A2A.1`.

The three non-empty SwiftNIO manifests were byte-identical at SHA-256 `982f20d820bc4460acc9d4116c3e6c8ec837f717dc82edbb7308f7400515c909`. The resulting Orlix source manifest SHA-256 is `6d3ed45d047b7e3ed09da81372eb18e7360633e04682682b75bb95f8cb4ba6c6`.

The release gate compares the exported app privacy manifest structurally with this source manifest. A real exported app has not yet passed that gate.

## Package license audit

The same resolved-checkout audit found a root `LICENSE`, `LICENSE.txt`, or equivalent file for 29 of 30 resolved Swift packages. It also found package-specific notice inputs including `grpc-swift/NOTICES.txt` and `opentelemetry-swift/NOTICE`.

The transitive `thrift-swift` checkout at revision `18ff09e6b30e589ed38f90a1af23e193b8ecef8e` contains no root license or notice file. Therefore complete distributable notice coverage and legal approval cannot be verified from the resolved source graph. This blocks marking public application distribution approved.

`Orlix/App/THIRD_PARTY_NOTICES.md` currently contains the vendored native Ghostty, libssh2, and OpenSSL notices. It does not yet contain a verified distributable notice set for all 30 resolved Swift packages. Do not represent the current notice file as complete.

## Required closure evidence

- Confirm the license and attribution obligations for `thrift-swift` at the pinned revision from an authoritative source.
- Produce and legally review a distributable notice set covering all resolved Swift packages and native artifacts, including required NOTICE content.
- Build and inspect a real exported iOS and iPadOS application package, including the aggregate privacy report and embedded dependency manifests.
- Record written legal and App Review decisions before setting `public_distribution_approved` to `true`.
