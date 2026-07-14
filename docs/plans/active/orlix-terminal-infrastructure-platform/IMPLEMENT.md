# Orlix Terminal Infrastructure Platform Implementation

## Objective

Execute the active plan from the complete native Orlix iOS and iPadOS application, then add the Default Local Instance, Herdr authority, remote transports, and the mobile container product. Native macOS implementation remains last, after the mobile terminal and container releases are in good shape and published to the App Store.

## Current checkpoint

- Status: #50 direct native Orlix application integration checkpoint recorded; #49 immutable release-input provenance checkpoint in progress.
- Pinned imported revision: `791eebae946b0831ffff3ac839e0f2b75d076458`.
- Application rule: compile the native application source directly as Orlix. Do not create an `OrlixTerminal` framework or generic product boundary.
- Target rule: production Orlix has exactly one SwiftUI `@main`, supplied by `OrlixApp`. The retired UIKit application target, duplicate assets, and conflicting Ghostty package are removed.
- Kit rule: OrlixOS remains the delivered Linux Kit and the app-facing Local Runtime and Local Instance API owner.
- Platform order: publish the iOS and iPadOS terminal release, then the iOS and iPadOS container release, then begin native macOS implementation.
- TCTI dependency: preserve the active no-phone golden ELF and switch-debug oracle lane. This project consumes its evidence and does not redirect it.

## Execution ledger

1. Keep ADRs, the active plan, glossary, ownership skill, and tracker issues aligned with the native Orlix mobile-first direction.
2. **#50:** preserve the pristine imported iPhone and iPad baseline, compile the full application through `project.yml`, and launch the complete mobile feature surface as Orlix.
3. **#49:** inventory the actual imported features and fail closed on missing capability, provenance, identity, privacy, encryption, entitlement, provisioning, CloudKit, or test evidence.
4. **#51:** add the Default Local Instance as a real `TerminalTarget` backed directly by OrlixOS, never as a Remote Host or SSH connection.
5. Integrate Herdr only after #50, #49, and #51, making it authoritative for Sessions, Workspaces, Tabs, splits, and Panes while keeping native and raw TUI clients as peers.
6. Complete remote transport and RootShell-derived regression work.
7. Complete the mobile container and Docker product. Apply Contained visual references and Orchard typed patterns only in that phase.
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

- Corrected ADR 0024, ADR 0027, this active plan, the glossary, and the implementation-boundaries skill to make the complete native mobile application the direct Orlix foundation. No `OrlixTerminal` framework or generic product boundary remains in the accepted direction.
- Required one production `OrlixApp` SwiftUI `@main`. The retired UIKit application and conflicting Ghostty dependency are removed.
- Recorded the implementation order as #50 native Orlix application, #49 imported-feature capability and provenance gate, #51 Default Local Instance through OrlixOS, then Herdr.
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

## 2026-07-13 pinned application-source import

- Fetched immutable commit `791eebae946b0831ffff3ac839e0f2b75d076458`.
- Imported the complete repository at `Orlix/App` with a non-squashed Git subtree.
- Subtree merge commit: `63bcb1230fa739ac6fbc34d873b349ffee566453`.
- Recorded immutable origin, package pins, native source versions, artifact hashes, rebuild entry points, license paths, and update policy in `docs/reference/ORLIX_APP_SOURCE_PROVENANCE.md`.

The subtree import preserves the upstream application, Live Activity, shared source, resources, packages, vendor libraries, scripts, unit tests, UI tests, and upstream project as a baseline reference. It does not by itself prove that the Orlix XcodeGen target compiles or launches the imported application.

## 2026-07-13 native Orlix mobile application checkpoint

### Scope and immutable inputs

This checkpoint implements only #50, the native Orlix mobile application foundation. It does not start #49, #51, Herdr, containers, Docker compatibility, or native macOS implementation.

- Working branch: `feat/orlix-mobile-foundation`.
- Pinned imported revision: `791eebae946b0831ffff3ac839e0f2b75d076458`.
- Planning commit: `7389fb99d26c82acaefa08a9d8339c39ea4b24d7`.
- Non-squashed subtree merge: `63bcb1230fa739ac6fbc34d873b349ffee566453`.
- Provenance commit: `c4f853a2`.
- Durable project definition: `project.yml`. The generated `Orlix.xcodeproj` remains ignored and disposable.

