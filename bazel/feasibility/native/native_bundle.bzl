"""Pinned native Apple SSH archives. Ghostty comes from libghostty-spm.

OpenSSL and libssh2 are separate actions for each Apple destination and
architecture. libssh2 consumes that slice's OpenSSL outputs. A slice spec is
an input only of that slice, so a device-only input does not rebuild the
simulator archives.
"""

_SLICES = [
    struct(
        destination = "ios-device",
        architecture = "arm64",
        sdk = "iphoneos",
        openssl_target = "ios64-xcrun",
        min_flag = "-miphoneos-version-min=15.0",
        deployment = "15.0",
        system_name = "iOS",
        cross_sdk = "iPhoneOS.sdk",
    ),
    struct(
        destination = "ios-simulator",
        architecture = "arm64",
        sdk = "iphonesimulator",
        openssl_target = "iossimulator-xcrun",
        min_flag = "-mios-simulator-version-min=15.0",
        deployment = "15.0",
        system_name = "iOS",
        cross_sdk = "iPhoneSimulator.sdk",
    ),
    struct(
        destination = "macos",
        architecture = "arm64",
        sdk = "macosx",
        openssl_target = "darwin64-arm64-cc",
        min_flag = "-mmacosx-version-min=13.3",
        deployment = "13.3",
        system_name = "Darwin",
        cross_sdk = "",
    ),
]

_MANIFEST = "\n".join([
    "{",
    '  "cmake": "4.0.3",',
    '  "component": "ssh",',
    '  "ios_minimum": "15.0",',
    '  "libssh2": "1.11.1",',
    '  "macos_minimum": "13.3",',
    '  "openssl": "3.2.0",',
    '  "xcode": "26.6"',
    "}",
    "",
])

def _slice_text(slice):
    return "\n".join([
        "destination=%s" % slice.destination,
        "architecture=%s" % slice.architecture,
        "sdk=%s" % slice.sdk,
        "openssl_target=%s" % slice.openssl_target,
        "min_flag=%s" % slice.min_flag,
        "deployment=%s" % slice.deployment,
        "system_name=%s" % slice.system_name,
        "cross_sdk=%s" % slice.cross_sdk,
        "",
    ])

def _pinned_apple_env(ctx):
    shell = ctx.configuration.default_shell_env
    developer_dir = shell.get("ORLIX_PINNED_DEVELOPER_DIR")
    if not developer_dir:
        fail("native Apple archives require action_env ORLIX_PINNED_DEVELOPER_DIR")
    return {
        "DEVELOPER_DIR": developer_dir,
        "HOME": "/var/empty",
        "PATH": "/usr/bin:/bin",
        "SOURCE_DATE_EPOCH": "1",
    }

