"""Pinned GNU Bash Autotools package from OrlixMLibC sysroot and UAPI."""

load("//bazel:artifact_identity.bzl", "declare_artifact_identity")
load("//bazel/providers:kernel_info.bzl", "OrlixInstalledUapiInfo")
load("//bazel/providers:package_info.bzl", "OrlixPackageTreeInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")

def _pinned_env(ctx):
    shell = ctx.configuration.default_shell_env
    developer_dir = shell.get("ORLIX_PINNED_DEVELOPER_DIR")
    if not developer_dir:
        fail("bash package requires action_env ORLIX_PINNED_DEVELOPER_DIR")
    env = {
        "DEVELOPER_DIR": developer_dir,
        "HOME": "/var/empty",
        "LC_ALL": "C",
        "PYTHONDONTWRITEBYTECODE": "1",
        "CCACHE_CONFIGPATH": "/dev/null",
        "CCACHE_COMPILERCHECK": "content",
        "CCACHE_MAXSIZE": "20G",
        "PATH": "/opt/homebrew/opt/bison/bin:/opt/homebrew/opt/coreutils/libexec/gnubin:/opt/homebrew/opt/lld/bin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/bin:/bin",
    }
    tmpdir = shell.get("TMPDIR")
    if tmpdir:
        env["TMPDIR"] = tmpdir
    return env