### Direct application and target mapping

`project.yml` now defines the following target split:

| Target | Checkpoint mapping |
| --- | --- |
| `Orlix` | Production iOS and iPadOS application. Directly compiles imported `App`, `Compatibility`, `Core`, `Features`, `Generated`, `GhosttyTerminal`, and `OrlixShared` source, plus Orlix telemetry. It embeds imported resources and links OrlixOS, OrlixKernel, the Live Activity extension, and the imported production package graph. |
| `OrlixLiveActivity` | Imported Live Activity and shared widget source, translated to the Orlix product and bundle identity. |
| `OrlixAppTests` | Unit-test target. Its imported Swift files and Orlix telemetry tests compile against the `Orlix` module. |
| `OrlixUITests` | Imported UI-test target. Its five Swift files run against the `Orlix` application. |

Production has exactly one SwiftUI entry point, `@main struct OrlixApp: App`, in `Orlix/App/Orlix/App/OrlixApp.swift`. The retired UIKit application source, duplicate assets, target, scheme, bundle identity, and conflicting Ghostty package are removed. Production links only the vendored Ghostty archives and headers.

The production target directly retains the imported asset catalogs, resource and localization tree, `ghostty` folder resource, `terminfo` folder resource, privacy manifest, Live Activity, MLX, Cloudflared, Mosh, ZIPFoundation, OpenTelemetry, and AppIntents inputs. SwiftUmami is absent from the production dependency graph. OrlixOS remains the app-facing delivered Linux session owner. This checkpoint does not move Linux lifecycle or policy into the application.

### Complete Orlix identity

The application uses Orlix identity internally and externally. The final source root is `Orlix/App`; its application, shared, Live Activity, unit-test, and UI-test directories, source filenames, Swift types, launch arguments, accessibility identifiers, generated constants, scripts, resources, assets, target metadata, schemes, bundle identities, CloudKit container, Keychain group, URL scheme, StoreKit products, storage keys, environment variables, and remote tmux namespaces use Orlix naming.

- The application source root is `Orlix/App/Orlix`.
- The production entry point is `Orlix/App/Orlix/App/OrlixApp.swift`.
- The privacy manifest is `Orlix/App/Orlix/PrivacyInfo.xcprivacy`.
- The Live Activity asset is `OrlixLiveIcon`.
- All 14 main application and all 14 Live Activity localization sets use Orlix product copy.
- The imported unit inventory remains 62 Swift files. `OrlixAppTests` also compiles the separate Orlix telemetry test file.
- The inherited Aizen logger namespaces, notification names, managed-session fixtures, theme defaults, theme filenames, source headers, and web package name were renamed to Orlix.
- The visible About footer identifies Orlix, the source link points to OrlixSystem, and the mobile tagline names iPhone and iPad. Required copyright, license, and immutable import history remain in the license and source-provenance record.
- The renamed standalone `Orlix.xcodeproj` no longer links or resolves SwiftUmami, matching the authoritative `project.yml` graph.

The original non-squashed subtree was imported at `Orlix/Orlix`, then moved and fully renamed to `Orlix/App`. The final integration commit renews the subtree metadata at the current path.

### Integration corrections

The following corrections were required to make the imported application graph compile as Orlix:

- Declared `Orlix/App/Orlix/PrivacyInfo.xcprivacy` as a source entry with `buildPhase: resources` for production and the legacy diagnostic target. The legacy source directory excludes its same-named manifest so each target receives one deliberate manifest. The earlier target-level XcodeGen `resources` entries did not copy the production manifest.
- Renamed the complete product copy across all 14 main application localization sets and all 14 Live Activity localization sets. This includes the visible Face ID prompt.
- Renamed the Live Activity asset and its source reference to `OrlixLiveIcon`.
- Added device and simulator Ghostty and libssh2 header search paths to `OrlixAppTests`. The first integrated unit attempt stopped at `fatal error: 'ghostty.h' file not found`.
- Added the Swift `DEBUG` active compilation condition to the Debug configuration. The second integrated unit attempt otherwise omitted the test-only `TerminalTabManager.resetForTesting` helper.
- Kept the imported test changes narrow. Sixty-one of 62 unit files only update the test-module import to `@testable import Orlix`. `RemoteTmuxManagerParserTests.swift` additionally translates intended Orlix runtime paths and identifiers.
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

