# Orlix Terminal Infrastructure Platform Implementation

## Objective

Execute the active plan by importing pinned VVTerm directly as the Orlix iOS and iPadOS application, then adding the Default Local Instance, Herdr authority, remote transports, and the mobile container product. Native macOS implementation remains last, after the mobile terminal and container releases are in good shape and published to the App Store.

## Current checkpoint

- Status: #50 direct VVTerm mobile application integration and evidence collection in progress.
- Pinned VVTerm revision: `791eebae946b0831ffff3ac839e0f2b75d076458`.
- Application rule: compile imported VVTerm application source directly as Orlix. Do not create an `OrlixTerminal` framework or generic product boundary.
- Fallback rule: isolate `TerminalViewController` and its `libghostty-spm` dependency in a separate developer-only diagnostic app target. Production Orlix has exactly one SwiftUI `@main`, supplied by the Orlix application fork.
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
- Required one production Orlix application SwiftUI `@main`. The current UIKit controller and `libghostty-spm` are isolated in a separate developer-only diagnostic application target so the production executable does not link two Ghostty implementations.
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

## 2026-07-13 pinned VVTerm subtree import

- Added `https://github.com/vivy-company/vvterm.git` as the local `vvterm-upstream` remote.
- Fetched immutable commit `791eebae946b0831ffff3ac839e0f2b75d076458`.
- Imported the complete repository at `Orlix/App` with a non-squashed Git subtree.
- Subtree merge commit: `63bcb1230fa739ac6fbc34d873b349ffee566453`.
- Recorded package pins, native source versions, artifact hashes, rebuild entry points, license paths, and update policy in `docs/reference/VVTERM_PROVENANCE.md`.

The subtree import preserves the upstream application, Live Activity, shared source, resources, packages, vendor libraries, scripts, unit tests, UI tests, and upstream project as a baseline reference. It does not by itself prove that the Orlix XcodeGen target compiles or launches the imported application.

## 2026-07-13 direct VVTerm mobile application checkpoint

### Scope and immutable inputs

This checkpoint implements only #50, the direct VVTerm mobile application foundation. It does not start #49, #51, Herdr, containers, Docker compatibility, or native macOS implementation.

- Working branch: `feat/vvterm-orlix-mobile-foundation`.
- Pinned upstream revision: `791eebae946b0831ffff3ac839e0f2b75d076458`.
- Planning commit: `7389fb99d26c82acaefa08a9d8339c39ea4b24d7`, `docs(terminal): adopt direct VVTerm mobile foundation`.
- Non-squashed subtree merge: `63bcb1230fa739ac6fbc34d873b349ffee566453`.
- Provenance commit: `c4f853a2`, `docs(vvterm): record import provenance`.
- Durable project definition: `project.yml`. The generated `Orlix.xcodeproj` remains ignored and disposable.

### Direct application and target mapping

`project.yml` now defines the following target split:

| Target | Checkpoint mapping |
| --- | --- |
| `Orlix` | Production iOS and iPadOS application. Directly compiles imported `App`, `Compatibility`, `Core`, `Features`, `Generated`, `GhosttyTerminal`, and `OrlixShared` source, plus Orlix telemetry. It embeds imported resources and links OrlixOS, OrlixKernel, the Live Activity extension, and the imported production package graph. |
| `OrlixLiveActivity` | Imported Live Activity and shared widget source, translated to the Orlix product and bundle identity. |
| `OrlixAppTests` | Imported unit-test target. Its 62 imported Swift files plus the separately identified Orlix telemetry test file compile against the `Orlix` module. |
| `OrlixUITests` | Imported UI-test target. Its five Swift files run against the `Orlix` application. |
| `OrlixLegacyTerminal` | Separate developer-only diagnostic application. It alone compiles the previous UIKit application source and links `libghostty-spm` through `GhosttyKit`. |

Production has exactly one SwiftUI entry point, `@main struct OrlixApp: App`, in `Orlix/App/Orlix/App/OrlixApp.swift`. It does not compile `AppDelegate.swift`, `SceneDelegate.swift`, `TerminalViewController.swift`, or `ApplicationExitController.swift` from the previous UIKit application. The old UIKit `@main` and `GhosttyKit` dependency are confined to `OrlixLegacyTerminal`. Production instead links the fork's vendored Ghostty archives and headers. The two Ghostty implementations do not coexist in one executable.

