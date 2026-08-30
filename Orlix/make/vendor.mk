# Native vendor build implementation. Public entry point: make build type=vendor vendor=<selector>.
.ONESHELL:

.PHONY: __vendor-all __vendor-ghostty __vendor-ssh __vendor-tssh __vendor-build

__vendor-all: VENDOR_COMMAND := all
__vendor-all: __vendor-build

__vendor-ghostty: VENDOR_COMMAND := ghostty
__vendor-ghostty: __vendor-build

__vendor-ssh: VENDOR_COMMAND := ssh
__vendor-ssh: __vendor-build

__vendor-tssh: VENDOR_COMMAND := tssh
__vendor-tssh: __vendor-build

__vendor-build:
	set -- "$(VENDOR_COMMAND)"
	# Orlix vendor build (GhosttyKit + libssh2/OpenSSL + native TSSH)

	set -euo pipefail

	PROJECT_ROOT="$(PROJECT_DIR)"

	VENDOR_GHOSTTY="$$PROJECT_ROOT/Vendor/libghostty"
	VENDOR_SSH="$$PROJECT_ROOT/Vendor/libssh2"
	VENDOR_TSSH="$$PROJECT_ROOT/Vendor/trzsz-ssh"
	BUILD_DIR_SSH="$$PROJECT_ROOT/.build/ssh"
	BUILD_DIR_TSSH="$$PROJECT_ROOT/.build/tssh"

	OPENSSL_VERSION="3.2.0"
	OPENSSL_SHA256="14c826f07c7e433706fb5c69fa9e25dab95684844b4c962a2cf1bf183eb4690e"
	LIBSSH2_VERSION="1.11.1"
	LIBSSH2_SHA256="d9ec76cbe34db98eec3539fe2c899d26b0c837cb3eb466a56b0f109cabf658f7"
	TSSH_VERSION="0.2.2"
	TSSH_SOURCE_REVISION="bd9e777620a2ec40e7be59ec31e8a008ff5f7fa5"
	TSSH_XCFRAMEWORK_SHA256="8290485b258da18bf5895e2ecc1c6c30d8050e6385a187a058af21402c21172c"
	TSSH_XCFRAMEWORK_URL="https://github.com/kitknox/trzsz-ssh-rootshell/releases/download/v$${TSSH_VERSION}/TrzszSSH.xcframework.zip"
	TSSH_VPN_XCFRAMEWORK_SHA256="b998d313f5341db98c2b67217ce51ab46049fcb4c99c53cd6ac8f00fdd18fb0f"
	TSSH_VPN_XCFRAMEWORK_URL="https://github.com/kitknox/trzsz-ssh-rootshell/releases/download/v$${TSSH_VERSION}/VPNTunnel.xcframework.zip"
	MACOS_DEPLOYMENT_TARGET="13.3"
	IOS_DEPLOYMENT_TARGET="16.0"

	GHOSTTY_REPO="https://github.com/wiedymi/ghostty.git"
	GHOSTTY_REF="$${GHOSTTY_REF:-02af5158c76036291183e746d436eb8f15356662}"
	BUNDLE_ID="com.rudironsoni.orlix"

	KEEP_WORKDIR="$${KEEP_WORKDIR:-0}"
	GHOSTTY_WORKDIR=""

	RED='\033[0;31m'
	GREEN='\033[0;32m'
	YELLOW='\033[1;33m'
	BLUE='\033[0;34m'
	NC='\033[0m'

	log_info() { echo -e "$${GREEN}[INFO]$${NC} $$1"; }
	log_warn() { echo -e "$${YELLOW}[WARN]$${NC} $$1"; }
	log_error() { echo -e "$${RED}[ERROR]$${NC} $$1"; }
	log_section() { echo -e "\n$${BLUE}==== $$1 ====$${NC}\n"; }


	require_cmd() {
	    if ! command -v "$$1" >/dev/null 2>&1; then
	        log_error "Missing dependency: $$1"
	        exit 1
	    fi
	}

	check_deps_ghostty() {
	    require_cmd git
	    require_cmd zig
	    require_cmd xcodebuild
	    require_cmd perl
	    require_cmd rsync
	}

	check_deps_ssh() {
	    require_cmd curl
	    require_cmd shasum
	    require_cmd tar
	    require_cmd cmake
	    require_cmd make
	    require_cmd xcrun
	}

	check_deps_tssh() {
	    require_cmd curl
	    require_cmd shasum
	    require_cmd ditto
	    require_cmd plutil
	    require_cmd otool
	}

	audit_tssh() {
	    local framework="$$VENDOR_TSSH/TrzszSSH.xcframework"
	    local vpn_framework="$$VENDOR_TSSH/VPNTunnel.xcframework"
	    local artifact_manifest="$$PROJECT_ROOT/Vendor/native-artifacts.sha256"
	    [ -f "$$artifact_manifest" ] || {
	        log_error "Missing tracked native artifact manifest: $$artifact_manifest"
	        exit 1
	    }
	    local expected_tssh_artifacts actual_tssh_artifacts
	    expected_tssh_artifacts="$$(awk '$$2 ~ /^Vendor\/trzsz-ssh\// { print $$2 }' "$$artifact_manifest" | sort)"
	    actual_tssh_artifacts="$$(
	        cd "$$PROJECT_ROOT"
	        find Vendor/trzsz-ssh -type f -name '*.a' -print | sort
	    )"
	    [ -n "$$expected_tssh_artifacts" ] &&
	    [ "$$actual_tssh_artifacts" = "$$expected_tssh_artifacts" ] || {
	        log_error "Installed TSSH archives do not match the tracked artifact inventory"
	        exit 1
	    }
	    (
	        cd "$$PROJECT_ROOT"
	        awk '$$2 ~ /^Vendor\/trzsz-ssh\// { print }' "$$artifact_manifest" |
	            shasum -a 256 -c - >/dev/null
	    ) || {
	        log_error "Installed TSSH archive hash does not match the tracked artifact manifest"
	        exit 1
	    }
	    local info="$$framework/Info.plist"
	    [ -f "$$info" ] || { log_error "Missing TSSH XCFramework metadata: $$info"; exit 1; }
	    plutil -lint "$$info" >/dev/null
	    local device_lib="$$framework/ios-arm64/libTrzszSSH.a"
	    local simulator_lib="$$framework/ios-arm64_x86_64-simulator/libTrzszSSH.a"
	    [ -s "$$device_lib" ] || { log_error "Missing TSSH iOS archive: $$device_lib"; exit 1; }
	    [ -s "$$simulator_lib" ] || { log_error "Missing TSSH simulator archive: $$simulator_lib"; exit 1; }
	    local device_minimum_versions simulator_minimum_versions
	    device_minimum_versions="$$(otool -l "$$device_lib" | awk '$$1 == "minos" { print $$2 }' | sort -u)"
	    simulator_minimum_versions="$$(otool -l "$$simulator_lib" | awk '$$1 == "minos" { print $$2 }' | sort -u)"
	    [ "$$device_minimum_versions" = "13.0" ] || {
	        log_error "Unexpected TSSH iOS minimum versions: $$device_minimum_versions"
	        exit 1
	    }
	    [ "$$simulator_minimum_versions" = "$$(printf '13.0\n14.0')" ] || {
	        log_error "Unexpected TSSH simulator minimum versions: $$simulator_minimum_versions"
	        exit 1
	    }
	    local vpn_info="$$vpn_framework/Info.plist"
	    [ -f "$$vpn_info" ] || { log_error "Missing VPN XCFramework metadata: $$vpn_info"; exit 1; }
	    plutil -lint "$$vpn_info" >/dev/null
	    local vpn_device_lib="$$vpn_framework/ios-arm64/libVPNTunnel.a"
	    local vpn_simulator_lib="$$vpn_framework/ios-arm64_x86_64-simulator/libVPNTunnel.a"
	    [ -s "$$vpn_device_lib" ] || { log_error "Missing VPN iOS archive: $$vpn_device_lib"; exit 1; }
	    [ -s "$$vpn_simulator_lib" ] || { log_error "Missing VPN simulator archive: $$vpn_simulator_lib"; exit 1; }
	    local vpn_device_minimum_versions vpn_simulator_minimum_versions
	    vpn_device_minimum_versions="$$(otool -l "$$vpn_device_lib" | awk '$$1 == "minos" { print $$2 }' | sort -u)"
	    vpn_simulator_minimum_versions="$$(otool -l "$$vpn_simulator_lib" | awk '$$1 == "minos" { print $$2 }' | sort -u)"
	    [ "$$vpn_device_minimum_versions" = "13.0" ] || {
	        log_error "Unexpected VPN iOS minimum versions: $$vpn_device_minimum_versions"
	        exit 1
	    }
	    [ "$$vpn_simulator_minimum_versions" = "$$(printf '13.0\n14.0')" ] || {
	        log_error "Unexpected VPN simulator minimum versions: $$vpn_simulator_minimum_versions"
	        exit 1
	    }
	    local source_revision
	    source_revision="$$(sed -n 's/^source_revision=//p' "$$VENDOR_TSSH/VERSION" 2>/dev/null)"
	    [ "$$source_revision" = "$$TSSH_SOURCE_REVISION" ] || {
	        log_error "Unexpected TSSH source revision: $$source_revision"
	        exit 1
	    }
	}

	build_tssh() {
	    log_section "TrzszSSH"
	    if [ -f "$$VENDOR_TSSH/VERSION" ] &&
	       grep -qx "version=$$TSSH_VERSION" "$$VENDOR_TSSH/VERSION" &&
	       grep -qx "source_revision=$$TSSH_SOURCE_REVISION" "$$VENDOR_TSSH/VERSION" &&
	       grep -qx "archive_sha256=$$TSSH_XCFRAMEWORK_SHA256" "$$VENDOR_TSSH/VERSION" &&
	       grep -qx "vpn_archive_sha256=$$TSSH_VPN_XCFRAMEWORK_SHA256" "$$VENDOR_TSSH/VERSION" &&
	       [ -d "$$VENDOR_TSSH/VPNTunnel.xcframework" ]; then
	        audit_tssh
	        log_info "TrzszSSH v$$TSSH_VERSION is ready"
	        return
	    fi

	    mkdir -p "$$BUILD_DIR_TSSH"
	    local archive="$$BUILD_DIR_TSSH/TrzszSSH-v$${TSSH_VERSION}.xcframework.zip"
	    local vpn_archive="$$BUILD_DIR_TSSH/VPNTunnel-v$${TSSH_VERSION}.xcframework.zip"
	    download_verified_archive "$$TSSH_XCFRAMEWORK_URL" "$$archive" "$$TSSH_XCFRAMEWORK_SHA256"
	    download_verified_archive "$$TSSH_VPN_XCFRAMEWORK_URL" "$$vpn_archive" "$$TSSH_VPN_XCFRAMEWORK_SHA256"

	    local stage
	    stage="$$(mktemp -d "/tmp/orlix-tssh.XXXXXX")"
	    ditto -x -k "$$archive" "$$stage"
	    ditto -x -k "$$vpn_archive" "$$stage"
	    [ -d "$$stage/TrzszSSH.xcframework" ] || {
	        rm -rf "$$stage"
	        log_error "Downloaded TSSH archive did not contain TrzszSSH.xcframework"
	        exit 1
	    }
	    [ -d "$$stage/VPNTunnel.xcframework" ] || {
	        rm -rf "$$stage"
	        log_error "Downloaded VPN archive did not contain VPNTunnel.xcframework"
	        exit 1
	    }
	    mkdir -p "$$VENDOR_TSSH"
	    rm -rf "$$VENDOR_TSSH/TrzszSSH.xcframework" "$$VENDOR_TSSH/VPNTunnel.xcframework"
	    mv "$$stage/TrzszSSH.xcframework" "$$VENDOR_TSSH/"
	    mv "$$stage/VPNTunnel.xcframework" "$$VENDOR_TSSH/"
	    rm -rf "$$stage"
	    printf 'version=%s\nsource_revision=%s\narchive_sha256=%s\nvpn_archive_sha256=%s\n' \
	        "$$TSSH_VERSION" "$$TSSH_SOURCE_REVISION" "$$TSSH_XCFRAMEWORK_SHA256" "$$TSSH_VPN_XCFRAMEWORK_SHA256" \
	        > "$$VENDOR_TSSH/VERSION"
	    audit_tssh
	    log_info "TrzszSSH v$$TSSH_VERSION ready"
	}

	strip_lib() {
	    local lib="$$1"
	    if command -v xcrun >/dev/null 2>&1; then
	        xcrun strip -S -x "$$lib" || strip -S -x "$$lib"
	    else
	        strip -S -x "$$lib"
	    fi
	}

	build_ghosttykit() {
	    log_section "GhosttyKit"

	    if [[ ! "$$GHOSTTY_REF" =~ ^[0-9a-f]{40}$$ ]]; then
	        log_error "GHOSTTY_REF must be a full 40-character commit, got: $$GHOSTTY_REF"
	        exit 1
	    fi

	    GHOSTTY_WORKDIR="$$(mktemp -d "/tmp/ghosttykit.XXXXXX")"
	    local workdir="$$GHOSTTY_WORKDIR"

	    log_info "Fetching ghostty @ $${GHOSTTY_REF}..."
	    git init "$${workdir}/ghostty"
	    git -C "$${workdir}/ghostty" remote add origin "$${GHOSTTY_REPO}"
	    git -C "$${workdir}/ghostty" fetch --filter=blob:none --depth 1 origin "$${GHOSTTY_REF}"
	    git -C "$${workdir}/ghostty" checkout --detach FETCH_HEAD

	    local embedded_path="$${workdir}/ghostty/src/apprt/embedded.zig"
	    if [ -f "$${embedded_path}" ]; then
	        log_info "Disabling Ghostty window blur (App Store safe)..."
	        python3 - <<PY
	from pathlib import Path

	path = Path("$${embedded_path}")
	text = path.read_text()
	start_marker = "    /// Sets the window background blur on macOS to the desired value."
	end_marker = '    extern "c" fn CGSDefaultConnectionForThread() *anyopaque;\n'
	if start_marker not in text or end_marker not in text: raise SystemExit("Ghostty private blur block not found; aborting.")
	start = text.index(start_marker)
	end = text.index(end_marker, start) + len(end_marker)
	new = "    /// Sets the window background blur on macOS to the desired value.\n    /// App Store builds must avoid non-public APIs; keep this as a no-op.\n    export fn ghostty_set_window_background_blur(\n        app: *App,\n        window: *anyopaque,\n    ) void {\n        _ = app;\n        _ = window;\n    }\n"
	path.write_text(text[:start] + new + text[end:])
	PY
	    fi

	    # Patch to link Metal frameworks (same as orlix)
	    if [ -f "$${workdir}/ghostty/pkg/macos/build.zig" ]; then
	        perl -0pi -e 's/lib\.linkFramework\("IOSurface"\);/lib.linkFramework("IOSurface");\n    lib.linkFramework("Metal");\n    lib.linkFramework("MetalKit");/g' "$${workdir}/ghostty/pkg/macos/build.zig"
	        perl -0pi -e 's/module\.linkFramework\("IOSurface", \.\{\}\);/module.linkFramework("IOSurface", .{});\n        module.linkFramework("Metal", .{});\n        module.linkFramework("MetalKit", .{});/g' "$${workdir}/ghostty/pkg/macos/build.zig"
	    fi

	    # IOSurfaceLayer fixes live in the Ghostty fork; no local patching here.

	    # Patch bundle ID to use Orlix's instead of Ghostty's
	    sed -i '' "s/com\\.mitchellh\\.ghostty/$${BUNDLE_ID}/g" "$${workdir}/ghostty/src/build_config.zig"

	    # Lower iOS minimum to match app deployment target
	    perl -0pi -e 's@// iOS [0-9]+ picked arbitrarily@// iOS 16 matches app deployment target@' "$${workdir}/ghostty/src/build/Config.zig"
	    perl -0pi -e 's/\\.ios => \\.\\{ \\.semver = \\.\\{\\n\\s*\\.major = [0-9]+,\\n\\s*\\.minor = [0-9]+,\\n\\s*\\.patch = [0-9]+,\\n\\s*\\} \\},/\\.ios => .{ .semver = .{\\n            .major = 16,\\n            .minor = 0,\\n            .patch = 0,\\n        } },/s' "$${workdir}/ghostty/src/build/Config.zig"

	    log_info "Building GhosttyKit.xcframework..."

	    local zig_flags=(
	        '-Dapp-runtime=none'
	        '-Demit-xcframework=true'
	        '-Demit-macos-app=false'
	        '-Demit-exe=false'
	        '-Demit-docs=false'
	        '-Demit-webdata=false'
	        '-Demit-helpgen=false'
	        '-Demit-terminfo=false'
	        '-Demit-termcap=false'
	        '-Demit-themes=false'
	        '-Doptimize=ReleaseFast'
	        '-Dstrip'
	        '-Dxcframework-target=universal'
	    )

	    (cd "$${workdir}/ghostty" && zig build "$${zig_flags[@]}" -p "$${workdir}/zig-out")

	    local xcframework="$${workdir}/ghostty/macos/GhosttyKit.xcframework"
	    if [ ! -d "$${xcframework}" ]; then
	        log_error "$${xcframework} not found"
	        exit 1
	    fi

	    local macos_lib
	    local ios_lib
	    local sim_lib
	    macos_lib=$$(find "$${xcframework}" -path "*/macos-*/*ghostty*.a" -type f -print -quit)
	    ios_lib=$$(find "$${xcframework}" -path "*/ios-arm64/libghostty*.a" -type f -print -quit)
	    sim_lib=$$(find "$${xcframework}" -path "*/ios-arm64-simulator/libghostty*.a" -type f -print -quit)

	    if [ -z "$${macos_lib}" ] || [ -z "$${ios_lib}" ] || [ -z "$${sim_lib}" ]; then
	        log_error "Failed to locate libghostty.a inside xcframework"
	        exit 1
	    fi

	    mkdir -p "$${VENDOR_GHOSTTY}/lib" "$${VENDOR_GHOSTTY}/ios/lib" "$${VENDOR_GHOSTTY}/ios-simulator/lib"
	    cp "$${macos_lib}" "$${VENDOR_GHOSTTY}/lib/libghostty.a"
	    cp "$${ios_lib}" "$${VENDOR_GHOSTTY}/ios/lib/libghostty.a"
	    cp "$${sim_lib}" "$${VENDOR_GHOSTTY}/ios-simulator/lib/libghostty.a"

	    if [ -d "$${workdir}/ghostty/include" ]; then
	        mkdir -p "$${VENDOR_GHOSTTY}/include" "$${VENDOR_GHOSTTY}/ios/include" "$${VENDOR_GHOSTTY}/ios-simulator/include"
	        rsync -a --exclude='module.modulemap' "$${workdir}/ghostty/include/" "$${VENDOR_GHOSTTY}/include/"
	        rsync -a --exclude='module.modulemap' "$${workdir}/ghostty/include/" "$${VENDOR_GHOSTTY}/ios/include/"
	        rsync -a --exclude='module.modulemap' "$${workdir}/ghostty/include/" "$${VENDOR_GHOSTTY}/ios-simulator/include/"
	    fi

	    rm -rf "$${VENDOR_GHOSTTY}/GhosttyKit.xcframework"
	    rsync -a "$${xcframework}" "$${VENDOR_GHOSTTY}/"

	    printf "%s\n" "$$(git -C "$${workdir}/ghostty" rev-parse HEAD)" > "$${VENDOR_GHOSTTY}/VERSION"

	    strip_lib "$${VENDOR_GHOSTTY}/lib/libghostty.a"
	    strip_lib "$${VENDOR_GHOSTTY}/ios/lib/libghostty.a"
	    strip_lib "$${VENDOR_GHOSTTY}/ios-simulator/lib/libghostty.a"

	    # Also strip static libs inside the xcframework to stay under GitHub size limits.
	    while IFS= read -r -d '' lib; do
	        strip_lib "$${lib}"
	    done < <(find "$${VENDOR_GHOSTTY}/GhosttyKit.xcframework" -name "*.a" -type f -print0)

	    log_info "GhosttyKit done"
	    log_info "  macOS: $$(ls -lh "$${VENDOR_GHOSTTY}/lib/libghostty.a" | awk '{print $$5}')"
	    log_info "  iOS: $$(ls -lh "$${VENDOR_GHOSTTY}/ios/lib/libghostty.a" | awk '{print $$5}')"
	    log_info "  iOS Simulator: $$(ls -lh "$${VENDOR_GHOSTTY}/ios-simulator/lib/libghostty.a" | awk '{print $$5}')"

	    if [ "$${KEEP_WORKDIR}" = "1" ]; then
	        log_warn "Keeping workdir: $${workdir}"
	    else
	        rm -rf "$${workdir}"
	        GHOSTTY_WORKDIR=""
	    fi
	}

	# ---------- libssh2 / OpenSSL ----------

	download_verified_archive() {
	    local url="$$1"
	    local archive="$$2"
	    local expected_sha256="$$3"

	    if [ ! -f "$$archive" ]; then
	        local partial="$${archive}.partial"
	        rm -f "$$partial"
	        curl --fail --location --retry 3 --output "$$partial" "$$url"
	        mv "$$partial" "$$archive"
	    fi

	    local actual_sha256
	    actual_sha256="$$(shasum -a 256 "$$archive" | awk '{print $$1}')"
	    if [ "$$actual_sha256" != "$$expected_sha256" ]; then
	        log_error "SHA-256 mismatch for $$archive: expected $$expected_sha256, got $$actual_sha256"
	        exit 1
	    fi
	}

	download_sources() {
	    mkdir -p "$${BUILD_DIR_SSH}"
	    cd "$${BUILD_DIR_SSH}"

	    local openssl_archive="openssl-$${OPENSSL_VERSION}.tar.gz"
	    download_verified_archive \
	        "https://www.openssl.org/source/$${openssl_archive}" \
	        "$$openssl_archive" \
	        "$$OPENSSL_SHA256"

	    if [ ! -d "openssl-$${OPENSSL_VERSION}" ]; then
	        log_info "Extracting OpenSSL $${OPENSSL_VERSION}..."
	        tar xzf "$$openssl_archive"
	    fi

	    local libssh2_archive="libssh2-$${LIBSSH2_VERSION}.tar.gz"
	    download_verified_archive \
	        "https://www.libssh2.org/download/$${libssh2_archive}" \
	        "$$libssh2_archive" \
	        "$$LIBSSH2_SHA256"

	    if [ ! -d "libssh2-$${LIBSSH2_VERSION}" ]; then
	        log_info "Extracting libssh2 $${LIBSSH2_VERSION}..."
	        tar xzf "$$libssh2_archive"
	    fi
	}

	build_openssl_macos() {
	    log_info "Building OpenSSL for macOS arm64..."
	    cd "$${BUILD_DIR_SSH}/openssl-$${OPENSSL_VERSION}"

	    make clean 2>/dev/null || true

	    local mac_sdk
	    mac_sdk=$$(xcrun --sdk macosx --show-sdk-path)
	    export MACOSX_DEPLOYMENT_TARGET="$${MACOS_DEPLOYMENT_TARGET}"
	    export CC="$$(xcrun --sdk macosx -f clang) -isysroot $${mac_sdk} -mmacosx-version-min=$${MACOS_DEPLOYMENT_TARGET}"

	    ./Configure darwin64-arm64-cc \
	        --prefix="$${BUILD_DIR_SSH}/openssl-macos" \
	        no-shared \
	        no-tests

	    make -j"$$(sysctl -n hw.ncpu)"
	    make install_sw

	    unset MACOSX_DEPLOYMENT_TARGET CC
	}

	build_openssl_ios() {
	    log_info "Building OpenSSL for iOS arm64..."
	    cd "$${BUILD_DIR_SSH}/openssl-$${OPENSSL_VERSION}"

	    make clean 2>/dev/null || true

	    local ios_sdk
	    ios_sdk=$$(xcrun --sdk iphoneos --show-sdk-path)
	    export CROSS_TOP="$$(xcrun --sdk iphoneos --show-sdk-platform-path)/Developer"
	    export CROSS_SDK="iPhoneOS.sdk"
	    export CC="$$(xcrun --sdk iphoneos -f clang) -isysroot $${ios_sdk} -miphoneos-version-min=$${IOS_DEPLOYMENT_TARGET}"

	    ./Configure ios64-xcrun \
	        --prefix="$${BUILD_DIR_SSH}/openssl-ios" \
	        -miphoneos-version-min=$${IOS_DEPLOYMENT_TARGET} \
	        no-shared \
	        no-tests \
	        no-apps

	    make -j"$$(sysctl -n hw.ncpu)" build_libs
	    make install_sw

	    unset CROSS_TOP CROSS_SDK CC
	}

	build_openssl_simulator() {
	    log_info "Building OpenSSL for iOS Simulator arm64..."
	    cd "$${BUILD_DIR_SSH}/openssl-$${OPENSSL_VERSION}"

	    make clean 2>/dev/null || true

	    local sim_sdk
	    sim_sdk=$$(xcrun --sdk iphonesimulator --show-sdk-path)
	    export CROSS_TOP="$$(xcrun --sdk iphonesimulator --show-sdk-platform-path)/Developer"
	    export CROSS_SDK="iPhoneSimulator.sdk"
	    export CC="$$(xcrun --sdk iphonesimulator -f clang) -isysroot $${sim_sdk} -arch arm64 -mios-simulator-version-min=$${IOS_DEPLOYMENT_TARGET}"

	    ./Configure iossimulator-xcrun \
	        --prefix="$${BUILD_DIR_SSH}/openssl-simulator" \
	        -mios-simulator-version-min=$${IOS_DEPLOYMENT_TARGET} \
	        no-shared \
	        no-tests \
	        no-apps

	    make -j"$$(sysctl -n hw.ncpu)" build_libs
	    make install_sw

	    unset CROSS_TOP CROSS_SDK CC
	}

	build_libssh2_macos() {
	    log_info "Building libssh2 for macOS arm64..."
	    cd "$${BUILD_DIR_SSH}/libssh2-$${LIBSSH2_VERSION}"

	    rm -rf build-macos
	    mkdir -p build-macos && cd build-macos

	    cmake .. \
	        -Wno-dev \
	        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	        -DCMAKE_OSX_ARCHITECTURES=arm64 \
	        -DCMAKE_OSX_DEPLOYMENT_TARGET=$${MACOS_DEPLOYMENT_TARGET} \
	        -DCMAKE_INSTALL_PREFIX="$${VENDOR_SSH}/macos" \
	        -DOPENSSL_ROOT_DIR="$${BUILD_DIR_SSH}/openssl-macos" \
	        -DBUILD_SHARED_LIBS=OFF \
	        -DBUILD_EXAMPLES=OFF \
	        -DBUILD_TESTING=OFF

	    make -j"$$(sysctl -n hw.ncpu)"
	    make install

	    cp "$${BUILD_DIR_SSH}/openssl-macos/lib/libssl.a" "$${VENDOR_SSH}/macos/lib/"
	    cp "$${BUILD_DIR_SSH}/openssl-macos/lib/libcrypto.a" "$${VENDOR_SSH}/macos/lib/"
	}

	build_libssh2_ios() {
	    log_info "Building libssh2 for iOS arm64..."
	    cd "$${BUILD_DIR_SSH}/libssh2-$${LIBSSH2_VERSION}"

	    rm -rf build-ios
	    mkdir -p build-ios && cd build-ios

	    local ios_sdk
	    ios_sdk=$$(xcrun --sdk iphoneos --show-sdk-path)

	    cmake .. \
	        -Wno-dev \
	        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	        -DCMAKE_SYSTEM_NAME=iOS \
	        -DCMAKE_OSX_SYSROOT="$${ios_sdk}" \
	        -DCMAKE_OSX_ARCHITECTURES=arm64 \
	        -DCMAKE_OSX_DEPLOYMENT_TARGET=$${IOS_DEPLOYMENT_TARGET} \
	        -DCMAKE_INSTALL_PREFIX="$${VENDOR_SSH}/ios" \
	        -DOPENSSL_ROOT_DIR="$${BUILD_DIR_SSH}/openssl-ios" \
	        -DOPENSSL_INCLUDE_DIR="$${BUILD_DIR_SSH}/openssl-ios/include" \
	        -DOPENSSL_CRYPTO_LIBRARY="$${BUILD_DIR_SSH}/openssl-ios/lib/libcrypto.a" \
	        -DOPENSSL_SSL_LIBRARY="$${BUILD_DIR_SSH}/openssl-ios/lib/libssl.a" \
	        -DBUILD_SHARED_LIBS=OFF \
	        -DBUILD_EXAMPLES=OFF \
	        -DBUILD_TESTING=OFF

	    make -j"$$(sysctl -n hw.ncpu)"
	    make install

	    cp "$${BUILD_DIR_SSH}/openssl-ios/lib/libssl.a" "$${VENDOR_SSH}/ios/lib/"
	    cp "$${BUILD_DIR_SSH}/openssl-ios/lib/libcrypto.a" "$${VENDOR_SSH}/ios/lib/"
	}

	build_libssh2_simulator() {
	    log_info "Building libssh2 for iOS Simulator arm64..."
	    cd "$${BUILD_DIR_SSH}/libssh2-$${LIBSSH2_VERSION}"

	    rm -rf build-simulator
	    mkdir -p build-simulator && cd build-simulator

	    local sim_sdk
	    sim_sdk=$$(xcrun --sdk iphonesimulator --show-sdk-path)

	    cmake .. \
	        -Wno-dev \
	        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	        -DCMAKE_SYSTEM_NAME=iOS \
	        -DCMAKE_OSX_SYSROOT="$${sim_sdk}" \
	        -DCMAKE_OSX_ARCHITECTURES=arm64 \
	        -DCMAKE_OSX_DEPLOYMENT_TARGET=$${IOS_DEPLOYMENT_TARGET} \
	        -DCMAKE_INSTALL_PREFIX="$${VENDOR_SSH}/ios-simulator" \
	        -DOPENSSL_ROOT_DIR="$${BUILD_DIR_SSH}/openssl-simulator" \
	        -DOPENSSL_INCLUDE_DIR="$${BUILD_DIR_SSH}/openssl-simulator/include" \
	        -DOPENSSL_CRYPTO_LIBRARY="$${BUILD_DIR_SSH}/openssl-simulator/lib/libcrypto.a" \
	        -DOPENSSL_SSL_LIBRARY="$${BUILD_DIR_SSH}/openssl-simulator/lib/libssl.a" \
	        -DBUILD_SHARED_LIBS=OFF \
	        -DBUILD_EXAMPLES=OFF \
	        -DBUILD_TESTING=OFF

	    make -j"$$(sysctl -n hw.ncpu)"
	    make install

	    cp "$${BUILD_DIR_SSH}/openssl-simulator/lib/libssl.a" "$${VENDOR_SSH}/ios-simulator/lib/"
	    cp "$${BUILD_DIR_SSH}/openssl-simulator/lib/libcrypto.a" "$${VENDOR_SSH}/ios-simulator/lib/"
	}

	create_modulemap() {
	    log_info "Writing libssh2 module map..."

	    cat > "$${VENDOR_SSH}/module.modulemap" << 'EOF_MODULE'
	module libssh2 {
	    header "include/libssh2.h"
	    header "include/libssh2_sftp.h"
	    header "include/libssh2_publickey.h"
	    link "ssh2"
	    link "ssl"
	    link "crypto"
	    export *
	}
	EOF_MODULE
	}

	build_ssh() {
	    log_section "libssh2 + OpenSSL"
	    download_sources
	    build_openssl_macos
	    build_libssh2_macos
	    build_openssl_ios
	    build_libssh2_ios
	    build_openssl_simulator
	    build_libssh2_simulator
	    create_modulemap

	    log_info "libssh2 done"
	    log_info "  macOS: $$(ls -lh "$${VENDOR_SSH}/macos/lib/libssh2.a" | awk '{print $$5}')"
	    log_info "  iOS: $$(ls -lh "$${VENDOR_SSH}/ios/lib/libssh2.a" | awk '{print $$5}')"
	    log_info "  iOS Simulator: $$(ls -lh "$${VENDOR_SSH}/ios-simulator/lib/libssh2.a" | awk '{print $$5}')"
	}

	COMMAND="$${1:-all}"

	case "$${COMMAND}" in
	    all)
	        check_deps_ghostty
	        check_deps_ssh
	        check_deps_tssh
	        build_ghosttykit
	        build_ssh
	        build_tssh
	        ;;
	    ghostty)
	        check_deps_ghostty
	        build_ghosttykit
	        ;;
	    ssh)
	        check_deps_ssh
	        build_ssh
	        ;;
	    tssh)
	        check_deps_tssh
	        build_tssh
	        ;;
	    *)
	        log_error "Unknown command: $${COMMAND}"
	        exit 1
	        ;;
	esac
