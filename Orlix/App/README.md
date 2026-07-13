# Orlix

[![macOS](https://img.shields.io/badge/macOS-13.3+-black?style=flat-square&logo=apple)](https://github.com/rudironsoni/orlix)
[![iOS](https://img.shields.io/badge/iOS-16.1+-black?style=flat-square&logo=apple)](https://github.com/rudironsoni/orlix)
[![Swift](https://img.shields.io/badge/Swift-5.0+-F05138?style=flat-square&logo=swift&logoColor=white)](https://swift.org)
[![Source License](https://img.shields.io/badge/Source-GPL%203.0-blue?style=flat-square)](LICENSE)
[![Sponsor](https://img.shields.io/badge/Sponsor-GitHub-ff69b4?style=flat-square&logo=github)](https://github.com/sponsors/rudironsoni)

Your servers. Everywhere.

![Orlix macOS](/web/src/preview.png)

## Overview

Orlix is a cross-platform SSH terminal app for Apple platforms. The current codebase targets iOS and macOS, uses Ghostty for terminal rendering, libssh2/OpenSSL for SSH transport, CloudKit for sync, and Keychain for local credential storage.

This application is an Orlix-owned fork of [VVTerm](https://github.com/vivy-company/vvterm), imported at commit `791eebae946b0831ffff3ac839e0f2b75d076458`. The fork retains the upstream GPL license, copyright notice, and third-party attributions. Orlix product names, identifiers, targets, schemes, storage namespaces, and user-facing identity are independent from the upstream application.

## Current State

- Main app target: `Orlix`
- Companion target: `OrlixLiveActivity`
- Runtime targets: `macOS 13.3+` and `iOS 16.1+`
- Hardware targets: Apple Silicon / arm64 only
- App-owned code is organized under `Orlix/App`, `Orlix/Core`, and `Orlix/Features`
- The repo also contains tests, native vendor builds, feature specs under `docs/specs`, and the marketing site under `web/`

## Implemented Feature Areas

### Terminal and connections

- GPU-accelerated terminal rendering via `GhosttyKit`
- SSH authentication with password, SSH key, and SSH key + passphrase
- Connection modes for standard SSH, Tailscale, Mosh, and Cloudflare Access
- Multi-session connection management with tabs, split panes, reconnect handling, and persisted session state
- tmux-aware startup, attach, install, and recovery flows
- Rich paste and clipboard helpers for terminal input
- iOS keyboard accessory support, including special keys and custom actions
- iOS Live Activity status for active terminal connections

### Servers and organization

- Workspaces with ordering, colors, and environment grouping
- Server metadata including favorites, tags, notes, last-connected timestamps, and biometric-unlock requirements
- Pro limit enforcement for workspaces, servers, tabs, and split panes
- Local-network SSH discovery via Bonjour and subnet probing

### Remote files

- SFTP-backed remote file browser for iOS and macOS
- Directory browsing with breadcrumbs, sorting, hidden-file toggles, and persisted browser state
- File preview, upload, download, export/share, new folder, rename, move, and delete flows
- Permission editing and remote-file conflict resolution

### Security and sync

- Keychain-backed storage for SSH credentials and Cloudflare service tokens
- CloudKit sync for servers, workspaces, terminal theme preferences, and terminal accessory profile data
- Full-app lock and per-server biometric unlock
- Privacy-mode support

### Customization and productivity

- Built-in and custom terminal themes with validation, storage-path management, and sync-aware preference handling
- Customizable terminal accessory bar with reorderable actions and user-defined shortcuts
- Terminal presets for saved commands/snippets
- Settings surfaces for general, terminal, sync, keychain, pro, and about flows
- Welcome/onboarding and in-app support surfaces

### Stats and voice input

- Remote server stats collection with live CPU and memory history
- On-device voice-to-command pipeline with MLX model management and Apple Speech fallback

## Architecture

Orlix uses a feature-first structure for app-owned code.

```text
Orlix/
├── App/                         # App entry, composition roots, shared root containers
├── Core/                        # Shared infrastructure and cross-feature primitives
│   ├── Logging/
│   ├── Network/
│   ├── Security/
│   ├── SSH/
│   ├── Sync/
│   ├── Terminal/
│   └── UI/
├── Features/                    # Product features
│   ├── ConnectionViews/
│   ├── LocalDiscovery/
│   ├── RemoteFiles/
│   ├── Security/
│   ├── Servers/
│   ├── Settings/
│   ├── Stats/
│   ├── Store/
│   ├── Support/
│   ├── TerminalAccessories/
│   ├── TerminalPresets/
│   ├── TerminalSessions/
│   ├── TerminalThemes/
│   ├── VoiceInput/
│   └── Welcome/
├── GhosttyTerminal/             # Ghostty bridge and terminal host views
├── Compatibility/               # Version/platform helpers
├── Generated/                   # Build-time generated sources
└── Resources/                   # Bundled assets, themes, terminfo, localizations
```

Feature modules follow these boundaries:

- `Domain`: pure types and rules
- `Application`: state, orchestration, coordinators, managers
- `Infrastructure`: persistence, transport, adapters, external integrations
- `UI`: SwiftUI/AppKit/UIKit presentation

Other top-level folders in the repo:

```text
Orlix-iOS/                     # iOS Info.plist and entitlements
Orlix-macOS/                   # macOS Info.plist and entitlements
OrlixLiveActivity/             # ActivityKit target
OrlixShared/                   # Shared Activity attributes and small shared types
OrlixTests/                    # Unit and integration tests
OrlixUITests/                  # UI tests
Vendor/                         # Vendored native dependencies
docs/specs/                     # Feature specs and implementation notes
scripts/                        # Vendor build scripts
web/                            # Imported Astro site, renamed for the Orlix fork
```

## Requirements

- Apple Silicon Mac for development
- Xcode `16.0+`
- macOS `13.3+`
- iOS `16.1+`
- `zig` and `cmake`

Install the non-Xcode build tools with Homebrew:

```bash
brew install zig cmake
```

## Building From Source

```bash
git clone https://github.com/rudironsoni/orlix.git
cd orlix

# Build native vendor libraries (GhosttyKit + libssh2/OpenSSL)
./scripts/build.sh all

# Open the project in Xcode
open Orlix.xcodeproj
```

`./scripts/build.sh` supports `all`, `ghostty`, `ssh`, `clean`, and `help`.

## Dependencies

Native/vendor dependencies:

- [libghostty](https://github.com/ghostty-org/ghostty) for terminal emulation and rendering
- [libssh2](https://github.com/libssh2/libssh2) for SSH transport
- [OpenSSL](https://github.com/openssl/openssl) for cryptography

Swift package dependencies currently resolved by the Xcode project:

- [Cloudflared](https://github.com/wiedymi/swift-cloudflared)
- [swift-mosh](https://github.com/wiedymi/swift-mosh)
- [mlx-swift](https://github.com/ml-explore/mlx-swift)
- [ZIPFoundation](https://github.com/weichsel/ZIPFoundation)
- [swift-numerics](https://github.com/apple/swift-numerics)
- [TweetNacl](https://github.com/bitmark-inc/tweetnacl-swiftwrap.git)

## Installation

Orlix does not yet publish an App Store link from this imported project metadata. Release instructions and published distribution links belong to the containing Orlix repository.

## Pro Tier

| Feature | Free | Pro |
|---------|------|-----|
| Workspaces | 1 | Unlimited |
| Servers | 1 | Unlimited |
| Tabs | 1 | Unlimited |
| Split panes | No | Yes |

**Pricing:** Monthly ($6.49), Yearly ($24.99), Lifetime ($49.99)

## Documentation

- [CONTRIBUTING.md](CONTRIBUTING.md) for contribution workflow
- [SECURITY.md](SECURITY.md) for vulnerability reporting
- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for third-party notices
- [UPSTREAM-CLA.md](UPSTREAM-CLA.md) for the imported upstream contributor license agreement
- `docs/specs/` for feature specs such as biometric locks, local discovery, terminal themes, terminal accessories, remote rich clipboard, and the SFTP browser

## License

Source code in this imported fork is licensed under GNU GPL v3.0 (`LICENSE`). The imported upstream App Store binary license is retained as [UPSTREAM-APPSTORE-BINARY-LICENSE.md](UPSTREAM-APPSTORE-BINARY-LICENSE.md) for provenance only. It does not define licensing or distribution terms for Orlix binaries.

Copyright © 2026 Orlix contributors
