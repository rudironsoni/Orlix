"""Pinned native Apple SSH archives. Ghostty comes from libghostty-spm."""

def _outputs(ctx, archive):
    return struct(
        device = ctx.actions.declare_file(ctx.label.name + "/ios-device/lib" + archive + ".a"),
        simulator = ctx.actions.declare_file(ctx.label.name + "/ios-simulator/lib" + archive + ".a"),
        macos = ctx.actions.declare_file(ctx.label.name + "/macos/lib" + archive + ".a"),
        manifest = ctx.actions.declare_file(ctx.label.name + "/manifest.json"),
    )

def _result(o):
    files = [o.device, o.simulator, o.macos, o.manifest]
    groups = {
        "ios_device": depset([o.device]),
        "ios_simulator": depset([o.simulator]),
        "macos": depset([o.macos]),
        "manifest": depset([o.manifest]),
    }
    if hasattr(o, "device_resources"):
        files.extend([o.device_resources, o.simulator_resources, o.macos_resources])
        groups["ios_device_resources"] = depset([o.device_resources])
        groups["ios_simulator_resources"] = depset([o.simulator_resources])
        groups["macos_resources"] = depset([o.macos_resources])
    return [
        DefaultInfo(files = depset(files)),
        OutputGroupInfo(**groups),
    ]

def _pinned_apple_env(ctx, extra):
    shell = ctx.configuration.default_shell_env
    developer_dir = shell.get("DEVELOPER_DIR")
    if not developer_dir:
        fail("native Apple archives require action_env DEVELOPER_DIR")
    env = {
        "DEVELOPER_DIR": developer_dir,
        "HOME": "/var/empty",
        "PATH": "/usr/bin:/bin",
    }
    tmpdir = shell.get("TMPDIR")
    if tmpdir:
        env["TMPDIR"] = tmpdir
    env.update(extra)
    return env

