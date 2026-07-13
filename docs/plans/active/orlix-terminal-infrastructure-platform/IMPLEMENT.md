# Orlix Terminal Infrastructure Platform Implementation

## Objective

Execute the active plan by importing pinned VVTerm directly as the Orlix iOS and iPadOS application, then adding the Default Local Instance, Herdr authority, remote transports, and the mobile container product. Native macOS implementation remains last, after the mobile terminal and container releases are in good shape and published to the App Store.

## Current checkpoint

- Status: documentation correction in progress.
- Pinned VVTerm revision: `791eebae946b0831ffff3ac839e0f2b75d076458`.
- Application rule: compile imported VVTerm application source directly as Orlix. Do not create an `OrlixTerminal` framework or generic product boundary.
- Fallback rule: isolate `TerminalViewController` and its `libghostty-spm` dependency in a separate developer-only diagnostic app target. Production Orlix has exactly one SwiftUI `@main`, supplied by the VVTerm-derived application.
- Kit rule: OrlixOS remains the delivered Linux Kit and the app-facing Local Runtime and Local Instance API owner.
- Platform order: publish the iOS and iPadOS terminal release, then the iOS and iPadOS container release, then begin native macOS implementation.
- TCTI dependency: preserve the active no-phone golden ELF and switch-debug oracle lane. This project consumes its evidence and does not redirect it.

## Execution ledger

1. Correct ADRs, the active plan, glossary, ownership skill, and tracker issues to the direct-VVTerm mobile-first direction.
2. **#50:** establish the pristine pinned VVTerm iPhone and iPad baseline, import the full application with history, translate it into `project.yml`, and launch the complete mobile feature surface as Orlix.
3. **#49:** inventory the actual imported features and fail closed on missing capability, provenance, identity, privacy, encryption, entitlement, provisioning, CloudKit, or test evidence.
4. **#51:** add the Default Local Instance as a real `TerminalTarget` backed directly by OrlixOS, never by VVTerm Server or SSH.
5. Integrate Herdr only after #50, #49, and #51, making it authoritative for Sessions, Workspaces, Tabs, splits, and Panes while keeping native and raw TUI clients as peers.
6. Complete remote transport and RootShell-derived regression work.
7. Complete the mobile container and Docker product. Apply Contained visual references and Orchard typed patterns here, not during the direct VVTerm import.
8. Publish the mobile container and Docker release with Windows Containers as the sole deliberate exclusion.
9. Begin native macOS implementation using the compatible source and package foundation preserved during mobile work.

## Verification record

For each checkpoint, record exact commands, immutable commits, dependency and artifact hashes, rebuild commands, targets, destinations, pass/fail counts, skips and reasons, `.xcresult` paths, crash-report checks, and evidence paths. Do not claim Local Runtime readiness outside ADR 0017 promotion order or ahead of the active TCTI evidence.

The #50 baseline record must include:

- the upstream remote, pinned commit, history-preserving subtree import command and location, and documented immutable update command;
- all mobile sources, resources, localizations, packages, vendor libraries, entitlements, privacy manifests, Live Activity inputs, unit tests, and UI tests translated into `project.yml`;
- pristine and Orlix-derived iPhone and iPad runs for the one upstream unit target with 62 Swift files and one UI target with 5 Swift files;
- separately labeled Orlix integration or snapshot coverage because the pinned upstream does not provide separate integration or snapshot targets;
- a feature parity ledger and pristine comparison with no silently omitted mobile feature.

The #49 evidence record must include:

- immutable commits for `mlx-swift`, `swift-cloudflared`, `swift-mosh`, `swift-numerics`, `swift-umami`, TweetNaCl, and ZIPFoundation;
- revisions, hashes, rebuild commands, licenses, and notices for vendored Ghostty, libssh2, OpenSSL, and every generated vendor artifact;
- proof that mutable `custom-io`, `main`, or other branch-only inputs cannot enter a release;
- the stale-identity mapping and scan for app, test, UI-test, Live Activity, CloudKit, storage, Keychain, Cloudflare, widget, StoreKit, team, product-name, usage-string, APNs, and network-entitlement values;
- provisioning-profile and CloudKit production-schema evidence;
- schema validation of the merged privacy-manifest union and verified encryption export classification in the exported `Info.plist`.

The #51 evidence record must cover input, output, resize, close, background, foreground, and restart through a directly backed OrlixOS terminal target, plus ownership checks proving no Local Runtime or Linux policy moved into the Swift app or OrlixHostAdapter.

## 2026-07-13 documentation and tracker correction

- Corrected ADR 0024, ADR 0027, this active plan, the glossary, and the implementation-boundaries skill to make the complete pinned VVTerm mobile application the direct Orlix application foundation. No `OrlixTerminal` framework or generic product boundary remains in the accepted direction.
- Required one production VVTerm-derived SwiftUI `@main`. The current UIKit controller and `libghostty-spm` are isolated in a separate developer-only diagnostic application target so the production executable does not link two Ghostty implementations.
- Recorded the implementation order as #50 direct VVTerm import, #49 imported-feature capability and provenance gate, #51 Default Local Instance through OrlixOS, then Herdr.
- Moved Contained visual reference work and Orchard typed container patterns to the later mobile Container phase.
- Updated GitHub issues #50, #49, and #51 to this order and acceptance model. Also removed the rejected `OrlixTerminal` boundary from deferred macOS issue #94 without starting Mac implementation.

Verification:

```text
rtk proxy make tcti-gate TARGET=tcti-plan-consistency
pass: Build/TCTI/reports/tcti-plan-consistency/report.json

rtk proxy make agent-harness-check
pass: all

rtk git diff --check
exit 0
```

The stale-reference scan found `OrlixTerminal` only in explicit prohibitions. These checks prove documentation and harness consistency only. They do not prove imported application behavior.