- Checkout: `/tmp/orlix-app-plan-791eebae` at `791eebae946b0831ffff3ac839e0f2b75d076458`.
- The pristine iPhone application build succeeded.
- Unit log: `/tmp/orlix-app-pristine-unit-tests.log`.
- XCTest completed 92 tests with two failures:
  - `TerminalAccessoryProfileTests.testNormalizedDropsDeletedCustomActionReferences`
  - `TerminalAccessoryProfileTests.testNormalizedRemovesDuplicateActiveItems`
- Swift Testing completed 240 tests in 39 suites. `RemoteFilePermissionTests.draftUpdatesBitsAndSummaries()` failed three expectations for `0o740`, `"740"`, and `"rwxr-----"`.
- Xcode 27 beta stopped making progress while finalizing the result bundle after test execution. The invocation was interrupted and is not recorded as a passing `xcodebuild` run.
- UI log: `/tmp/orlix-app-pristine-ui-tests.log`.
- `NoticePresentationUITests` completed six tests with zero failures.
- `TerminalKeyboardUITests` completed seven tests with seven failures while waiting for `keyboardVisible=true`, although the terminal and IME proxy were active.
- `OrlixUITests` completed two tests with zero failures.
- The later launch-configuration matrix stopped progressing during automation-session setup for more than 20 minutes. It was interrupted with exit 130, so the full pristine UI invocation is incomplete.
- A pristine iPad build and test baseline has not yet been recorded.

#### Integrated Orlix application

The production iPhone application build succeeded. The post-privacy-fix log is `/tmp/orlix-app-integrated-iphone-build-privacy.log`; the earlier successful integrated log is `/tmp/orlix-app-integrated-iphone-build-6.log`. The successful build compiled and linked the direct imported source, vendored Ghostty, imported package graph, OrlixOS, OrlixKernel, Live Activity, OpenTelemetry, both asset catalogs, and the app-level privacy manifest. It did not link SwiftUmami.

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
  -scheme Orlix \
  -configuration Debug \
  -destination 'platform=iOS Simulator,id=AF8A3028-1F38-4409-958F-21AD8137B6BA' \
  build