def _bash_package_impl(ctx):
    sysroot = ctx.attr.sysroot[OrlixLibcSysrootInfo]
    uapi = ctx.attr.uapi[OrlixInstalledUapiInfo]
    install_tree = ctx.actions.declare_directory(ctx.label.name + "/install")
    file_manifest = ctx.actions.declare_file(ctx.label.name + "/file-manifest.txt")
    license_manifest = ctx.actions.declare_file(ctx.label.name + "/license-manifest.txt")
    metadata = ctx.actions.declare_file(ctx.label.name + "/package-metadata.txt")
    digest = ctx.actions.declare_file(ctx.label.name + "/source-input.sha256")
    script = ctx.actions.declare_file(ctx.label.name + ".sh")
    ctx.actions.write(script, r"""
set -euo pipefail
exec_root="$PWD"
configure="$exec_root/$1"
headers="$exec_root/$2"
uapi_headers="$exec_root/$3"
libraries="$exec_root/$4"
runtime="$exec_root/$5"
install_out="$exec_root/$6"
file_manifest="$exec_root/$7"
license_manifest="$exec_root/$8"
metadata="$exec_root/$9"
digest_out="$exec_root/${10}"
package_name="${11}"
package_version="${12}"
case "$configure" in
  *OrlixOS/Sources/make*|*OrlixCoreUtils/Makefile*|*OrlixKernel/Makefile*|*OrlixMLibC/Makefile*)
    echo "bash must not invoke wrapper Makefiles: $configure" >&2
    exit 1
    ;;
esac
clang="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
ar_bin="$(/usr/bin/command -v llvm-ar)"
ranlib_bin="$(/usr/bin/command -v llvm-ranlib)"
strip_bin="$(/usr/bin/command -v llvm-strip)"
objdump_bin="$(/usr/bin/command -v llvm-objdump)"
test -x "$clang"
test -x "$ar_bin"
test -x "$ranlib_bin"
test -x "$strip_bin"
test -x "$objdump_bin"
test -s "$libraries/libc.a"
test -s "$libraries/crt1.o"
test -s "$libraries/crti.o"
test -s "$libraries/crtn.o"
test -s "$runtime"
work="$ORLIX_PACKAGE_WORK_ROOT"
headers="$work/inputs/headers"
uapi_headers="$work/inputs/uapi"
libraries="$work/inputs/libraries"
runtime="$work/inputs/runtime/libcompiler_rt.a"
/bin/mkdir -p "$work/build" "$work/dest/usr/bin"
launcher="${ORLIX_COMPILER_LAUNCHER-/opt/homebrew/bin/ccache}"
case "$launcher" in
  /opt/homebrew/bin/ccache) test -n "${CCACHE_DIR:-}" || { echo "CCACHE_DIR is required; use the repository Make interface" >&2; exit 1; } ;;
  "") ;;
  *) echo "unsupported compiler launcher: $launcher" >&2; exit 1 ;;
esac
export CCACHE_BASEDIR="$work"
export CCACHE_EXTRAFILES="$exec_root/__COMPILER_IDENTITY__"
export CCACHE_STATSLOG="$work/compiler-cache.log"
: > "$CCACHE_STATSLOG"
export MAKE=/opt/homebrew/bin/gmake
cd "$work/build"
libm="$libraries/libc.a"
libpthread="$libraries/libc.a"
libssp=""
libssp_ns=""
[ -s "$libraries/libm.a" ] && libm="$libraries/libm.a"
[ -s "$libraries/libpthread.a" ] && libpthread="$libraries/libpthread.a"
[ -s "$libraries/libssp.a" ] && libssp="$libraries/libssp.a"
[ -s "$libraries/libssp_nonshared.a" ] && libssp_ns="$libraries/libssp_nonshared.a"
export CC="${launcher:+$launcher }$clang --target=aarch64-linux-gnu -isystem $headers -isystem $uapi_headers/include -D_GNU_SOURCE -fhosted -fno-builtin -ffixed-x18 -fPIE -ffile-prefix-map=$work=."
export CFLAGS="-O2 -Wno-unknown-warning-option -ffile-prefix-map=$work=. -ffile-prefix-map=..=. -ffile-prefix-map=../..=. -ffile-prefix-map=../../..=."
export LDFLAGS="--target=aarch64-linux-gnu -fuse-ld=lld -nostdlib -static-pie -Wl,--gc-sections -Wl,-z,max-page-size=0x4000 $libraries/crt1.o $libraries/crti.o -Wl,--start-group"
export LIBS="$libraries/libc.a $libm $libpthread $libssp_ns $libssp $runtime -Wl,--end-group $libraries/crtn.o"
export AR="$ar_bin"
export RANLIB="$ranlib_bin"
export CC_FOR_BUILD="/usr/bin/xcrun --sdk macosx cc -target arm64-apple-macosx"
export BUILD_CC="$CC_FOR_BUILD"
export bash_cv_getenv_redef=no
export bash_cv_getcwd_malloc=yes
export bash_cv_func_strchrnul_works=yes
if [ "$(/bin/cat "$work/inputs/configure-required")" = 1 ]; then
  "$work/src/configure" --host=aarch64-linux-gnu --build=aarch64-apple-darwin --prefix=/usr --without-bash-malloc --enable-static-link --disable-nls --disable-readline --without-installed-readline --without-curses
fi
/usr/bin/printf '0\n' > .build
/usr/bin/printf 'bash: %s %s %s %s %s %s %s %s %s\n' "$libraries/libc.a" "$libm" "$libpthread" "$libssp_ns" "$libssp" "$runtime" "$libraries/crt1.o" "$libraries/crti.o" "$libraries/crtn.o" > orlix-link-inputs.mk
/usr/bin/printf 'version.h: %s\npipesize.h: %s\nbuiltins/libbuiltins.a: %s\n' "$work/src/support/mkversion.sh" "$work/src/builtins/psize.sh" "$work/src/builtins/psize.sh" >> orlix-link-inputs.mk
export MAKEFILES="$work/build/orlix-link-inputs.mk"
"$MAKE" -f Makefile -f orlix-link-inputs.mk -j1 bash
/usr/bin/install -m 0755 bash "$work/dest/usr/bin/bash"
"$strip_bin" "$work/dest/usr/bin/bash"
/usr/bin/file "$work/dest/usr/bin/bash" | /usr/bin/grep -F -q 'ELF 64-bit LSB pie executable, ARM aarch64' || {
  /usr/bin/file "$work/dest/usr/bin/bash" >&2
  exit 1
}
"$objdump_bin" -p "$work/dest/usr/bin/bash" | /usr/bin/awk '/^[[:space:]]*LOAD[[:space:]]/ && $0 !~ /align 2\*\*14/ { print "Orlix Bash PT_LOAD is not 16 KiB aligned: " $0 > "/dev/stderr"; bad=1 } END { exit bad }'
/bin/mkdir -p "$install_out"
/bin/cp -R "$work/dest/." "$install_out/"
(cd "$install_out" && /usr/bin/find . -type f -print | /usr/bin/sort) > "$file_manifest"
/usr/bin/printf '%s\n' 'license=GPL-3.0-or-later' > "$license_manifest"
/usr/bin/printf 'name=%s\nversion=%s\nengine=configure-make-destdir\n' "$package_name" "$package_version" > "$metadata"
digest="$( ( cd "$install_out" && /usr/bin/find . -type f -print0 | /usr/bin/sort -z | /usr/bin/xargs -0 /usr/bin/shasum -a 256 ) | /usr/bin/shasum -a 256 | /usr/bin/awk '{print $1}' )"
/usr/bin/printf '%s\n' "$digest" > "$digest_out"
if [ -n "$launcher" ]; then "$launcher" --print-log-stats --format=json; fi
""".replace("__COMPILER_IDENTITY__", ctx.file.compiler_identity.path))
    ctx.actions.run_shell(
        mnemonic = "OrlixGuestPackage",
        progress_message = "Building GNU Bash with incremental Make state",
        command = "PYTHONPATH=. /usr/bin/python3 -B -c 'from bazel.feasibility.packages import build_state; import sys; raise SystemExit(build_state.run(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5:]))' \"$@\"",
        arguments = [
            ctx.attr.package_name,
            script.path,
            ctx.file.toolchain_identity.path,
            ctx.file.compiler_identity.path,
            ctx.file.configure.path,
            sysroot.headers.path,
            uapi.headers.path,
            sysroot.libraries.path,
            sysroot.compiler_runtime.path,
            install_tree.path,
            file_manifest.path,
            license_manifest.path,
            metadata.path,
            digest.path,
            ctx.attr.package_name,
            ctx.attr.package_version,
        ],
        inputs = depset(
            direct = [
                script,
                ctx.file.toolchain_identity,
                ctx.file.compiler_identity,
                ctx.file.build_state,
                ctx.file.shared_state,
                ctx.file.configure,
                sysroot.headers,
                uapi.headers,
                sysroot.libraries,
                sysroot.compiler_runtime,
            ] + ctx.files.sources,
        ),
        outputs = [install_tree, file_manifest, license_manifest, metadata, digest],
        env = _pinned_env(ctx),
        use_default_shell_env = True,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-remote-cache": "1", "no-sandbox": "1"},
    )
    artifact_identity = declare_artifact_identity(
        ctx,
        "install",
        ctx.file._artifact_identity_serializer,
        root = install_tree,
    )
    return [
        DefaultInfo(files = depset([
            install_tree,
            file_manifest,
            license_manifest,
            metadata,
            digest,
            artifact_identity.manifest,
            artifact_identity.digest,
        ])),
        OrlixPackageTreeInfo(
            artifact_identity_closure = depset([artifact_identity.digest]),
            artifact_identity_digest = artifact_identity.digest,
            artifact_identity_manifest = artifact_identity.manifest,
            dependency_digests = sysroot.consumed_uapi_digest,
            file_manifest = file_manifest,
            install_tree = install_tree,
            license_manifest = license_manifest,
            package_metadata = metadata,
            source_input_digest = digest,
        ),
    ]

orlix_bash_package = rule(
    implementation = _bash_package_impl,
    attrs = {
        "toolchain_identity": attr.label(allow_single_file = True, default = "@orlix_kernel_toolchain//:bash-identity.json"),
        "compiler_identity": attr.label(allow_single_file = True, default = "@orlix_kernel_toolchain//:guest-compiler-identity.json"),
        "build_state": attr.label(allow_single_file = True, default = ":build_state.py"),
        "shared_state": attr.label(allow_single_file = True, default = "//bazel:build_state.py"),
        "sysroot": attr.label(mandatory = True, providers = [OrlixLibcSysrootInfo]),
        "uapi": attr.label(mandatory = True, providers = [OrlixInstalledUapiInfo]),
        "configure": attr.label(allow_single_file = True, mandatory = True),
        "sources": attr.label(allow_files = True, mandatory = True),
        "package_name": attr.string(default = "bash"),
        "package_version": attr.string(default = "5.3"),
        "_artifact_identity_serializer": attr.label(
            allow_single_file = True,
            default = Label("//bazel:content_digest.py"),
        ),
    },
)
