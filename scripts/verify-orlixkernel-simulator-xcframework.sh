#!/usr/bin/env bash
set -euo pipefail

xcframework=""
profile="${PROFILE:-appstore}"
linux_version="${LINUX_VERSION:-6.12}"
linux_arch="${LINUX_ARCH:-orlix}"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --xcframework)
            xcframework="$2"
            shift 2
            ;;
        --profile)
            profile="$2"
            shift 2
            ;;
        --linux-version)
            linux_version="$2"
            shift 2
            ;;
        --linux-arch)
            linux_arch="$2"
            shift 2
            ;;
        *)
            printf 'unknown argument: %s\n' "$1" >&2
            exit 1
            ;;
    esac
done

if [ -z "$xcframework" ]; then
    printf 'usage: %s --xcframework OrlixKernel.xcframework [--profile appstore]\n' "$0" >&2
    exit 1
fi

case "$profile" in
    appstore|development) ;;
    *)
        printf 'unsupported PROFILE=%s (expected one of: appstore development)\n' "$profile" >&2
        exit 1
        ;;
esac

framework="$xcframework/ios-arm64-simulator/OrlixKernel.framework"
payload="$framework/OrlixKernelPayload.bundle"

for required in \
    "$xcframework/Info.plist" \
    "$framework/Info.plist" \
    "$framework/OrlixKernel" \
    "$payload/Info.plist" \
    "$payload/vmlinux" \
    "$payload/vmlinux.sha256" \
    "$payload/selected_profile.txt" \
    "$payload/linux_arch.txt" \
    "$payload/linux_version.txt" \
    "$payload/dtbs/appstore.dtb" \
    "$payload/dtbs/development.dtb"; do
    if [ ! -s "$required" ]; then
        printf 'missing non-empty XCFramework proof input: %s\n' "$required" >&2
        exit 1
    fi
done

plutil -lint "$xcframework/Info.plist" "$framework/Info.plist" "$payload/Info.plist" >/dev/null

stored_profile="$(tr -d '[:space:]' < "$payload/selected_profile.txt")"
stored_arch="$(tr -d '[:space:]' < "$payload/linux_arch.txt")"
stored_version="$(tr -d '[:space:]' < "$payload/linux_version.txt")"

if [ "$stored_profile" != "$profile" ]; then
    printf 'XCFramework payload profile mismatch: expected %s, found %s\n' "$profile" "$stored_profile" >&2
    exit 1
fi
if [ "$stored_arch" != "$linux_arch" ]; then
    printf 'XCFramework payload arch mismatch: expected %s, found %s\n' "$linux_arch" "$stored_arch" >&2
    exit 1
fi
if [ "$stored_version" != "$linux_version" ]; then
    printf 'XCFramework payload Linux version mismatch: expected %s, found %s\n' "$linux_version" "$stored_version" >&2
    exit 1
fi

expected_hash="$(tr -d '[:space:]' < "$payload/vmlinux.sha256")"
actual_hash_line="$(shasum -a 256 "$payload/vmlinux")"
actual_hash="${actual_hash_line%% *}"

if [ "$actual_hash" != "$expected_hash" ]; then
    printf 'XCFramework vmlinux sha256 mismatch: expected %s, found %s\n' "$expected_hash" "$actual_hash" >&2
    exit 1
fi

printf 'verified simulator OrlixKernel XCFramework: %s (profile %s, vmlinux sha256 %s)\n' "$xcframework" "$profile" "$actual_hash"