The production target directly retains the imported asset catalogs, resource and localization tree, `ghostty` folder resource, `terminfo` folder resource, privacy manifest, Live Activity, MLX, Cloudflared, Mosh, ZIPFoundation, OpenTelemetry, and AppIntents inputs. SwiftUmami is absent from the production dependency graph. OrlixOS remains the app-facing delivered Linux session owner. This checkpoint does not move Linux lifecycle or policy into the application.

### Complete Orlix fork rename

The imported application is now an Orlix fork internally and externally. The final source root is `Orlix/App`; its application, shared, Live Activity, unit-test, and UI-test directories, source filenames, Swift types, launch arguments, accessibility identifiers, generated constants, scripts, resources, assets, target metadata, schemes, bundle identities, CloudKit container, Keychain group, URL scheme, StoreKit products, storage keys, environment variables, and remote tmux namespaces use Orlix naming.

- The application source root is `Orlix/App/Orlix`.
- The production entry point is `Orlix/App/Orlix/App/OrlixApp.swift`.
- The privacy manifest is `Orlix/App/Orlix/PrivacyInfo.xcprivacy`.
- The Live Activity asset is `OrlixLiveIcon`.
- All 14 main application and all 14 Live Activity localization sets use Orlix product copy.
- The imported unit inventory remains 62 Swift files. `OrlixAppTests` also compiles the separate Orlix telemetry test file.
- The inherited Aizen logger namespaces, notification names, managed-session fixtures, theme defaults, theme filenames, source headers, and web package name were renamed to Orlix.
- The visible About footer now identifies Orlix. The source link is labeled `Upstream Source`, and the mobile tagline names iPhone and iPad. Required upstream copyright, license, immutable import history, and the real upstream repository URL remain as provenance.
- The renamed standalone `Orlix.xcodeproj` no longer links or resolves SwiftUmami, matching the authoritative `project.yml` graph.

The original non-squashed subtree was imported at `Orlix/VVTerm`, then moved and fully renamed to `Orlix/App`. The final integration commit renews the subtree metadata at the current path.

### Integration corrections

The following corrections were required to make the imported application graph compile as Orlix:

- Declared `Orlix/App/Orlix/PrivacyInfo.xcprivacy` as a source entry with `buildPhase: resources` for production and the legacy diagnostic target. The legacy source directory excludes its same-named manifest so each target receives one deliberate manifest. The earlier target-level XcodeGen `resources` entries did not copy the production manifest.
- Renamed the complete product copy across all 14 main application localization sets and all 14 Live Activity localization sets. This includes the visible Face ID prompt.
- Renamed the Live Activity asset and its source reference to `OrlixLiveIcon`.
- Added device and simulator Ghostty and libssh2 header search paths to `OrlixAppTests`. The first integrated unit attempt stopped at `fatal error: 'ghostty.h' file not found`.
- Added the Swift `DEBUG` active compilation condition to the Debug configuration. The second integrated unit attempt otherwise omitted the test-only `TerminalTabManager.resetForTesting` helper.
- Kept the imported test changes narrow. Sixty-one of 62 unit files only change `@testable import VVTerm` to `@testable import Orlix`. `RemoteTmuxManagerParserTests.swift` additionally translates intended Orlix runtime paths and identifiers.
- Replaced the no-op imported analytics compatibility methods with typed mappings into `OrlixTelemetry`, preserving connection, paywall, purchase, limit, onboarding, custom-action, split, review, and analytics-disable event properties without retaining the upstream Umami transport.

### Build and unit evidence

The environment was checked before Xcode work:

```sh
rtk proxy xcode-offload doctor \
  --root "$(external-ssd-root)" \
  --require-shims \
  --strict
```

All checks passed. The required `Orlix-iPhone-15-Pro-Max` simulator, UDID `ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3`, reached `Device already booted, nothing to do.`

#### Pristine pinned baseline

