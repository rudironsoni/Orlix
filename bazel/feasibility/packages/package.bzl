"""Autotools/upstream-Make guest package from OrlixMLibC sysroot only."""

load("//bazel:artifact_identity.bzl", "declare_artifact_identity")
load("//bazel:compiler_cache.bzl", "compiler_cache_env")
load("//bazel/providers:package_info.bzl", "OrlixPackageTreeInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")
load(":source_key.bzl", "declare_source_key", "declare_tree_tar")

def _pinned_env(ctx):
    env = {
        "HOME": "/var/empty",
        "PATH": "/usr/bin:/bin",
        "CCACHE_CONFIGPATH": "/dev/null",
        "CCACHE_COMPILERCHECK": "content",
        "CCACHE_MAXSIZE": "20G",
    }
    env.update(compiler_cache_env(ctx.configuration.default_shell_env))
    return env

def _extract():
    # filter= exists only when tarfile.data_filter does (Python 3.12+).
    return "/usr/bin/python3 -B -c 'import sys,tarfile; archive=tarfile.open(sys.argv[1]); archive.extractall(sys.argv[2], filter=\"data\") if hasattr(tarfile, \"data_filter\") else archive.extractall(sys.argv[2])'"

def _guest_package_impl(ctx):
    sysroot = ctx.attr.sysroot[OrlixLibcSysrootInfo]
    header_archive = declare_tree_tar(ctx, "headers", sysroot.headers)
    library_archive = declare_tree_tar(ctx, "libraries", sysroot.libraries)
    source_files = ctx.files.configure_directory
    source_archive, source_stamp = declare_source_key(ctx, "sources", source_files)
    install_tree = ctx.actions.declare_directory(ctx.label.name + "/install")
    file_manifest = ctx.actions.declare_file(ctx.label.name + "/file-manifest.txt")
    license_manifest = ctx.actions.declare_file(ctx.label.name + "/license-manifest.txt")
    metadata = ctx.actions.declare_file(ctx.label.name + "/package-metadata.txt")
    ctx.actions.run_shell(
        mnemonic = "OrlixGuestPackage",
        progress_message = "Building guest package %s with configure/Make" % ctx.attr.package_name,
        command = r"""
set -euo pipefail
exec_root="$PWD"
header_archive="$exec_root/$1"
library_archive="$exec_root/$2"
runtime="$exec_root/$3"
install_out="$exec_root/$4"
file_manifest="$exec_root/$5"
license_manifest="$exec_root/$6"
metadata="$exec_root/$7"
package_name="${8}"
package_version="${9}"
configure_rel="${10}"
source_archive="$exec_root/${11}"
source_stamp="$exec_root/${12}"
gmake="$exec_root/${13}"
tool_bin="$(/usr/bin/dirname "$gmake")"
export PATH="$tool_bin:/usr/bin:/bin"
tool_root="$(/usr/bin/dirname "$tool_bin")"
export DYLD_LIBRARY_PATH="$tool_root/lib:$tool_root/llvm/lib"
case "$configure_rel" in
  *OrlixOS/Sources/make*|*OrlixCoreUtils/Sources/make*|*OrlixKernel/Makefile*)
    echo "guest package must not invoke wrapper Makefiles: $configure_rel" >&2
    exit 1
    ;;
esac
test -s "$source_archive"
test -s "$source_stamp"
test -s "$runtime"
clang="$tool_bin/clang"
test -x "$clang"
work="$(/usr/bin/mktemp -d /tmp/orlix-package.XXXXXX)"
trap '/bin/rm -rf "$work"' EXIT
headers="$work/headers"
libraries="$work/libraries"
sources="$work/sources"
/bin/mkdir -p "$headers" "$libraries" "$sources" "$work/dest"
__EXTRACT__ "$header_archive" "$headers"
__EXTRACT__ "$library_archive" "$libraries"
__EXTRACT__ "$source_archive" "$sources"
test -s "$libraries/libc.a"
test -s "$libraries/crt1.o"
test -s "$libraries/crti.o"
test -s "$libraries/crtn.o"
src="$sources/$(/usr/bin/dirname "$configure_rel")"
configure_name="$(/usr/bin/basename "$configure_rel")"
/bin/chmod +x "$src/$configure_name"
cd "$src"
if [ -n "${CCACHE_DISABLE:-}" ] || [ ! -x "$tool_bin/ccache" ]; then
  launcher=""
elif [ -n "${CCACHE_DIR:-}" ]; then
  launcher="$tool_bin/ccache"
elif [ -n "${HOME:-}" ] && [ "$HOME" != "/var/empty" ] && /bin/mkdir -p "${HOME}/Library/Caches/Orlix/ccache"; then
  export CCACHE_DIR="${HOME}/Library/Caches/Orlix/ccache"
  launcher="$tool_bin/ccache"
else
  launcher=""
fi
export ORLIX_COMPILER_LAUNCHER="$launcher"
export CCACHE_BASEDIR="$work"
CC="${launcher:+$launcher }$clang" \
CFLAGS="--target=aarch64-linux-gnu -isystem $headers -fno-builtin -ffixed-x18 -fPIE -ffile-prefix-map=$work=. -ffile-prefix-map=$tool_bin=/orlix/tools" \
LDFLAGS="--target=aarch64-linux-gnu -fuse-ld=$tool_bin/ld.lld -nostdlib -static-pie -Wl,-z,max-page-size=0x4000 $libraries/crt1.o $libraries/crti.o -Wl,--start-group $libraries/libc.a $runtime -Wl,--end-group $libraries/crtn.o" \
    "./$configure_name" --host=aarch64-linux-gnu --prefix=/usr
"$tool_bin/gmake" -j1
DESTDIR="$work/dest" "$tool_bin/gmake" -j1 install
test -x "$work/dest/usr/bin/true"
/bin/mkdir -p "$install_out"
/bin/cp -R "$work/dest/." "$install_out/"
(cd "$install_out" && /usr/bin/find . -type f -print | /usr/bin/sort) > "$file_manifest"
/usr/bin/printf '%s\n' 'license=unlicense-feasibility-true' > "$license_manifest"
/usr/bin/printf 'name=%s\nversion=%s\nengine=configure-make-destdir\n' "$package_name" "$package_version" > "$metadata"
""".replace("__EXTRACT__", _extract()),
        arguments = [
            header_archive.path,
            library_archive.path,
            sysroot.compiler_runtime.path,
            install_tree.path,
            file_manifest.path,
            license_manifest.path,
            metadata.path,
            ctx.attr.package_name,
            ctx.attr.package_version,
            ctx.file.configure.short_path,
            source_archive.path,
            source_stamp.path,
            ctx.file._gmake.path,
        ],
        inputs = depset(
            direct = [
                header_archive,
                library_archive,
                sysroot.compiler_runtime,
                source_archive,
                source_stamp,
                ctx.file.configure,
                ctx.file.source,
                ctx.file._gmake,
            ] + source_files + ctx.files._host_tools,
        ),
        outputs = [install_tree, file_manifest, license_manifest, metadata],
        env = _pinned_env(ctx),
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-sandbox": "1"},
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
            source_input_digest = artifact_identity.digest,
        ),
    ]

orlix_guest_package = rule(
    implementation = _guest_package_impl,
    attrs = {
        "sysroot": attr.label(mandatory = True, providers = [OrlixLibcSysrootInfo]),
        "configure": attr.label(allow_single_file = True, mandatory = True),
        "configure_directory": attr.label(allow_files = True, mandatory = True),
        "source": attr.label(allow_single_file = True, mandatory = True),
        "package_name": attr.string(mandatory = True),
        "package_version": attr.string(mandatory = True),
        "_gmake": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/gmake")),
        "_host_tools": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:mlibc_support")),
        "_artifact_identity_serializer": attr.label(
            allow_single_file = True,
            default = Label("//bazel:content_digest.py"),
        ),
        "_source_key": attr.label(
            allow_single_file = True,
            default = ":source_key.py",
        ),
    },
)