```

A real iPad simulator was created because none was available:

- Name: `Orlix-iPad-Air-11-M4`.
- Device: iPad Air 11-inch M4.
- Runtime: iOS 26.5.
- UDID: `AF8A3028-1F38-4409-958F-21AD8137B6BA`.
- First-boot data migration reached terminal `Finished` successfully.

The integrated production application built successfully for this iPad destination. The log is `/tmp/orlix-app-integrated-ipad-build.log`. This proves an iPad-targeted compile and link, not iPad runtime feature behavior.

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
| 1 | `/tmp/orlix-app-integrated-unit-tests.log` | Compile stopped because `ghostty.h` was not found. Result bundle: `Test-Orlix App Tests-2026.07.13_22-33-24-+0200.xcresult`. |
| 2 | `/tmp/orlix-app-integrated-unit-tests-2.log` | Headers compiled, then tests stopped compiling because the Swift `DEBUG` condition did not expose `resetForTesting`. Result bundle: `Test-Orlix App Tests-2026.07.13_22-34-08-+0200.xcresult`. |
| 3 | `/tmp/orlix-app-integrated-unit-tests-3.log` | Test execution completed with the exact pristine failure set, then Xcode stopped making progress during result-bundle finalization and was interrupted. |

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

All 15 native archive SHA-256 values documented in `docs/reference/ORLIX_APP_SOURCE_PROVENANCE.md` were independently recomputed. Fifteen matched and none were missing.

A targeted identity scan found no stale non-Orlix product or organization values in the compiled iOS and iPadOS Swift, plist, entitlement, StoreKit, resource, or binary inputs. Compatible macOS source metadata and web identity were also renamed to Orlix without beginning the macOS product implementation. The About source link points to the OrlixSystem repository. Immutable origin facts and required copyright notices remain confined to the license and source-provenance record.

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
| 29 | iPhone | BUILT, RUNTIME-TESTED | The production build succeeded on the required iPhone simulator. Final install and launch returned a PID that survived 64 seconds, with no matching fresh crash report. Feature runtime proof remains limited to the tests listed above. |
| 30 | iPadOS | BUILT, RUNTIME-TESTED, BLOCKED | Production build, 12/12 telemetry, 2/2 focused UI, install, 43-second launch survival, and crash scan passed. The full unit target remains red and includes one unresolved iPad-only delta. |
| 31 | OrlixOS embedding | BUILT, AWAITING | `OrlixOS.framework` and `OrlixKernel.framework` are embedded and arm64. No Default Local Instance or OrlixOS terminal session runtime is claimed; that remains #51. |

### Final checkpoint boundary

The only remaining checkpoint evidence insertion is final bundle reinspection after the last iPad build. All iPhone and iPad build, test, install, launch, crash, and dependency-isolation evidence is recorded above.

This checkpoint proves direct source integration, complete Orlix naming, successful iPhone and iPad compilation, final iPhone and iPad install and launch survival, bundle composition, provenance hashes, stale-identity removal, typed telemetry preservation, and the exact imported unit and UI behavior recorded above. It does not prove terminal rendering, remote connectivity, OrlixOS terminal behavior, #49, #51, Herdr, containers, Docker compatibility, or native macOS behavior.

#### 2026-07-14 Post-integration beta runtime gate

- OrlixOS target-derived payload and root-descriptor metadata executed two tests with zero failures on the pinned simulator.
- The original PTY selector executed zero tests because it omitted the XCTest class. The corrected selector executed the intended PTY test once. That test and the OCI-derived materialized-root test each failed in the hosted Linux runtime with `Kernel panic - not syncing: Orlix: failed to synchronize hosted user mappings`.
- A later current build-30 TCTI stability run completed its kernel, payload, app build, install, and launch path but captured no first TCTI syscall marker or `linux_exec_start_thread` event during its 45-second runtime window.
- This evidence does not invalidate the direct Orlix application import. It blocks TestFlight promotion and proves neither Ghostty rendering nor Default Local Instance behavior.
- #51 Default Local Instance, Herdr, containers, Docker compatibility, and native macOS remain unstarted. The app-fork checkpoint evidence is recorded, but final bundle reinspection is not the only remaining release work. Current TCTI runtime and structured release gates must pass before TestFlight promotion.

#### 2026-07-14 Orlix application identity clarification

- ADR 0024 and the active plan now state the product boundary explicitly: Orlix owns its application fork, Orlix is the sole application identity, and the pinned upstream source is ancestry rather than a product, compatibility layer, target, module, bundle identifier, source path, or public architecture concept.
- The repository-wide tracked-file and content audit found no upstream product identity in source, project metadata, targets, tests, resources, website content, scripts, ADRs, plans, architecture documentation, or tracked paths.
- The only remaining upstream-name text occurrences are the immutable repository URL in `docs/reference/ORLIX_APP_SOURCE_PROVENANCE.md` and the original copyright notice in `Orlix/App/LICENSE`. Those references remain because changing them would falsify provenance or legal attribution.
- `project.yml` and the retained Xcode project identify the app, tests, UI tests, Live Activity extension, bundle identifiers, and products as Orlix. This checkpoint changes documentation wording only and does not add new runtime proof.

#### 2026-07-14 Beta simulator gate retry

- The clean `make beta-simulator-gate` retry passed dependency, project-generation, package-resolution, Xcode/CoreSimulator health, required-simulator boot, and single-booted-simulator checks.
- The OrlixOS metadata stage executed both intended tests and passed with zero failures and zero skips.
- The corrected PTY selector executed `testLinuxPTYCarriesInteractiveShellInputAndOutput` exactly once. The guest reached `/init`, then executable hosted-user-page refresh returned `-1` and the kernel panicked with `Orlix: failed to synchronize hosted user mappings`. XCTest failed the selected case with one unexpected failure.
- Xcode emitted the complete one-test failure summary, then produced no output for more than 90 seconds during result-bundle finalization. The finalizer was interrupted only after preserving the explicit failure evidence. The gate remains failed.
- Current TCTI status at commit `a6873e51` still selects `tcti-kernel-execve-binfmt-elf-smoke`. Its validated envelope is `environment_only_failure` with `must_stop=true`, `continue_refresh_allowed=false`, `runtime_patch_allowed=false`, `harness_patch_allowed=false`, and `release_gate_eligible=false`.
- No archive, IPA export, upload, App Store Connect mutation, build-number bump, physical-device execution, runtime patch, or harness patch followed this retry.

#### 2026-07-14 plan-context harness concurrency repair

- Parallel read-only tool calls could lose required active-plan paths because each post-tool hook performed an unlocked read-modify-write of the same temporary JSON state file. The observed state retained eight required paths and dropped three paths that had completed successfully, so the next legitimate `make` command was blocked.
- `.codex/hooks/orlix_hook_common.py` now serializes state updates with `flock` and atomically replaces the JSON state file. Concurrent readers can no longer overwrite one another or expose a partially written state file.
- `.codex/hooks/tests/test_lifecycle_guards.py` now launches concurrent post-tool read hooks across multiple active plans and proves a subsequent workspace mutation is allowed only after every required path survives.

Evidence:

```text
rtk proxy python3 -m unittest discover -s .codex/hooks/tests -p 'test_lifecycle_guards.py' -v
24 tests, including test_plan_context_guard_preserves_concurrent_required_reads
OK