- Checkout: `/tmp/orlix-vvterm-plan-791eebae` at `791eebae946b0831ffff3ac839e0f2b75d076458`.
- The pristine iPhone application build succeeded.
- Unit log: `/tmp/orlix-vvterm-pristine-unit-tests.log`.
- XCTest completed 92 tests with two failures:
  - `TerminalAccessoryProfileTests.testNormalizedDropsDeletedCustomActionReferences`
  - `TerminalAccessoryProfileTests.testNormalizedRemovesDuplicateActiveItems`
- Swift Testing completed 240 tests in 39 suites. `RemoteFilePermissionTests.draftUpdatesBitsAndSummaries()` failed three expectations for `0o740`, `"740"`, and `"rwxr-----"`.
- Xcode 27 beta stopped making progress while finalizing the result bundle after test execution. The invocation was interrupted and is not recorded as a passing `xcodebuild` run.
- UI log: `/tmp/orlix-vvterm-pristine-ui-tests.log`.
- `NoticePresentationUITests` completed six tests with zero failures.
- `TerminalKeyboardUITests` completed seven tests with seven failures while waiting for `keyboardVisible=true`, although the terminal and IME proxy were active.
- `OrlixUITests` completed two tests with zero failures.
- The later launch-configuration matrix stopped progressing during automation-session setup for more than 20 minutes. It was interrupted with exit 130, so the full pristine UI invocation is incomplete.
- A pristine iPad build and test baseline has not yet been recorded.

#### Integrated Orlix application

The production iPhone application build succeeded. The post-privacy-fix log is `/tmp/orlix-vvterm-integrated-iphone-build-privacy.log`; the earlier successful integrated log is `/tmp/orlix-vvterm-integrated-iphone-build-6.log`. The successful build compiled and linked the direct imported source, vendored Ghostty, imported package graph, OrlixOS, OrlixKernel, Live Activity, OpenTelemetry, both asset catalogs, and the app-level privacy manifest. It did not link SwiftUmami.

The integrated build commands were:

```sh
rtk proxy xcodebuild \
  -project Orlix.xcodeproj \
  -scheme Orlix \
  -configuration Debug \
  -destination 'platform=iOS Simulator,id=ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3' \
  build

rtk proxy xcodebuild \
  -project Orlix.xcodeproj \
  -scheme OrlixLegacyTerminal \
  -configuration Debug \
  -destination 'platform=iOS Simulator,id=ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3' \
  build

rtk proxy xcodebuild \
  -project Orlix.xcodeproj \
  -scheme Orlix \
  -configuration Debug \
  -destination 'platform=iOS Simulator,id=AF8A3028-1F38-4409-958F-21AD8137B6BA' \
  build
```

The developer-only legacy application also built successfully on the required iPhone simulator. Its log is `/tmp/orlix-vvterm-legacy-iphone-build.log`. Its dependency graph contains `GhosttyKit`; the production dependency graph does not.

A real iPad simulator was created because none was available:

- Name: `Orlix-iPad-Air-11-M4`.
- Device: iPad Air 11-inch M4.
- Runtime: iOS 26.5.
- UDID: `AF8A3028-1F38-4409-958F-21AD8137B6BA`.
- First-boot data migration reached terminal `Finished` successfully.

The integrated production application built successfully for this iPad destination. The log is `/tmp/orlix-vvterm-integrated-ipad-build.log`. This proves an iPad-targeted compile and link, not iPad runtime feature behavior.

Integrated unit attempts are recorded separately:

```sh
rtk proxy xcodebuild \
  -project Orlix.xcodeproj \
  -scheme "Orlix App Tests" \
  -configuration Debug \
  -destination 'platform=iOS Simulator,id=ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3' \
  -parallel-testing-enabled NO \
  -enableCodeCoverage NO \
  -only-testing:OrlixAppTests \
  test
```