def _ssh_impl(ctx):
    o = _outputs(ctx, "orlix_ssh")
    inputs = depset(
        direct = [ctx.file.openssl_configure, ctx.file.libssh2_cmake],
        transitive = [
            ctx.attr.openssl_source[DefaultInfo].files,
            ctx.attr.libssh2_source[DefaultInfo].files,
            ctx.attr.cmake_runtime[DefaultInfo].files,
        ],
    )
    ctx.actions.run_shell(
        mnemonic = "OrlixSshAppleArchives",
        progress_message = "Building pinned OpenSSL and libssh2 Apple archives",
        command = r"""
set -euo pipefail
unset SDKROOT
exec_root="$PWD"
openssl_configure="$exec_root/$1"; libssh2_cmake="$exec_root/$2"; cmake="$exec_root/$3"
device_out="$exec_root/$4"; simulator_out="$exec_root/$5"
macos_out="$exec_root/$6"; manifest_out="$exec_root/$7"
test -n "${DEVELOPER_DIR:-}"
xcode_ver="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcodebuild -version)"
xcode_name="$(printf '%s\n' "$xcode_ver" | /usr/bin/sed -n '1p')"
xcode_build="$(printf '%s\n' "$xcode_ver" | /usr/bin/sed -n '2p')"
case "$xcode_name|$xcode_build" in
  "Xcode 26.6|Build version 17F113"|"Xcode 27.0|Build version 27A5252f") ;;
  *) echo "unsupported Xcode: $xcode_ver" >&2; exit 1 ;;
esac
test "$("$cmake" --version | /usr/bin/head -n 1)" = "cmake version 4.0.3"
test "$(/usr/bin/make --version | /usr/bin/head -n 1)" = "GNU Make 3.81"
test "$(/usr/bin/perl -e 'printf "%vd", $^V')" = "5.34.1"
work="$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/orlix-ssh.XXXXXX")"
trap '/bin/rm -rf "$work"' EXIT
/bin/mkdir -p "$work/home"
export HOME="$work/home"
openssl_root="$(/usr/bin/dirname "$openssl_configure")"
libssh2_root="$(/usr/bin/dirname "$libssh2_cmake")"
build_one() {
    name="$1"; sdk="$2"; target="$3"; min_flag="$4"; deployment="$5"; output="$6"
    openssl_src="$work/openssl-$name"; openssl_prefix="$work/openssl-install-$name"
    libssh2_src="$work/libssh2-$name"; libssh2_build="$work/libssh2-build-$name"; libssh2_install="$work/libssh2-install-$name"
    /bin/cp -R "$openssl_root" "$openssl_src"; /bin/cp -R "$libssh2_root" "$libssh2_src"
    /bin/chmod -R u+w "$openssl_src" "$libssh2_src"
    sdk_path="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcrun --sdk "$sdk" --show-sdk-path)"
    clang="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcrun --sdk "$sdk" -f clang)"
    ( cd "$openssl_src"
      export CC="$clang -isysroot $sdk_path -arch arm64 $min_flag"
      if test "$sdk" != macosx; then
          export CROSS_TOP="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcrun --sdk "$sdk" --show-sdk-platform-path)/Developer"
          if test "$sdk" = iphoneos; then export CROSS_SDK=iPhoneOS.sdk; else export CROSS_SDK=iPhoneSimulator.sdk; fi
      fi
      /usr/bin/perl ./Configure "$target" --prefix="$openssl_prefix" no-shared no-tests no-apps
      /usr/bin/make -j8 build_libs
      /usr/bin/make install_sw )
    system_name=Darwin; if test "$sdk" != macosx; then system_name=iOS; fi
    "$cmake" -S "$libssh2_src" -B "$libssh2_build" -G "Unix Makefiles" -Wno-dev \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_MAKE_PROGRAM=/usr/bin/make \
      -DCMAKE_SYSTEM_NAME="$system_name" -DCMAKE_OSX_SYSROOT="$sdk_path" \
      -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET="$deployment" \
      -DCMAKE_INSTALL_PREFIX="$libssh2_install" -DOPENSSL_ROOT_DIR="$openssl_prefix" \
      -DOPENSSL_INCLUDE_DIR="$openssl_prefix/include" -DOPENSSL_CRYPTO_LIBRARY="$openssl_prefix/lib/libcrypto.a" \
      -DOPENSSL_SSL_LIBRARY="$openssl_prefix/lib/libssl.a" -DCRYPTO_BACKEND=OpenSSL \
      -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF
    /usr/bin/make -C "$libssh2_build" -j8; /usr/bin/make -C "$libssh2_build" install
    /bin/mkdir -p "$(/usr/bin/dirname "$output")"
    /usr/bin/libtool -static -o "$output" "$libssh2_install/lib/libssh2.a" \
      "$openssl_prefix/lib/libssl.a" "$openssl_prefix/lib/libcrypto.a"
    /usr/bin/lipo "$output" -verify_arch arm64
}
build_one ios-device iphoneos ios64-xcrun -miphoneos-version-min=15.0 15.0 "$device_out"
build_one ios-simulator iphonesimulator iossimulator-xcrun -mios-simulator-version-min=15.0 15.0 "$simulator_out"
build_one macos macosx darwin64-arm64-cc -mmacosx-version-min=13.3 13.3 "$macos_out"
/bin/mkdir -p "$(/usr/bin/dirname "$manifest_out")"
/usr/bin/printf '%s\n' '{' '  "cmake": "4.0.3",' '  "component": "ssh",' \
  '  "ios_minimum": "15.0",' '  "libssh2": "1.11.1",' '  "macos_minimum": "13.3",' \
  '  "openssl": "3.2.0",' '  "xcode": "26.6"' '}' > "$manifest_out"
""",
        arguments = [
            ctx.file.openssl_configure.path,
            ctx.file.libssh2_cmake.path,
            ctx.executable.cmake.path,
            o.device.path,
            o.simulator.path,
            o.macos.path,
            o.manifest.path,
        ],
        inputs = inputs,
        tools = [ctx.executable.cmake],
        outputs = [o.device, o.simulator, o.macos, o.manifest],
        env = _pinned_apple_env(ctx, {}),
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )
    return _result(o)

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
