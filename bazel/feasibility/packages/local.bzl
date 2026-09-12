"""Orlix-local C programs against OrlixMLibC sysroot and UAPI."""

load("//bazel:artifact_identity.bzl", "declare_artifact_identity")
load("//bazel/providers:kernel_info.bzl", "OrlixInstalledUapiInfo")
load("//bazel/providers:package_info.bzl", "OrlixPackageTreeInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")

def _pinned_env(ctx):
    shell = ctx.configuration.default_shell_env
    developer_dir = shell.get("ORLIX_PINNED_DEVELOPER_DIR")
    if not developer_dir:
        fail("local C package requires action_env ORLIX_PINNED_DEVELOPER_DIR")
    env = {
        "DEVELOPER_DIR": developer_dir,
        "HOME": "/var/empty",
        "PATH": "/opt/homebrew/opt/lld/bin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/bin:/bin",
    }
    tmpdir = shell.get("TMPDIR")
    if tmpdir:
        env["TMPDIR"] = tmpdir
    return env

def _local_c_package_impl(ctx):
    sysroot = ctx.attr.sysroot[OrlixLibcSysrootInfo]
    uapi = ctx.attr.uapi[OrlixInstalledUapiInfo]
    install_tree = ctx.actions.declare_directory(ctx.label.name + "/install")
    file_manifest = ctx.actions.declare_file(ctx.label.name + "/file-manifest.txt")
    license_manifest = ctx.actions.declare_file(ctx.label.name + "/license-manifest.txt")
    metadata = ctx.actions.declare_file(ctx.label.name + "/package-metadata.txt")
    digest = ctx.actions.declare_file(ctx.label.name + "/source-input.sha256")
    ctx.actions.run_shell(
        mnemonic = "OrlixGuestPackage",
        progress_message = "Building local C package %s" % ctx.attr.package_name,
        command = r"""
set -euo pipefail
exec_root="$PWD"
headers="$exec_root/$1"
uapi_headers="$exec_root/$2"
libraries="$exec_root/$3"
runtime="$exec_root/$4"
install_out="$exec_root/$5"
file_manifest="$exec_root/$6"
license_manifest="$exec_root/$7"
metadata="$exec_root/$8"
digest_out="$exec_root/$9"
output_rel="${10}"
package_name="${11}"
package_version="${12}"
root_init_source="${13}"
shift 13
c_srcs=""
inc=""
for rel in "$@"; do
  abs="$exec_root/$rel"
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
work="$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/orlix-local.XXXXXX")"
trap '/bin/rm -rf "$work"' EXIT
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
  build_program "$work/dest/rootinit" "$exec_root/$root_init_source"
fi
/bin/mkdir -p "$install_out"
/bin/cp -R "$work/dest/." "$install_out/"
/usr/bin/find "$install_out" -type f -print | /usr/bin/sort > "$file_manifest"
/usr/bin/printf '%s\n' 'license=GPL-2.0-or-later' > "$license_manifest"
/usr/bin/printf 'name=%s\nversion=%s\nengine=local-c\n' "$package_name" "$package_version" > "$metadata"
digest="$( ( cd "$install_out" && /usr/bin/find . -type f -print0 | /usr/bin/sort -z | /usr/bin/xargs -0 /usr/bin/shasum -a 256 ) | /usr/bin/shasum -a 256 | /usr/bin/awk '{print $1}' )"
/usr/bin/printf '%s\n' "$digest" > "$digest_out"
""",
        arguments = [
            sysroot.headers.path,
            uapi.headers.path,
            sysroot.libraries.path,
            sysroot.compiler_runtime.path,
            install_tree.path,
            file_manifest.path,
            license_manifest.path,
            metadata.path,
            digest.path,
            ctx.attr.output,
            ctx.attr.package_name,
            ctx.attr.package_version,
            ctx.file.root_init.path if ctx.file.root_init else "",
        ] + [f.path for f in ctx.files.srcs],
        inputs = depset(
            direct = [
                sysroot.headers,
                uapi.headers,
                uapi.uapi_digest,
                sysroot.libraries,
                sysroot.compiler_runtime,
                sysroot.consumed_uapi_digest,
            ] + ctx.files.srcs + ([ctx.file.root_init] if ctx.file.root_init else []),
        ),
        outputs = [install_tree, file_manifest, license_manifest, metadata, digest],
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
    },
)