| Attempt | Log | Result |
| --- | --- | --- |
| 1 | `/tmp/orlix-vvterm-integrated-unit-tests.log` | Compile stopped because `ghostty.h` was not found. Result bundle: `Test-Orlix App Tests-2026.07.13_22-33-24-+0200.xcresult`. |
| 2 | `/tmp/orlix-vvterm-integrated-unit-tests-2.log` | Headers compiled, then tests stopped compiling because the Swift `DEBUG` condition did not expose `resetForTesting`. Result bundle: `Test-Orlix App Tests-2026.07.13_22-34-08-+0200.xcresult`. |
| 3 | `/tmp/orlix-vvterm-integrated-unit-tests-3.log` | Test execution completed with the exact pristine failure set, then Xcode stopped making progress during result-bundle finalization and was interrupted. |

The third attempt completed 92 XCTest tests with the same two `TerminalAccessoryProfileTests` failures and 240 Swift Testing tests in 39 suites with the same three failed `RemoteFilePermissionTests.draftUpdatesBitsAndSummaries()` expectations. No additional Orlix-specific unit failure was observed. The imported baseline remains red, and the interrupted `xcodebuild` invocation is not a pass.

#### Final renamed iPhone evidence

After the complete Orlix rename and final source corrections, the project was regenerated and the production `Orlix` Debug build succeeded on required iPhone simulator `ADE0D3EB-6E89-41DD-9AB9-CA20F10609F3`. The known vendored Ghostty warning remains: some simulator objects were built for iOS Simulator 17.0 while Orlix links with deployment target 16.1.

- `TelemetryTests`: 12 executed, 12 passed. Result bundle: `Test-Orlix App Tests-2026.07.13_23-20-33-+0200.xcresult`.
- Main `OrlixUITests`: 2 executed, 2 passed. Result bundle: `Test-Orlix App Tests-2026.07.13_23-31-03-+0200.xcresult`.
- `TerminalKeyboardUITests`: 7 executed, 7 failed in the same keyboard-visible family as the pristine baseline. Result bundle: `Test-Orlix App Tests-2026.07.13_23-26-40-+0200.xcresult`.
- `NoticePresentationUITests` first exposed a deterministic harness timing failure. The fixed three-second handoff completed before XCTest returned from the integrated application launch, so the test could not observe the initial connecting sheet. The debug harness now preserves the initial state for a 30-second observation window. The exact formerly failing test then executed once with zero failures. Result bundle: `Test-Orlix App Tests-2026.07.13_23-40-50-+0200.xcresult`.
- The full fixed Notice suite executed six tests with zero failures. Xcode then stopped emitting output for 90 seconds during result-bundle finalization and was interrupted with exit 130. The executed assertions are green, but the interrupted invocation is not recorded as a passing `xcodebuild` run. Existing partial result bundle: `Test-Orlix App Tests-2026.07.13_23-46-09-+0200.xcresult`.
- Two earlier post-fix attempts executed no test because the stale booted simulator could not attach to `testmanagerd`. Rebooting the required iPhone reached terminal `Finished` and restored XCTest attachment. Those environment failures are recorded separately in `Test-Orlix App Tests-2026.07.13_23-36-21-+0200.xcresult` and `Test-Orlix App Tests-2026.07.13_23-37-34-+0200.xcresult`.

The final application installed successfully. `simctl launch` returned PID `56554`; the process remained alive after 23 seconds and again after 64 seconds. No Orlix, OrlixAppTests, OrlixUITests, XCTest, or SimLaunchHost crash report appeared in the observation window from `2026-07-13T21:33:36Z` through `2026-07-13T21:34:53Z`. This proves installation, launch, and short-term process survival only.

#### Final renamed iPad evidence

The final project was regenerated with both simulators initially shut down. Only `Orlix-iPad-Air-11-M4`, UDID `AF8A3028-1F38-4409-958F-21AD8137B6BA`, was booted for the following evidence. Both simulators were shut down when the packet finished.

