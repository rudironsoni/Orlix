"""Autotools/upstream-Make guest package from OrlixMLibC sysroot only."""

load("//bazel/providers:package_info.bzl", "OrlixPackageTreeInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")

def _pinned_env(ctx):
    shell = ctx.configuration.default_shell_env
    developer_dir = shell.get("ORLIX_PINNED_DEVELOPER_DIR")
    if not developer_dir:
        fail("guest package requires action_env ORLIX_PINNED_DEVELOPER_DIR")
    env = {
        "DEVELOPER_DIR": developer_dir,
        "HOME": "/var/empty",
        "PATH": "/opt/homebrew/opt/lld/bin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/bin:/bin",
    }
    tmpdir = shell.get("TMPDIR")
    if tmpdir:
        env["TMPDIR"] = tmpdir
    return env

def _guest_package_impl(ctx):
    sysroot = ctx.attr.sysroot[OrlixLibcSysrootInfo]
    install_tree = ctx.actions.declare_directory(ctx.label.name + "/install")
    file_manifest = ctx.actions.declare_file(ctx.label.name + "/file-manifest.txt")
    license_manifest = ctx.actions.declare_file(ctx.label.name + "/license-manifest.txt")
    metadata = ctx.actions.declare_file(ctx.label.name + "/package-metadata.txt")
    digest = ctx.actions.declare_file(ctx.label.name + "/source-input.sha256")
    ctx.actions.run_shell(
        mnemonic = "OrlixGuestPackage",
        progress_message = "Building guest package %s with configure/Make" % ctx.attr.package_name,
        command = r"""
set -euo pipefail
exec_root="$PWD"
configure="$exec_root/$1"
headers="$exec_root/$2"
libraries="$exec_root/$3"
runtime="$exec_root/$4"
install_out="$exec_root/$5"
file_manifest="$exec_root/$6"
license_manifest="$exec_root/$7"
metadata="$exec_root/$8"
digest_out="$exec_root/$9"
package_name="${10}"
package_version="${11}"
case "$configure" in
  *OrlixOS/Sources/make*|*OrlixCoreUtils/Sources/make*|*OrlixKernel/Makefile*)
    echo "guest package must not invoke wrapper Makefiles: $configure" >&2
    exit 1
    ;;
esac
clang="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
test -x "$clang"
test -s "$libraries/libc.a"
test -s "$libraries/crt1.o"
test -s "$libraries/crti.o"
test -s "$libraries/crtn.o"
test -s "$runtime"
work="$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/orlix-package.XXXXXX")"
trap '/bin/rm -rf "$work"' EXIT
src="$(/usr/bin/dirname "$configure")"
/bin/mkdir -p "$work/src" "$work/dest"
/bin/cp -R "$src/." "$work/src"
/usr/bin/find "$work/src" -type d -exec /bin/chmod u+w {} +
/bin/chmod +x "$work/src/configure"
cd "$work/src"
CC="$clang" \
CFLAGS="--target=aarch64-linux-gnu -isystem $headers -fno-builtin -ffixed-x18 -fPIE -ffile-prefix-map=$work=." \
LDFLAGS="--target=aarch64-linux-gnu -fuse-ld=lld -nostdlib -static-pie -Wl,-z,max-page-size=0x4000 $libraries/crt1.o $libraries/crti.o -Wl,--start-group $libraries/libc.a $runtime -Wl,--end-group $libraries/crtn.o" \
    ./configure --host=aarch64-linux-gnu --prefix=/usr
/usr/bin/make -j1
DESTDIR="$work/dest" /usr/bin/make -j1 install
test -x "$work/dest/usr/bin/true"
/bin/mkdir -p "$install_out"
/bin/cp -R "$work/dest/." "$install_out/"
/usr/bin/find "$install_out" -type f -print | /usr/bin/sort > "$file_manifest"
/usr/bin/printf '%s\n' 'license=unlicense-feasibility-true' > "$license_manifest"
/usr/bin/printf 'name=%s\nversion=%s\nengine=configure-make-destdir\n' "$package_name" "$package_version" > "$metadata"
digest="$( ( cd "$install_out" && /usr/bin/find . -type f -print0 | /usr/bin/sort -z | /usr/bin/xargs -0 /usr/bin/shasum -a 256 ) | /usr/bin/shasum -a 256 | /usr/bin/awk '{print $1}' )"
/usr/bin/printf '%s\n' "$digest" > "$digest_out"
""",
        arguments = [
            ctx.file.configure.path,
            sysroot.headers.path,
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
                ctx.file.source,
                sysroot.headers,
                sysroot.libraries,
                sysroot.compiler_runtime,
                sysroot.consumed_uapi_digest,
            ],
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

orlix_guest_package = rule(
    implementation = _guest_package_impl,
    attrs = {
        "sysroot": attr.label(mandatory = True, providers = [OrlixLibcSysrootInfo]),
        "configure": attr.label(allow_single_file = True, mandatory = True),
        "source": attr.label(allow_single_file = True, mandatory = True),
        "package_name": attr.string(mandatory = True),
        "package_version": attr.string(mandatory = True),
    },
)
