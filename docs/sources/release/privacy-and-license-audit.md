---
type: source
tags:
  - provenance
updated: 2026-07-15
status: current
summary: "Canonical repository source for privacy and license audit."
---

# Privacy and license audit

## Scope

This record covers the Swift package resolution and application privacy manifest used by the Orlix iOS and iPadOS application at build 35. It is engineering evidence, not legal approval, App Review approval, or an exported-package privacy report.

The authoritative package resolution is:

- Path: `Orlix.xcodeproj/project.xcworkspace/xcshareddata/swiftpm/Package.resolved`
- SHA-256: `e87bd90e34626ead2825ad45a8de13f4e1132e6fda81286eb03c844f33dc013e`
- Resolved pins: 32

## Privacy manifest union

The audit inspected every `PrivacyInfo.xcprivacy` file in the 32 resolved package checkouts under the dynamically resolved Xcode package cache. Fifteen dependency manifests were present.

- Swift Crypto, SwiftNIO SSL, and Swift Protobuf manifests declared no tracking, collected data, tracking domains, or required-reason API use.
- Three SwiftNIO manifests and ZIPFoundation declared `NSPrivacyAccessedAPICategoryFileTimestamp` with reason `0A2A.1`: `NIOFS`, `NIOPosix`, `_NIOFileSystem`, and `ZIPFoundation`.
- The Orlix application already declared `NSPrivacyAccessedAPICategoryUserDefaults` with reason `CA92.1` for its telemetry data categories.
- `Orlix/Orlix/PrivacyInfo.xcprivacy` now contains the union of non-empty application and resolved-dependency required-reason declarations: UserDefaults `CA92.1` and File Timestamp `0A2A.1`.

The three non-empty SwiftNIO manifests were byte-identical at SHA-256 `982f20d820bc4460acc9d4116c3e6c8ec837f717dc82edbb7308f7400515c909`. ZIPFoundation's manifest SHA-256 is `9a2f930cedb8d58309a581b9bf9bf3673685ec02ae2197d9f1c56828b718dffd`. The resulting Orlix source manifest SHA-256 is `6d3ed45d047b7e3ed09da81372eb18e7360633e04682682b75bb95f8cb4ba6c6`.

The release gate compares the exported app privacy manifest structurally against this source manifest. That exported-app verification remains unproven until a healthy archive and export complete.

## License and notice audit

The same resolved-checkout audit found a root `LICENSE`, `LICENSE.txt`, or equivalent file for 31 of 32 resolved Swift packages. It also found package-specific notice inputs including `grpc-swift/NOTICES.txt` and `opentelemetry-swift/NOTICE`.

Resolution includes `thrift-swift` because OpenTelemetry exposes a `JaegerExporter` target. Orlix links `OpenTelemetryProtocolExporterHTTP`, `OpenTelemetryApi`, and `OpenTelemetrySdk`; it does not request `JaegerExporter`. Therefore the resolved `thrift-swift` checkout must not be represented as shipped Orlix code without build or link evidence.

`Orlix/THIRD_PARTY_NOTICES.md` currently contains notices for the vendored native Ghostty, libssh2, and OpenSSL artifacts. It does not yet contain a verified distributable notice set for the Swift package products actually linked into the exported Orlix application. Do not use all 32 resolved pins as a substitute for shipped-product closure, and do not represent the current notice file as complete.

## Required closure evidence

- Capture the actual Swift package product target closure from a healthy Orlix archive build or link map.
- Produce a legally reviewed distributable notice set covering the shipped Swift package closure and all native artifacts, including any required NOTICE content.
- Build and inspect a real exported iOS and iPadOS application package, including the aggregate privacy report and embedded dependency manifests.
- Record written legal and App Review decisions before setting `public_distribution_approved` to `true`.
