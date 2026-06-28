# TestFlight Beta Checklist

This checklist gates the first Orlix TestFlight beta. It is scoped to publishing the existing iOS app with the current OrlixOS payload and an honest runtime boundary. It is not a claim that OCI environments, upstream package conformance, or the full package ladder are complete.

## Source Of Truth

- Xcode project source: `project.yml`.
- Generated project: `Orlix.xcodeproj`, local output only.
- App target and archive scheme: `Orlix`.
- Release profile: `PROFILE=release`.
- Archive output: `Build/Release/Orlix.xcarchive`.
- Export output: `Build/Release/Export`.

Do not commit generated `.xcodeproj`, DerivedData, archives, `.ipa` files, or other build products.

## Environment

Use the external SSD-backed Xcode wrapper path:

```sh
export PATH="$HOME/.local/bin:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"
```

Use one simulator for beta validation:

```text
iPhone 17, iOS 26.5
UDID: E65F0D05-980C-4368-8CDC-2D2BF3E05757
```

If multiple simulators are booted, shut down all except the chosen simulator before app-hosted validation.

## Local Gate

Run prerequisite checks:

```sh
brew bundle check --file Brewfile
xcode-storage-doctor
make beta-prerequisites
```

Check local signing/keychain access before spending time on a full archive:

```sh
make beta-signing-diagnostics ORLIX_CODE_SIGN_IDENTITY="Apple Distribution"
```

Run the focused first-beta simulator gate:

```sh
make beta-install-simulator
make beta-simulator-gate
```

`beta-install-simulator` builds the Release simulator app, uninstalls any stale `com.rudironsoni.Orlix` app and the legacy `com.rudironsoni.OrlixTerminal` or `org.orlix.OrlixTerminal` install on the chosen simulator, installs the current build, verifies the installed OrlixOS payload metadata selects the direct release root, and launches the app.

`beta-simulator-gate` runs the minimum app-hosted beta proof on the chosen simulator:

- OrlixOS payload metadata resolves through target-derived metadata.
- OrlixOS root image descriptors resolve through target-derived metadata.
- The Linux PTY carries interactive shell input and output.
- OCI MVP proof: a materialized OCI-derived root binds descriptor execution defaults through the OrlixOS terminal session path.

For app-hosted failures, inspect simulator and app crash reports before changing project code.

## Archive

Archive and validate the app bundle:

```sh
make beta-archive ORLIX_DEVELOPMENT_TEAM=<Apple team id>
make beta-validate-archive
```

Automatic signing is the default. For a manually managed App Store/TestFlight profile, pass generic Xcode signing settings through the beta target instead of editing `project.yml`:

```sh
make beta-archive \
  ORLIX_DEVELOPMENT_TEAM=<Apple team id> \
  ORLIX_CODE_SIGN_STYLE=Manual \
  ORLIX_CODE_SIGN_IDENTITY="Apple Distribution" \
  ORLIX_PROVISIONING_PROFILE_SPECIFIER="<profile name>"
```

Signing, App Store Connect app registration, provisioning, account access, and keychain private-key access are external Apple state. If archive signing fails there, record the exact Apple blocker instead of changing Orlix runtime code.

Automatic signing asks Xcode to create or download provisioning profiles by default through `ORLIX_ALLOW_PROVISIONING_UPDATES=YES`. If the local Xcode account cannot create profiles, provide App Store Connect API key credentials through local-only paths:

```sh
make beta-archive \
  ORLIX_DEVELOPMENT_TEAM=<Apple team id> \
  ORLIX_ASC_API_KEY_PATH=<local AuthKey_XXXX.p8 path> \
  ORLIX_ASC_API_KEY_ID=<key id> \
  ORLIX_ASC_API_ISSUER_ID=<issuer id>
```

Do not commit App Store Connect private keys or provisioning profiles.

## Upload

If `make beta-archive` succeeds, upload with Xcode Organizer or export using the archive at:

```text
Build/Release/Orlix.xcarchive
```

To export from the command line, provide an App Store export options plist:

```sh
make beta-export-archive ORLIX_BETA_EXPORT_OPTIONS_PLIST=<ExportOptions.plist>
```

The export target also honors `ORLIX_ALLOW_PROVISIONING_UPDATES` and the `ORLIX_ASC_API_*` variables above.

An App Store Connect export template is available at:

```text
docs/release/ExportOptions-AppStore.plist.example
```

The default export directory is:

```text
Build/Release/Export
```

## Beta Claims

The first beta may claim:

- Orlix is packaged as the iOS host app.
- The app consumes the `OrlixOS` delivered Kit.
- The release archive embeds the expected OrlixOS and OrlixKernel frameworks.
- The bundled release OrlixOS payload is present in the archive.
- The selected app-hosted smoke tests passed on the single simulator used.
- OCI MVP support exists for a materialized OCI-derived root executing descriptor defaults through the OrlixOS terminal session, when `make beta-simulator-gate` passes.

The first beta must not claim:

- Full Linux distribution readiness.
- Full OCI runtime support beyond the focused materialized-root MVP gate.
- Full upstream Linux, OrlixMLibC, or Coreutils conformance.
- Package ladder completion beyond what current evidence proves.
- Runtime behavior from archive packaging evidence alone.

## Post-Beta Catalog

Do after TestFlight publication, not before the first beta unless one item blocks archive, upload, launch, payload inclusion, or honest beta readiness:

- Complete OCI pull, import, materialize, create, start, exec, kill, wait, state, delete, and healthcheck flows through Linux-owned behavior.
- Complete Linux-owned namespaces, cgroups, devices, mounts, networking, signals, process controls, and lifecycle records for OCI-derived environments.
- Expand upstream proof across Linux kselftest, OrlixMLibC tests, and Coreutils.
- Progress the package ladder through jq, curl, and zsh.
- Improve terminal polish, first-run state, diagnostics, and beta UX without exposing private Linux management APIs.
