# Apple procedure

Use the task envelope's Make targets for simulator builds, simulator tests, XCTest, archives, exports, TestFlight, and App Store validation. Use Xcode IDE or XcodeBuildMCP only for interactive or stateful diagnosis. Use the XcodeBuildMCP CLI for `doctor`, project scaffolding, one-off discovery, package inspection, and utilities.

Treat each result separately:

- A simulator build does not prove simulator launch or tests.
- XCTest output must identify the destination and `.xcresult` bundle. Inspect failures and attachments through `xcresulttool` or the enabled Xcode workflow.
- After an app-hosted failure or crash, inspect current simulator or app crash reports before changing code.
- Physical-device work requires `make agent-status AREA=orlix-tcti`, `make agent-next AREA=orlix-tcti`, and `make agent-task-envelope-check AREA=orlix-tcti`, current simulator-ladder evidence, and `physical_device_allowed=true`.
- Signing diagnostics, archive, export, upload, TestFlight processing, and App Store validation are separate gates. Preserve current signing prerequisites and never store credentials in repository state.

Keep raw build logs, `.xcresult` bundles, crash reports, device logs, and export logs outside conversational state. Return evidence identity, command, destination, profile, artifact identity, result, and the relevant failure.
