// swift-tools-version: 6.2

import PackageDescription

let package = Package(
    name: "OrlixSwiftDependencies",
    platforms: [
        .iOS(.v15),
        .macOS(.v13),
    ],
    dependencies: [
        .package(url: "https://github.com/rudironsoni/mlx-swift", revision: "7b6527f6eb6013c5221679ee08112c04aab6825e"),
        .package(url: "https://github.com/rudironsoni/swift-cloudflared.git", revision: "80eb5b73e00effe78c8d6d44d8aab8ffdd63e976"),
        .package(url: "https://github.com/rudironsoni/swift-mosh", revision: "e476c1e8745cc1d1a353bc72b8ee1d73eb35e197"),
        .package(url: "https://github.com/rudironsoni/swift-et", revision: "47cb48446af6565b5103ddaa431388c1d5bce366"),
        .package(url: "https://github.com/apple/swift-numerics", revision: "0c0290ff6b24942dadb83a929ffaaa1481df04a2"),
        .package(url: "https://github.com/bitmark-inc/tweetnacl-swiftwrap.git", revision: "f8fd111642bf2336b11ef9ea828510693106e954"),
        .package(url: "https://github.com/weichsel/ZIPFoundation.git", revision: "edbeaa39b426e54702194b0a601342322f01e400"),
        .package(url: "https://github.com/open-telemetry/opentelemetry-swift.git", revision: "9a6d6a8aed22c415bb1673206e337824635f818b"),
        .package(url: "https://github.com/open-telemetry/opentelemetry-swift-core.git", revision: "06f8a460a66f813758d22f09025d85df45450a63"),
    ]
)