def _openssl_action(ctx, slice, spec, ssl, crypto, headers):
    inputs = depset(
        direct = [spec, ctx.file.openssl_configure],
        transitive = [ctx.attr.openssl_source[DefaultInfo].files],
    )
    ctx.actions.run_shell(
        mnemonic = "OrlixOpenSSLAppleArchive",
        progress_message = "Building OpenSSL %s %s" % (slice.destination, slice.architecture),
        command = r"""
set -euo pipefail
unset SDKROOT
exec_root="$PWD"
spec="$exec_root/$1"
openssl_configure="$exec_root/$2"
ssl_out="$exec_root/$3"
crypto_out="$exec_root/$4"
headers_out="$exec_root/$5"
read_field() { /usr/bin/sed -n "s/^$1=//p" "$spec"; }
destination="$(read_field destination)"
architecture="$(read_field architecture)"
sdk="$(read_field sdk)"
openssl_target="$(read_field openssl_target)"
min_flag="$(read_field min_flag)"
cross_sdk="$(read_field cross_sdk)"
case "$destination" in ""|*[!a-z-]*) echo "unsupported destination: $destination" >&2; exit 1 ;; esac
case "$architecture" in arm64|x86_64) ;; *) echo "unsupported architecture: $architecture" >&2; exit 1 ;; esac
case "$sdk" in ""|*[!a-z0-9]*) echo "unsupported sdk: $sdk" >&2; exit 1 ;; esac
case "$openssl_target" in ""|*[!A-Za-z0-9-]*) echo "unsupported openssl target: $openssl_target" >&2; exit 1 ;; esac
case "$min_flag" in -m*-version-min=*) ;; *) echo "unsupported min flag: $min_flag" >&2; exit 1 ;; esac
case "$cross_sdk" in "" ) ;; *[!A-Za-z0-9.]*) echo "unsupported cross sdk: $cross_sdk" >&2; exit 1 ;; esac
test -n "${DEVELOPER_DIR:-}"
xcode_ver="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcodebuild -version)"
xcode_name="$(printf '%s\n' "$xcode_ver" | /usr/bin/sed -n '1p')"
xcode_build="$(printf '%s\n' "$xcode_ver" | /usr/bin/sed -n '2p')"
case "$xcode_name|$xcode_build" in
  "Xcode 27.0|Build version 27A266a"|"Xcode 26.6|Build version 17F113") ;;
  *) echo "unsupported Xcode: $xcode_ver" >&2; exit 1 ;;
esac
test "$(/usr/bin/make --version | /usr/bin/head -n 1)" = "GNU Make 3.81"
test "$(/usr/bin/perl -e 'printf "%vd", $^V')" = "5.34.1"
work="$exec_root/orlix-openssl-$destination-$architecture"
/bin/rm -rf "$work"
trap '/bin/rm -rf "$work"' EXIT
/bin/mkdir -p "$work/home"
export HOME="$work/home"
export SOURCE_DATE_EPOCH=1
openssl_root="$(/usr/bin/dirname "$openssl_configure")"
openssl_src="$work/openssl-src"
openssl_prefix="$work/openssl-install"
/bin/cp -RL "$openssl_root" "$openssl_src"
/bin/chmod -R u+w "$openssl_src"
sdk_path="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcrun --sdk "$sdk" --show-sdk-path)"
clang="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcrun --sdk "$sdk" -f clang)"
(
  cd "$openssl_src"
  export CC="$clang -isysroot $sdk_path -arch $architecture $min_flag"
  if test "$sdk" != macosx; then
    test -n "$cross_sdk"
    export CROSS_TOP="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcrun --sdk "$sdk" --show-sdk-platform-path)/Developer"
    export CROSS_SDK="$cross_sdk"
  fi
  /usr/bin/perl ./Configure "$openssl_target" --prefix="$openssl_prefix" no-shared no-tests no-apps
  /usr/bin/make -j8 build_libs SOURCE_DATE_EPOCH=1
  /usr/bin/make install_sw SOURCE_DATE_EPOCH=1
)
test -s "$openssl_prefix/lib/libssl.a"
test -s "$openssl_prefix/lib/libcrypto.a"
test -s "$openssl_prefix/include/openssl/opensslv.h"
/bin/mkdir -p "$(/usr/bin/dirname "$ssl_out")" "$headers_out"
/bin/cp "$openssl_prefix/lib/libssl.a" "$ssl_out"
/bin/cp "$openssl_prefix/lib/libcrypto.a" "$crypto_out"
/bin/cp -R "$openssl_prefix/include/." "$headers_out/"
/usr/bin/lipo "$ssl_out" -verify_arch "$architecture"
/usr/bin/lipo "$crypto_out" -verify_arch "$architecture"
test -s "$headers_out/openssl/opensslv.h"
""",
        arguments = [
            spec.path,
            ctx.file.openssl_configure.path,
            ssl.path,
            crypto.path,
            headers.path,
        ],
        inputs = inputs,
        outputs = [ssl, crypto, headers],
        env = _pinned_apple_env(ctx),
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )

def _libssh2_action(ctx, slice, spec, ssl, crypto, headers, archive):
    inputs = depset(
        direct = [spec, ctx.file.libssh2_cmake, ssl, crypto, headers],
        transitive = [
            ctx.attr.libssh2_source[DefaultInfo].files,
            ctx.attr.cmake_runtime[DefaultInfo].files,
        ],
    )
    ctx.actions.run_shell(
        mnemonic = "OrlixLibssh2AppleArchive",
        progress_message = "Building libssh2 %s %s" % (slice.destination, slice.architecture),
        command = r"""
set -euo pipefail
unset SDKROOT
exec_root="$PWD"
spec="$exec_root/$1"
libssh2_cmake="$exec_root/$2"
cmake="$exec_root/$3"
ssl_in="$exec_root/$4"
crypto_in="$exec_root/$5"
headers_in="$exec_root/$6"
archive_out="$exec_root/$7"
read_field() { /usr/bin/sed -n "s/^$1=//p" "$spec"; }
destination="$(read_field destination)"
architecture="$(read_field architecture)"
sdk="$(read_field sdk)"
deployment="$(read_field deployment)"
system_name="$(read_field system_name)"
case "$destination" in ""|*[!a-z-]*) echo "unsupported destination: $destination" >&2; exit 1 ;; esac
case "$architecture" in arm64|x86_64) ;; *) echo "unsupported architecture: $architecture" >&2; exit 1 ;; esac
case "$sdk" in ""|*[!a-z0-9]*) echo "unsupported sdk: $sdk" >&2; exit 1 ;; esac
case "$deployment" in [0-9]*.[0-9]*) ;; *) echo "unsupported deployment: $deployment" >&2; exit 1 ;; esac
case "$system_name" in Darwin|iOS) ;; *) echo "unsupported system: $system_name" >&2; exit 1 ;; esac
test -n "${DEVELOPER_DIR:-}"
xcode_ver="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcodebuild -version)"
xcode_name="$(printf '%s\n' "$xcode_ver" | /usr/bin/sed -n '1p')"
xcode_build="$(printf '%s\n' "$xcode_ver" | /usr/bin/sed -n '2p')"
case "$xcode_name|$xcode_build" in
  "Xcode 27.0|Build version 27A266a"|"Xcode 26.6|Build version 17F113") ;;
  *) echo "unsupported Xcode: $xcode_ver" >&2; exit 1 ;;
esac
test "$(/usr/bin/make --version | /usr/bin/head -n 1)" = "GNU Make 3.81"
test "$("$cmake" --version | /usr/bin/head -n 1)" = "cmake version 4.0.3"
test -s "$ssl_in"
test -s "$crypto_in"
test -d "$headers_in"
test -s "$headers_in/openssl/opensslv.h"
work="$exec_root/orlix-libssh2-$destination-$architecture"
/bin/rm -rf "$work"
trap '/bin/rm -rf "$work"' EXIT
/bin/mkdir -p "$work/home" "$work/openssl-prefix/lib" "$work/openssl-prefix/include"
export HOME="$work/home"
export SOURCE_DATE_EPOCH=1
/bin/cp "$ssl_in" "$work/openssl-prefix/lib/libssl.a"
/bin/cp "$crypto_in" "$work/openssl-prefix/lib/libcrypto.a"
/bin/cp -R "$headers_in/." "$work/openssl-prefix/include/"
libssh2_root="$(/usr/bin/dirname "$libssh2_cmake")"
libssh2_src="$work/libssh2-src"
libssh2_build="$work/libssh2-build"
libssh2_install="$work/libssh2-install"
/bin/cp -RL "$libssh2_root" "$libssh2_src"
/bin/chmod -R u+w "$libssh2_src"
sdk_path="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcrun --sdk "$sdk" --show-sdk-path)"
"$cmake" -S "$libssh2_src" -B "$libssh2_build" -G "Unix Makefiles" -Wno-dev \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_MAKE_PROGRAM=/usr/bin/make \
  -DCMAKE_SYSTEM_NAME="$system_name" -DCMAKE_OSX_SYSROOT="$sdk_path" \
  -DCMAKE_OSX_ARCHITECTURES="$architecture" -DCMAKE_OSX_DEPLOYMENT_TARGET="$deployment" \
  -DCMAKE_INSTALL_PREFIX="$libssh2_install" -DOPENSSL_ROOT_DIR="$work/openssl-prefix" \
  -DOPENSSL_INCLUDE_DIR="$work/openssl-prefix/include" \
  -DOPENSSL_CRYPTO_LIBRARY="$work/openssl-prefix/lib/libcrypto.a" \
  -DOPENSSL_SSL_LIBRARY="$work/openssl-prefix/lib/libssl.a" -DCRYPTO_BACKEND=OpenSSL \
  -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF
/usr/bin/make -C "$libssh2_build" -j8
/usr/bin/make -C "$libssh2_build" install
test -s "$libssh2_install/lib/libssh2.a"
/bin/mkdir -p "$(/usr/bin/dirname "$archive_out")"
/bin/cp "$libssh2_install/lib/libssh2.a" "$archive_out"
/usr/bin/lipo "$archive_out" -verify_arch "$architecture"
""",
        arguments = [
            spec.path,
            ctx.file.libssh2_cmake.path,
            ctx.executable.cmake.path,
            ssl.path,
            crypto.path,
            headers.path,
            archive.path,
        ],
        inputs = inputs,
        tools = [ctx.executable.cmake],
        outputs = [archive],
        env = _pinned_apple_env(ctx),
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )

def _ssh_impl(ctx):
    files = []
    groups = {}
    for slice in _SLICES:
        base = "%s/%s/%s" % (ctx.label.name, slice.destination, slice.architecture)
        spec = ctx.actions.declare_file("%s/%s/%s.slice" % (ctx.label.name, slice.destination, slice.architecture))
        ctx.actions.write(spec, _slice_text(slice))
        ssl = ctx.actions.declare_file(base + "/libssl.a")
        crypto = ctx.actions.declare_file(base + "/libcrypto.a")
        headers = ctx.actions.declare_directory(base + "/openssl-include")
        libssh2 = ctx.actions.declare_file(base + "/libssh2.a")
        _openssl_action(ctx, slice, spec, ssl, crypto, headers)
        _libssh2_action(ctx, slice, spec, ssl, crypto, headers, libssh2)
        prefix = slice.destination.replace("-", "_")
        groups[prefix + "_libssl"] = depset([ssl])
        groups[prefix + "_libcrypto"] = depset([crypto])
        groups[prefix + "_libssh2"] = depset([libssh2])
        files.extend([ssl, crypto, libssh2])
    manifest = ctx.actions.declare_file(ctx.label.name + "/manifest.json")
    ctx.actions.write(manifest, _MANIFEST)
    files.append(manifest)
    groups["manifest"] = depset([manifest])
    return [
        DefaultInfo(files = depset(files)),
        OutputGroupInfo(**groups),
    ]

orlix_ssh_archives = rule(
    implementation = _ssh_impl,
    attrs = {
        "openssl_source": attr.label(mandatory = True),
        "openssl_configure": attr.label(allow_single_file = True, mandatory = True),
        "libssh2_source": attr.label(mandatory = True),
        "libssh2_cmake": attr.label(allow_single_file = True, mandatory = True),
        "cmake": attr.label(allow_single_file = True, executable = True, cfg = "exec", mandatory = True),
        "cmake_runtime": attr.label(mandatory = True),
    },
)