rtk proxy make tcti-gate TARGET=tcti-plan-consistency
pass: Build/TCTI/reports/tcti-plan-consistency/report.json

rtk proxy make agent-harness-check
pass: all
```

This repair changes harness state coordination only. It adds no terminal, TCTI, Local Runtime, package, or release proof.

#### 2026-07-14 #49 immutable application release inputs

- Added `docs/reference/ORLIX_APP_RELEASE_INPUTS.json` as the machine-readable application release-input record for the imported source revision, every direct Swift package URL and full commit, pinned Ghostty/OpenSSL/libssh2 source inputs, all 15 committed native archive hashes, and required engineering evidence paths.
- Replaced the two version-only OpenTelemetry declarations in `project.yml` with the exact commits already present in the resolved graph: `21374ac2439aee4e206721ef91a7e8bf4c0579d6` and `84b9e341cbb7b4dd62cd1b89cd9e008995084132`.
- Ran the sanctioned product-input version target. `CURRENT_PROJECT_VERSION` advanced from 32 to 33 because this checkpoint changes `project.yml` and `Orlix/App/scripts/build.sh`.
- Updated `Orlix/App/scripts/build.sh` to reject non-commit Ghostty references, verify OpenSSL 3.2.0 source archive SHA-256 `14c826f07c7e433706fb5c69fa9e25dab95684844b4c962a2cf1bf183eb4690e`, and verify libssh2 1.11.0 source archive SHA-256 `3736161e41e2693324deb38c26cfdc3efe6209d634ba4258db1cecff6a5ad461` before extraction.
- Added `tools/release/orlix-app-release-inputs-check.sh` and its fail-closed regression test. `beta-prerequisites` now runs the same check before beta build work.
- Updated `docs/reference/ORLIX_APP_SOURCE_PROVENANCE.md` to identify the machine-readable record and distinguish engineering input integrity from public-distribution approval.

Evidence:

```text
rtk proxy bash -n Orlix/App/scripts/build.sh tools/release/orlix-app-release-inputs-check.sh tools/release/tests/test-orlix-app-release-inputs.sh
exit 0

rtk proxy make app-release-inputs-test
pass: Orlix application release inputs
pass: Orlix application release-input checks fail closed

rtk proxy make beta-prerequisites
exit 0

rtk proxy make product-build-prepare
bumped CURRENT_PROJECT_VERSION 32 -> 33 for product input changes

rtk proxy make product-build-version-check
product version unchanged: CURRENT_PROJECT_VERSION=33

rtk proxy xcodegen generate --spec project.yml
Created project at .../Orlix.xcodeproj

