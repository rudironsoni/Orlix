"""Orlix-local C programs against OrlixMLibC sysroot and UAPI."""

load("//bazel:artifact_identity.bzl", "declare_artifact_identity")
load("//bazel/providers:kernel_info.bzl", "OrlixInstalledUapiInfo")
load("//bazel/providers:package_info.bzl", "OrlixPackageTreeInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")
load(":source_key.bzl", "declare_source_key", "declare_tree_tar")

def _pinned_env(ctx):
    shell = ctx.configuration.default_shell_env
    developer_dir = shell.get("ORLIX_PINNED_DEVELOPER_DIR")
    if not developer_dir:
        fail("local C package requires action_env ORLIX_PINNED_DEVELOPER_DIR")
    return {
        "DEVELOPER_DIR": developer_dir,
        "HOME": "/var/empty",
        "PATH": "/opt/homebrew/opt/lld/bin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/bin:/bin",
    }

def _extract():
    return '/usr/bin/python3 -B -c \'import sys,tarfile; tarfile.open(sys.argv[1]).extractall(sys.argv[2], filter="data")\''

def _local_c_package_impl(ctx):
    sysroot = ctx.attr.sysroot[OrlixLibcSysrootInfo]
    uapi = ctx.attr.uapi[OrlixInstalledUapiInfo]
    header_archive = declare_tree_tar(ctx, "headers", sysroot.headers)
    uapi_archive = declare_tree_tar(ctx, "uapi", uapi.headers)
    library_archive = declare_tree_tar(ctx, "libraries", sysroot.libraries)
    source_files = ctx.files.srcs + ([ctx.file.root_init] if ctx.file.root_init else [])
    source_archive, source_stamp = declare_source_key(ctx, "sources", source_files)
    install_tree = ctx.actions.declare_directory(ctx.label.name + "/install")
    file_manifest = ctx.actions.declare_file(ctx.label.name + "/file-manifest.txt")
    license_manifest = ctx.actions.declare_file(ctx.label.name + "/license-manifest.txt")
    metadata = ctx.actions.declare_file(ctx.label.name + "/package-metadata.txt")
    ctx.actions.run_shell(
        mnemonic = "OrlixGuestPackage",
        progress_message = "Building local C package %s" % ctx.attr.package_name,
        command = r"""
set -euo pipefail
exec_root="$PWD"
header_archive="$exec_root/$1"
uapi_archive="$exec_root/$2"
library_archive="$exec_root/$3"
runtime="$exec_root/$4"
install_out="$exec_root/$5"
file_manifest="$exec_root/$6"
license_manifest="$exec_root/$7"
metadata="$exec_root/$8"
output_rel="${9}"
package_name="${10}"
package_version="${11}"
root_init_source="${12}"
source_archive="$exec_root/${13}"
source_stamp="$exec_root/${14}"
shift 14
test -s "$source_archive"
test -s "$source_stamp"
work="$(/usr/bin/mktemp -d /tmp/orlix-local.XXXXXX)"
trap '/bin/rm -rf "$work"' EXIT
headers="$work/headers"
uapi_headers="$work/uapi"
libraries="$work/libraries"
sources="$work/sources"
/bin/mkdir -p "$headers" "$uapi_headers" "$libraries" "$sources"
__EXTRACT__ "$header_archive" "$headers"
__EXTRACT__ "$uapi_archive" "$uapi_headers"
__EXTRACT__ "$library_archive" "$libraries"
__EXTRACT__ "$source_archive" "$sources"
c_srcs=""
inc=""
for rel in "$@"; do
  abs="$sources/$rel"
  inc="$inc -I$(/usr/bin/dirname "$abs")"
  case "$rel" in
    *.c) c_srcs="$c_srcs $abs" ;;
  esac
done
clang="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
strip_bin="$(/usr/bin/command -v llvm-strip)"
objdump_bin="$(/usr/bin/command -v llvm-objdump)"
test -x "$clang"
test -x "$strip_bin"
test -x "$objdump_bin"
test -s "$libraries/libc.a"
libm="$libraries/libc.a"
libpthread="$libraries/libc.a"
libssp=""
libssp_ns=""
[ -s "$libraries/libm.a" ] && libm="$libraries/libm.a"
[ -s "$libraries/libpthread.a" ] && libpthread="$libraries/libpthread.a"
[ -s "$libraries/libssp.a" ] && libssp="$libraries/libssp.a"
[ -s "$libraries/libssp_nonshared.a" ] && libssp_ns="$libraries/libssp_nonshared.a"
build_program() {
out="$1"
shift
/bin/mkdir -p "$(/usr/bin/dirname "$out")"
# shellcheck disable=SC2086
"$clang" --target=aarch64-linux-gnu -isystem "$headers" -isystem "$uapi_headers/include" $inc -D_GNU_SOURCE -std=c17 -O2 -fhosted -fno-builtin -ffixed-x18 -fPIE -ffile-prefix-map="$work"=. -static-pie -fuse-ld=lld -nostdlib -Wl,--gc-sections -Wl,-z,max-page-size=0x4000 "$libraries/crt1.o" "$libraries/crti.o" "$@" -Wl,--start-group "$libraries/libc.a" "$libm" "$libpthread" "$libssp_ns" "$libssp" "$runtime" -Wl,--end-group "$libraries/crtn.o" -o "$out"
"$strip_bin" "$out"
/usr/bin/file "$out" | /usr/bin/grep -F -q 'ELF 64-bit LSB pie executable, ARM aarch64' || { /usr/bin/file "$out" >&2; exit 1; }
"$objdump_bin" -p "$out" | /usr/bin/awk '/^[[:space:]]*LOAD[[:space:]]/ && $0 !~ /align 2\*\*14/ { print "PT_LOAD is not 16 KiB aligned: " $0 > "/dev/stderr"; bad=1 } END { exit bad }'
 }
build_program "$work/dest/$output_rel" $c_srcs
if [ -n "$root_init_source" ]; then
  build_program "$work/dest/rootinit" "$sources/$root_init_source"
fi
/bin/mkdir -p "$install_out"
/bin/cp -R "$work/dest/." "$install_out/"
/usr/bin/find "$install_out" -type f -print | /usr/bin/sort > "$file_manifest"
/usr/bin/printf '%s\n' 'license=GPL-2.0-or-later' > "$license_manifest"
/usr/bin/printf 'name=%s\nversion=%s\nengine=local-c\n' "$package_name" "$package_version" > "$metadata"
""".replace("__EXTRACT__", _extract()),
        arguments = [
            header_archive.path,
            uapi_archive.path,
            library_archive.path,
            sysroot.compiler_runtime.path,
            install_tree.path,
            file_manifest.path,
            license_manifest.path,
            metadata.path,
            ctx.attr.output,
            ctx.attr.package_name,
            ctx.attr.package_version,
            ctx.file.root_init.short_path if ctx.file.root_init else "",
            source_archive.path,
            source_stamp.path,
        ] + [source.short_path for source in ctx.files.srcs],
        inputs = depset(
            direct = [
                header_archive,
                uapi_archive,
                library_archive,
                sysroot.compiler_runtime,
                source_archive,
                source_stamp,
            ] + source_files,
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

orlix_local_c_package = rule(
    implementation = _local_c_package_impl,
    attrs = {
        "sysroot": attr.label(mandatory = True, providers = [OrlixLibcSysrootInfo]),
        "uapi": attr.label(mandatory = True, providers = [OrlixInstalledUapiInfo]),
        "srcs": attr.label_list(allow_files = True, mandatory = True),
        "output": attr.string(mandatory = True),
        "root_init": attr.label(allow_single_file = [".c"]),
        "package_name": attr.string(mandatory = True),
        "package_version": attr.string(default = "orlix"),
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
