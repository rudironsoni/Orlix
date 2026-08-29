# GitHub release setup

The repository workflows keep release policy in Git. Repository administrators must configure the credentials and approval controls in GitHub before the first run.

## Tag rule

Protect tags that match `ios-v*`. Only release maintainers can create or update these tags. A valid tag must match `ios-v<MARKETING_VERSION>`, point to the checked-out commit, and be on `origin/main`.

## `testflight-internal` environment

Configure these environment variables:

- `APPLE_TEAM_ID`
- `APPSTORE_API_KEY_ID`
- `APPSTORE_ISSUER_ID`
- `TESTFLIGHT_INTERNAL_GROUP`

Configure these environment secrets:

- `APPSTORE_API_PRIVATE_KEY`
- `APPLE_DISTRIBUTION_CERTIFICATE_P12_BASE64`
- `APPLE_DISTRIBUTION_CERTIFICATE_PASSWORD`

The TestFlight group must already exist and must be an internal group.

## `app-store-review` environment

Configure `APPSTORE_API_KEY_ID` and `APPSTORE_ISSUER_ID` as environment variables. Configure `APPSTORE_API_PRIVATE_KEY` as an environment secret. Add required reviewers to this environment when the repository plan supports deployment protection rules.

Production promotion also requires a manual workflow dispatch. The workflow submits the exact processed beta build for App Review and sets `automatic_release` to `false`.

## Required checked-in approvals and assets

Set every public distribution status and `public_distribution_approved` in `docs/sources/release/orlix-app-release-inputs.json` only after review. Add reviewed screenshots under `fastlane/screenshots/<locale>/`. Both workflows fail before external distribution while these inputs are incomplete.