- The production `Orlix` Debug build succeeded. Its target graph excludes `GhosttyKit` and its link line uses vendored `libghostty`. The known iOS Simulator 17.0 archive versus deployment target 16.1 warning remains. Log: `/tmp/orlix-final-rename-ipad-build.log`.
- `TelemetryTests`: 12 executed, 12 passed, and the invocation completed successfully. Result bundle: `Test-Orlix App Tests-2026.07.13_23-53-10-+0200.xcresult`.
- Full `OrlixAppTests`: XCTest executed 104 tests, including the 12 Orlix telemetry tests, with the same two `TerminalAccessoryProfileTests` failures. Swift Testing executed 240 tests in 39 suites with four issues. Three are the known `RemoteFilePermissionTests.draftUpdatesBitsAndSummaries()` expectations. The fourth is an unresolved iPad delta in `TerminalNativeFindTests.trimsWhitespaceOnlyQueriesBeforeSearching`, which expected `[NSRange(location: 7, length: 4)]`. Xcode stopped progressing after execution and was interrupted with exit 130. No parity or passing-invocation claim is made. Log: `/tmp/orlix-final-rename-ipad-unit-tests.log`.
- Focused main `OrlixUITests`: two executed, two passed, including `testExample` and `testLaunchPerformance`. Result bundle: `Test-Orlix App Tests-2026.07.13_23-57-45-+0200.xcresult`.
- The final application installed successfully. `simctl launch` returned PID `28575`, which remained alive after 43 seconds.
- The crash audit covered `2026-07-13 23:59:33 +0200` through `2026-07-14 00:01:29 +0200` across host DiagnosticReports and the iPad simulator CrashReporter. No matching Orlix, OrlixAppTests, OrlixUITests, XCTest, or SimLaunchHost report appeared.
- The developer-only `OrlixLegacyTerminal` build succeeded. Its 22-target graph explicitly contains the `GhosttyTerminal` and `GhosttyTheme` products from `GhosttyKit`, while production remains on vendored `libghostty`. Log: `/tmp/orlix-final-rename-ipad-legacy-build.log`.

The iPad smoke proves compilation, installation, launch, short-term survival, focused UI launch behavior, telemetry contracts, and dependency isolation. It does not prove terminal rendering, remote transport interoperability, or OrlixOS local-session behavior.

The vendored simulator `libghostty.a` objects report iOS Simulator 17.0 as their build version while the Orlix application declares iOS 16.1. This currently produces linker warnings rather than a build failure. The deployment compatibility must be resolved or accepted with evidence before release.

#### Post-vendor correction smoke

After the final OpenSSL/libssh2 rebuild and fixed-length Ghostty archive identity correction, the production iPad simulator build was rerun and completed with `** BUILD SUCCEEDED **`. The rebuilt application was installed on `Orlix-iPad-Air-11-M4` (`AF8A3028-1F38-4409-958F-21AD8137B6BA`) and launched with `simctl`. The process returned PID `74301`, remained alive for at least 3 minutes 22 seconds, and was still alive when the observation ended at `2026-07-14 00:16:27 +0200`. No matching host DiagnosticReports directory or simulator crash report was present. The simulator was shut down after the smoke.

This post-vendor smoke updates installation, launch, and short-term survival evidence only. It does not add terminal-rendering, SSH/Mosh interoperability, local OrlixOS-session, Herdr, container, or macOS product proof.

### Resource, provenance, and identity audits

The built production bundle was inspected and contained:

- `Assets.car` and all four imported fonts, byte-identical to their sources;
- 14 localization directories;
- `ghostty/themes` with 440 files matching the source tree;
- `terminfo/xterm-ghostty.src`, SHA-256 `8f15a6afe74a2a43467c8c5675ff013eecf0ed718cdd4e9c3e883b5a7503acfb`;
- arm64 `OrlixOS.framework` with identifier `com.rudironsoni.OrlixOS`;
- arm64 `OrlixKernel.framework` with identifier `com.rudironsoni.OrlixKernel`;
- `OrlixLiveActivity.appex` with identifier `com.rudironsoni.Orlix.liveactivity`;
- the main application with identifier `com.rudironsoni.Orlix` and display name `Orlix`;
- app-level `PrivacyInfo.xcprivacy`.

`plutil -lint` accepted the privacy manifest. The manifest declares the UserDefaults reason `CA92.1`, Product Interaction, Purchase History, Performance Data, and Other Diagnostic Data, with tracking disabled. This is schema evidence for the current manifest, not final privacy, legal, or App Store approval.

All 15 native archive SHA-256 values documented in `docs/reference/VVTERM_PROVENANCE.md` were independently recomputed. Fifteen matched and none were missing.

