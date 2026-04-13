// swift-tools-version:5.9

import PackageDescription

let package = Package(
    name: "IXLandSystem",
    platforms: [
        .iOS(.v16)
    ],
    products: [
        .library(
            name: "IXLandSystem",
            targets: ["IXLandSystem"]
        ),
    ],
    targets: [
        .target(
            name: "IXLandSystem",
            path: ".",
            exclude: [
                "Package.swift",
                "README.md",
                "AGENTS.md",
                "BUILD_COMPLETE.md",
                "BUILD_STATUS.md",
                "BUILD.md",
                "IMPLEMENTATION_STATUS.md",
                "IMPLEMENTATION_SUMMARY.md",
                "FINAL_STATUS.md",
                "SUMMARY.md",
                "WAMR_INTEGRATION.md",
                "MIGRATION_STATUS.md",
                "REALITY_CHECK.md",
                "docs",
                "Tests",
                ".xcodebuildmcp",
                "createXcFrameworks.sh",
                "fix_frameworks.sh",
                "getopt_long.c",
                "getopt.c",
                "ssh_cmd",
                "ssh_main.c",
                "compat/interpose",
                "Debug-iphonesimulator",
                "libixlandTest",
            ],
            sources: [
                "kernel",
                "fs",
                "runtime",
                "observability",
                "compat",
            ],
            publicHeadersPath: "include",
            cSettings: [
                .headerSearchPath("include"),
                .headerSearchPath("."),
                .define("_XOPEN_SOURCE"),
            ]
        ),
    ]
)