rtk proxy rg -n '21374ac2439aee4e206721ef91a7e8bf4c0579d6|84b9e341cbb7b4dd62cd1b89cd9e008995084132|exactVersion|branch:' Orlix.xcodeproj/project.pbxproj project.yml
generated project contains both full revisions; project.yml contains no exactVersion or branch declaration

rtk proxy jq -e '<OpenTelemetry resolved-revision equality>' Orlix.xcodeproj/project.xcworkspace/xcshareddata/swiftpm/Package.resolved
true

rtk proxy rg -n 'custom-io|exactVersion:|branch:' project.yml Orlix/App/scripts/build.sh docs/reference/ORLIX_APP_RELEASE_INPUTS.json
exit 1, no matches

rtk git diff --check
exit 0

rtk proxy make tcti-gate TARGET=tcti-plan-consistency
pass: Build/TCTI/reports/tcti-plan-consistency/report.json

rtk proxy make agent-harness-check
pass: all
```

The first post-change `agent-harness-check` correctly failed because the changed product inputs still carried build 32. After `product-build-prepare` advanced the build to 33, the report-backed kernel-gate freshness assertion and the full harness passed. No harness assertion was weakened to accept stale product evidence.

The required Xcode health check did not pass, so no `xcodebuild -resolvePackageDependencies` or simulator build is claimed for this checkpoint:

```text
rtk err env PATH="$HOME/.local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin" xcode-offload doctor --root "$(external-ssd-root)" --require-shims --strict
FAIL Cache sparsebundle is not readable by hdiutil: hdiutil: imageinfo failed - image not recognized
```

Current mount status also reports the system CoreSimulator Caches, Images, and Volumes stores and `/Applications/Xcodes` not mounted through their configured sparsebundles. Project source was not changed to compensate for that environment failure.

This is a partial #49 checkpoint. Capability inventory and runtime availability behavior, complete package-license notices, stale exported-product identity validation, entitlement and provisioning-profile validation, CloudKit production-schema evidence, privacy-manifest union review, encryption export classification, App Review approval, and written legal approval remain incomplete. `public_distribution_approved` remains `false`. #51, Herdr, Local Instances, containers, Docker compatibility, and native macOS implementation remain unstarted.

Final post-review verification after all tracked source edits: the focused release-input test passed both positive and deliberate-drift cases, `product-build-version-check` kept build 33, the 24 lifecycle-hook tests passed, `git diff --check` passed, plan consistency passed, and `agent-harness-check` exited 0 with `pass: all`. Both release-input scripts are executable with mode 0755.

#### 2026-07-14 #49 capability and exported-product fail-closed gate

[CORRECTION] I previously gave an unverified or speculative answer. It should have been labeled, and here is the corrected version. The earlier #50 feature-parity record claimed the targeted compiled-input identity scan found no stale identifiers. A new source-level scan found six compiled uses of `win.orlix.app` or `com.orlix` in logging, terminal preset storage, Ghostty notifications, IME, input, and macOS terminal input handling. Those identifiers now use `com.rudironsoni.Orlix`; the focused compiled-input scan returns no remaining forbidden identity fragment.

- Extended `docs/reference/ORLIX_APP_RELEASE_INPUTS.json` with one 36-entry capability inventory covering feature-ledger items 1 through 31 exactly once plus every named planned terminal-infrastructure capability. Each entry records iOS, iPadOS, and macOS state; internal invocation and public-advertising flags; repository evidence; approval requirements; and an actionable unavailable or promotion condition.
- Kept all public advertising disabled because `public_distribution_approved` remains `false`. Blocked, planned, deferred, and built-but-unverified states are not promoted to runtime proof.
- Bundled the same manifest in Orlix application resources. This makes the validated record available to application code without creating a second capability catalog. Application UI adoption is not claimed by this checkpoint.
- Added `tools/release/orlix_app_capability_gate.py`. Its repository mode rejects missing or duplicate capability IDs, incomplete platform maps, invalid invocation or advertising state, missing evidence, incomplete feature-ledger coverage, missing exported-product declarations, and forbidden compiled-input identity fragments.
- Added exported-application validation for the real `.app`: main and Live Activity bundle identity, display name, encryption declaration, byte-equivalent capability record, structurally equivalent privacy manifest, Live Activity widget kind in the extension executable, signed application entitlements, and embedded provisioning-profile entitlements. The strict public mode additionally requires recorded distribution, encryption, provisioning, and CloudKit production approvals.
- Integrated repository validation into existing immutable release-input checks and exported-product validation into `beta-validate-archive`. `beta-validate-archive` does not use strict public-approval mode because it is also an engineering archive inspection target. Public distribution remains independently closed.
- Added eight regression tests covering the valid manifest and fake exported package, missing capability, forbidden source identity, encryption mismatch, missing privacy manifest, provisioning mismatch, and missing public approvals.
- Ran sanctioned product-input versioning after changing `project.yml`; `CURRENT_PROJECT_VERSION` advanced from 33 to 34.

Evidence:

```text
rtk proxy make app-capability-test
8 tests, OK