A targeted identity scan found none of the following stale values in the compiled iOS and iPadOS Swift, plist, entitlement, or StoreKit inputs: `app.vivy`, `iCloud.app.vivy`, `analytics.vivy.app`, `id6757482822`, `com.vivy.vivyterm`, or `vvterm-cfaccess`. Imported compatible macOS source metadata and web identity were also renamed to Orlix without beginning the macOS product implementation. The About source label is `Upstream Source`. Only the actual upstream repository URL, immutable import history, and required upstream legal and copyright attribution retain the upstream name.

### Feature parity ledger

State meanings for this ledger are:

- `RETAINED`: the imported source or resource remains in the production target graph.
- `BUILT`: the integrated iPhone or iPad build compiled and linked the relevant graph.
- `RUNTIME-TESTED`: behavior was actually exercised through the Orlix-derived target or its tests.
- `BLOCKED`: the relevant proof has a known failure.
- `AWAITING`: required evidence has not been recorded.

`RETAINED` and `BUILT` do not imply that a feature works at runtime.

| # | Capability category | State | Evidence and remaining proof |
| ---: | --- | --- | --- |
| 1 | Direct application foundation | RETAINED, BUILT, RUNTIME-TESTED | `OrlixApp` is the sole production SwiftUI `@main`; iPhone and iPad builds succeeded. Final iPhone install, launch, and survival passed. |
| 2 | Ghostty rendering | RETAINED, BUILT, AWAITING | Vendored Ghostty compiled and linked. Rendering, sustained output, scrollback, and memory behavior are not yet runtime-tested. |
| 3 | SSH authentication and transport | RETAINED, BUILT, AWAITING | Imported SSH source and libssh2 archives built. No Orlix remote-host acceptance run is recorded. |
| 4 | Mosh | RETAINED, BUILT, AWAITING | Swift Mosh dependency and integration built. Roaming, reconnect, and server interoperability remain untested here. |
| 5 | Tabs, splits, reconnect, and persistence | RETAINED, BUILT, AWAITING | Imported feature graph built. Integrated lifecycle and restoration behavior remain untested. |
| 6 | tmux | RETAINED, BUILT, AWAITING | Imported tmux parsing and attach source built. Remote tmux behavior remains untested. This is not Herdr integration. |
| 7 | Clipboard, rich paste, input, find, selection, and pointer | RETAINED, BUILT, AWAITING | Imported terminal interaction source built. Integrated UI and device behavior remain untested. |
| 8 | Keyboard and accessory bar | RETAINED, BUILT, BLOCKED | The pristine keyboard UI baseline has seven `keyboardVisible` failures. Integrated UI comparison is awaiting. |
| 9 | Live Activity, Dynamic Island, and widget | RETAINED, BUILT, AWAITING | The extension built and is embedded. Activity lifecycle and presentation are not runtime-tested. |
| 10 | Workspaces, servers, and Pro limits | RETAINED, BUILT, AWAITING | Imported models and UI built. Persistence, limits, and interaction are not runtime-tested. |
| 11 | Local discovery | RETAINED, BUILT, AWAITING | Discovery source built. Network discovery behavior is not runtime-tested. |
| 12 | Remote files and SFTP | RETAINED, BUILT, AWAITING | Imported browser and transfer source built. Server interoperability and file behavior are not runtime-tested. |
| 13 | Keychain | RETAINED, BUILT, AWAITING | Keychain source and translated identity built. Real credential creation, retrieval, migration, and access-group behavior remain untested. |
| 14 | CloudKit and notifications | RETAINED, BUILT, AWAITING | Sync and notification source built. Provisioning, production schema, synchronization, and notification behavior remain unproved. |
| 15 | App lock, biometrics, and privacy | RETAINED, BUILT, AWAITING | Source, Face ID strings, and privacy manifest built. Biometric flow, lock lifecycle, and privacy behavior remain untested. |
| 16 | Themes | RETAINED, BUILT, AWAITING | Theme source and 440 Ghostty theme files are present. Selection and rendering remain untested. |
| 17 | Accessory customization and presets | RETAINED, BUILT, BLOCKED | Source built. Two pristine and integrated normalization unit tests fail identically. UI behavior remains untested. |
| 18 | Settings, welcome, support, and localization | RETAINED, BUILT, AWAITING | Source and 14 localizations built. Screen, layout, accessibility, and translation review remain untested. |
| 19 | StoreKit and Pro | RETAINED, BUILT, AWAITING | Store source and translated StoreKit input built. Purchase, restore, entitlement, and App Store configuration remain unproved. |
| 20 | Server statistics | RETAINED, BUILT, AWAITING | Collection and parsing source built. Live remote collection remains untested. |
| 21 | Voice input | RETAINED, BUILT, AWAITING | MLX and speech source built. Permission, model, transcription, and resource behavior remain untested. |
| 22 | Resources | RETAINED, BUILT | Bundle inspection verified assets, fonts, localizations, themes, terminfo, privacy manifest, frameworks, and extension. Feature-level resource use remains awaiting. |
| 23 | Dependencies | RETAINED, BUILT | Imported production graph linked without SwiftUmami. Mutable-input and release provenance enforcement belong to #49 and are not started. |
| 24 | Identity, entitlements, privacy, and encryption | RETAINED, BUILT, AWAITING | Targeted mobile identity scan and manifest lint passed. Provisioning, CloudKit production schema, export encryption classification, legal review, and exported-package inspection remain #49 work. |
| 25 | Lifecycle and Orlix telemetry | RETAINED, BUILT, RUNTIME-TESTED, AWAITING | The complete imported product event surface maps into Orlix telemetry with properties. Twelve focused telemetry tests passed and launch survival passed. Background, foreground, termination, and live emitted telemetry remain untested. |
| 26 | Unit tests | RUNTIME-TESTED, BLOCKED | Final iPhone telemetry passed 12/12. The prior integrated iPhone imported run matched the red pristine baseline. Final iPad execution ran 104 XCTest tests with the same two imported failures and 240 Swift Testing tests with the three known expectation failures plus one unresolved iPad-only `TerminalNativeFindTests` issue. Xcode result-bundle finalization stalled after execution. |
| 27 | UI tests | RUNTIME-TESTED, BLOCKED | Final iPhone suites completed with 6/6 Notice assertions and 2/2 main UI tests passing. Keyboard remained 0/7, matching the pristine failure family. Notice result-bundle finalization stalled after green assertions and was interrupted. |
| 28 | Integration and snapshot target classification | AWAITING | The pinned upstream has no separate integration or snapshot targets. Any Orlix-specific coverage must remain separately labeled and has not been claimed. |
| 29 | iPhone | BUILT, RUNTIME-TESTED | Production and legacy builds succeeded on the required iPhone simulator. Final install and launch returned a PID that survived 64 seconds, with no matching fresh crash report. Feature runtime proof remains limited to the tests listed above. |
| 30 | iPadOS | BUILT, RUNTIME-TESTED, BLOCKED | Production build, 12/12 telemetry, 2/2 focused UI, install, 43-second launch survival, and crash scan passed. The full unit target remains red and includes one unresolved iPad-only delta. |
| 31 | Legacy diagnostic application | BUILT | The separate application compiled with `GhosttyKit` after the final rename. Production uses vendored `libghostty`. The legacy target is developer-only and does not prove production behavior. |
| 32 | OrlixOS embedding | BUILT, AWAITING | `OrlixOS.framework` and `OrlixKernel.framework` are embedded and arm64. No Default Local Instance or OrlixOS terminal session runtime is claimed; that remains #51. |

### Final checkpoint boundary

The only remaining checkpoint evidence insertion is final bundle reinspection after the last iPad build. All iPhone and iPad build, test, install, launch, crash, and dependency-isolation evidence is recorded above.

This checkpoint proves direct source integration, complete Orlix fork naming, successful iPhone and iPad compilation, successful legacy diagnostic isolation, final iPhone and iPad install and launch survival, bundle composition, provenance hashes, stale-identity removal, typed telemetry preservation, and the exact imported unit and UI behavior recorded above. It does not prove terminal rendering, remote connectivity, OrlixOS terminal behavior, App Store readiness, #49, #51, Herdr, containers, Docker compatibility, or native macOS behavior.
