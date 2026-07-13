# Contributing to Orlix

Thanks for your interest in contributing to Orlix.

## Code of Conduct

By participating in this project, you agree to follow `CODE_OF_CONDUCT.md`.

## Before You Start

1. Search existing issues and pull requests to avoid duplicate work.
2. For large changes, open an issue first to align on approach and scope.
3. Keep pull requests focused and small when possible.

## Development Setup

Requirements:

- Xcode 16.0+
- Zig (for building Ghostty): `brew install zig`

Setup:

```bash
git clone https://github.com/rudironsoni/orlix.git
cd orlix
./scripts/build.sh all
open Orlix.xcodeproj
```

## Pull Request Guidelines

1. Create a branch from `main`.
2. Make your changes with clear commit messages.
3. Run relevant checks/tests locally before opening a PR.
4. Include screenshots or recordings for UI changes.
5. Include clear validation notes for networking/terminal behavior changes.

## License

By submitting contributions, you agree that your contributions may be distributed under the project's source license:

- Source code license: `LICENSE` (GPL-3.0)

The imported upstream CLA and App Store binary terms are retained as `UPSTREAM-CLA.md` and `UPSTREAM-APPSTORE-BINARY-LICENSE.md` for provenance only. They do not impose contribution or binary-distribution terms for Orlix.