rtk proxy make app-capability-gate
pass: Orlix application capability manifest

rtk proxy make app-release-inputs-test
pass: Orlix application capability manifest
pass: Orlix application release inputs
pass: Orlix application release-input checks fail closed

rtk proxy bash -n tools/release/orlix-app-release-inputs-check.sh
exit 0

rtk proxy make -n beta-validate-archive
exit 0

rtk proxy make product-build-prepare
bumped CURRENT_PROJECT_VERSION 33 -> 34 product input changes

rtk proxy make product-build-version-check
product version unchanged: CURRENT_PROJECT_VERSION=34

rtk proxy xcodegen generate --spec project.yml
Created project at .../Orlix.xcodeproj

rtk proxy rg -n '<forbidden identity fragments>' Orlix/App/Orlix project.yml Orlix.xcodeproj/project.pbxproj
exit 1, no matches

rtk git diff --check
exit 0
```

No real archive, signed entitlement, provisioning profile, CloudKit production schema, encryption classification, App Review approval, or legal approval was available to validate in this checkpoint. The exported-package gate is regression-tested against synthetic package inputs but has not yet passed a real archive. Complete package-license notice coverage and privacy-manifest union review remain #49 work. #51 remains blocked until #49 finishes and the active TCTI/runtime gates permit implementation.

#### 2026-07-14 #49 privacy union and package-license audit

- Audited all 30 pins in the authoritative Swift package resolution, SHA-256 `fbfb181219b355d4500e5279a4cd75cb7345532cab65e141f9c9f1f02c190f2d`.
- Inspected 14 dependency `PrivacyInfo.xcprivacy` files. Three resolved SwiftNIO target manifests declared File Timestamp required-reason API `0A2A.1`; the remaining dependency manifests declared no tracking, collected data, tracking domains, or required-reason APIs.
- Added File Timestamp `0A2A.1` to the Orlix app manifest alongside existing UserDefaults `CA92.1`. `docs/reference/ORLIX_APP_PRIVACY_AND_LICENSE_AUDIT.md` records the union evidence and limits.
- Release inputs now pin the full `Package.resolved` hash and pin count, the reviewed privacy source hash, and the exact required-reason union. The validator rejects resolution or privacy-union drift.
- The license scan found a root license input for 29 of 30 resolved packages plus package-specific notices such as gRPC Swift and OpenTelemetry. The transitive `thrift-swift` checkout at `18ff09e6b30e589ed38f90a1af23e193b8ecef8e` contains Apache License 2.0 source headers that require a `NOTICE`, but no root license or notice file. The existing distributable notice file covers only the three native vendored dependencies. `package_license_notice_status` therefore remains fail-closed as `blocked_missing_thrift_swift_notice`.
- The strict public gate now requires approved privacy and package-license status in addition to distribution, encryption, provisioning, and CloudKit production approval.
- Ran sanctioned product-input versioning after changing the bundled privacy manifest; `CURRENT_PROJECT_VERSION` advanced from 34 to 35.

Evidence:

```text
rtk proxy make app-capability-test
10 tests, OK

rtk proxy make app-release-inputs-test
pass: Orlix application capability manifest
pass: Orlix application release inputs
pass: Orlix application release-input checks fail closed

rtk proxy plutil -lint Orlix/App/Orlix/PrivacyInfo.xcprivacy
Orlix/App/Orlix/PrivacyInfo.xcprivacy: OK

