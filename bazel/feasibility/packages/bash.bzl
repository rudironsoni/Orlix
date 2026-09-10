"""Pinned GNU Bash Autotools package from OrlixMLibC sysroot and UAPI."""

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
    ctx.actions.run_shell(
        mnemonic = "OrlixGuestPackage",
        progress_message = "Building GNU Bash with configure/Make",
        command = r"""
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
work="$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/orlix-bash.XXXXXX")"
trap '/bin/rm -rf "$work"' EXIT
src="$(/usr/bin/dirname "$configure")"
/bin/mkdir -p "$work/src" "$work/build" "$work/dest/usr/bin"
/bin/cp -R "$src/." "$work/src"
/usr/bin/find "$work/src" -type d -exec /bin/chmod u+w {} +
/bin/chmod +x "$work/src/configure"
cd "$work/build"
libm="$libraries/libc.a"
libpthread="$libraries/libc.a"
libssp=""
libssp_ns=""
[ -s "$libraries/libm.a" ] && libm="$libraries/libm.a"
[ -s "$libraries/libpthread.a" ] && libpthread="$libraries/libpthread.a"
[ -s "$libraries/libssp.a" ] && libssp="$libraries/libssp.a"
[ -s "$libraries/libssp_nonshared.a" ] && libssp_ns="$libraries/libssp_nonshared.a"
export CC="$clang --target=aarch64-linux-gnu -isystem $headers -isystem $uapi_headers/include -D_GNU_SOURCE -fhosted -fno-builtin -ffixed-x18 -fPIE -ffile-prefix-map=$work=."
export CFLAGS="-O2 -Wno-unknown-warning-option -ffile-prefix-map=$work=."
export LDFLAGS="--target=aarch64-linux-gnu -fuse-ld=lld -nostdlib -static-pie -Wl,--gc-sections -Wl,-z,max-page-size=0x4000 $libraries/crt1.o $libraries/crti.o -Wl,--start-group"
export LIBS="$libraries/libc.a $libm $libpthread $libssp_ns $libssp $runtime -Wl,--end-group $libraries/crtn.o"
export AR="$ar_bin"
export RANLIB="$ranlib_bin"
export CC_FOR_BUILD="/usr/bin/xcrun --sdk macosx cc -target arm64-apple-macosx"
export BUILD_CC="$CC_FOR_BUILD"
export bash_cv_getenv_redef=no
export bash_cv_getcwd_malloc=yes
export bash_cv_func_strchrnul_works=yes
"$work/src/configure" --host=aarch64-linux-gnu --build=aarch64-apple-darwin --prefix=/usr --without-bash-malloc --enable-static-link --disable-nls --disable-readline --without-installed-readline --without-curses
/usr/bin/make -j1 bash
"$strip_bin" bash
/usr/bin/install -m 0755 bash "$work/dest/usr/bin/bash"
/usr/bin/file "$work/dest/usr/bin/bash" | /usr/bin/grep -F -q 'ELF 64-bit LSB pie executable, ARM aarch64' || {
  /usr/bin/file "$work/dest/usr/bin/bash" >&2
  exit 1
}
"$objdump_bin" -p "$work/dest/usr/bin/bash" | /usr/bin/awk '/^[[:space:]]*LOAD[[:space:]]/ && $0 !~ /align 2\*\*14/ { print "Orlix Bash PT_LOAD is not 16 KiB aligned: " $0 > "/dev/stderr"; bad=1 } END { exit bad }'
/bin/mkdir -p "$install_out"
/bin/cp -R "$work/dest/." "$install_out/"
/usr/bin/find "$install_out" -type f -print | /usr/bin/sort > "$file_manifest"
/usr/bin/printf '%s\n' 'license=GPL-3.0-or-later' > "$license_manifest"
/usr/bin/printf 'name=%s\nversion=%s\nengine=configure-make-destdir\n' "$package_name" "$package_version" > "$metadata"
digest="$( ( cd "$install_out" && /usr/bin/find . -type f -print0 | /usr/bin/sort -z | /usr/bin/xargs -0 /usr/bin/shasum -a 256 ) | /usr/bin/shasum -a 256 | /usr/bin/awk '{print $1}' )"
/usr/bin/printf '%s\n' "$digest" > "$digest_out"
""",
        arguments = [
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
                ctx.file.configure,
                sysroot.headers,
                uapi.headers,
                uapi.uapi_digest,
                sysroot.libraries,
                sysroot.compiler_runtime,
                sysroot.consumed_uapi_digest,
            ] + ctx.files.sources,
        ),
        outputs = [install_tree, file_manifest, license_manifest, metadata, digest],
        env = _pinned_env(ctx),
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-sandbox": "1"},
    )
    return [
        DefaultInfo(files = depset([install_tree, file_manifest, license_manifest, metadata, digest])),
        OrlixPackageTreeInfo(
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
        "sysroot": attr.label(mandatory = True, providers = [OrlixLibcSysrootInfo]),
        "uapi": attr.label(mandatory = True, providers = [OrlixInstalledUapiInfo]),
        "configure": attr.label(allow_single_file = True, mandatory = True),
        "sources": attr.label(allow_files = True, mandatory = True),
        "package_name": attr.string(default = "bash"),
        "package_version": attr.string(default = "5.3"),
    },
)