rtk proxy make product-build-prepare
bumped CURRENT_PROJECT_VERSION 34 -> 35 product input changes

rtk proxy make product-build-version-check
product version unchanged: CURRENT_PROJECT_VERSION=35

rtk proxy xcodegen generate --spec project.yml
Created project at .../Orlix.xcodeproj

rtk git diff --check
exit 0
```

This closes the locally derivable privacy source union but does not prove the aggregate privacy report of a real exported package. Complete distributable Swift-package notices, the authoritative Apache Thrift NOTICE applicable to `thrift-swift`, legal approval, App Review approval, real provisioning, CloudKit production schema, and export encryption classification remain unavailable. #49 and public distribution remain blocked. #51 does not start.

The required live Xcode health recheck still failed before any archive attempt:

```text
rtk err xcode-offload doctor --root "$(external-ssd-root)" --require-shims --strict
FAIL Cache sparsebundle is not readable by hdiutil: hdiutil: imageinfo failed - image not recognized
exit 1
```

No Xcode build, simulator gate, archive, export, upload, or external-system mutation followed this environment failure.

#### 2026-07-14 #49 resolved-versus-shipped dependency correction and environment diagnosis

[CORRECTION] I previously gave an unverified or speculative answer. It should have been labeled, and here is the corrected version. The package-resolution audit treated the missing `thrift-swift` NOTICE as a public-distribution blocker. The pinned OpenTelemetry manifest shows `thrift-swift` is a dependency of its `JaegerExporter` target. Orlix requests `OpenTelemetryProtocolExporterHTTP`, `OpenTelemetryApi`, and `OpenTelemetrySdk`, not `JaegerExporter`. A resolved package is not proof that its unused target ships. The notice blocker is therefore the still-unverified shipped Swift target closure, not specifically `thrift-swift`.

- Changed `package_license_notice_status` to `blocked_shipped_dependency_notice_inventory_unverified`.
- Updated the privacy and license audit to require a healthy build or link-map-derived shipped product closure before producing the distributable notice set.
- Reproduced the Xcode health failure and narrowed it to the four system-scope sparsebundle mounts. User-scope DeviceSet, DerivedData, and Archives mounts pass.
- `xcode-offload mounts repair --scope system --dry-run --verbose` initially stopped because the read-only iOS 26.5 runtime mount had a stale six-hour `SimLaunchHost.arm64` holder. No simulator was booted. After shutting down simulator state, terminating only that holder, and detaching the read-only runtime mount, the configured repair dry run completed successfully.
- The installed system LaunchDaemon still fails before mounting because it cannot create a timestamped directory below the root-owned external `Xcode/SystemBackups/mounts` path. A direct configured system repair requires user-authorized sudo; non-interactive sudo is unavailable in this session.

Evidence:

```text
rtk proxy xcode-offload mounts status --root "$(external-ssd-root)" --scope all --json
user DeviceSet, DerivedData, and Archives checks PASS
system Caches, Images, Volumes, and XcodeApps mount checks FAIL

rtk proxy lsof +f -- /Library/Developer/CoreSimulator/Volumes/iOS_23F77
SimLaunchHost.arm64 was the only holder

rtk proxy xcrun simctl shutdown all
rtk proxy kill -TERM <stale SimLaunchHost pid>
rtk proxy diskutil unmount /Library/Developer/CoreSimulator/Volumes/iOS_23F77
Volume iOS 26.5 Simulator unmounted

rtk proxy xcode-offload mounts repair --root "$(external-ssd-root)" --scope system --dry-run --verbose
exit 0, complete configured repair plan emitted

rtk proxy sudo -n true
sudo: password required
exit 1

rtk proxy launchctl kickstart -k system/io.github.rudironsoni.xcode-offload.mounts-system
exit 0

rtk err xcode-offload doctor --root "$(external-ssd-root)" --require-shims --strict
FAIL Cache sparsebundle is not readable by hdiutil: hdiutil: imageinfo failed - image not recognized
exit 1
```

No runtime image, simulator device, sparsebundle, or repository product source was deleted or replaced. A real build, link map, archive, signed entitlement check, and exported privacy inspection remain unavailable until the system-scope repair runs with user authorization.
