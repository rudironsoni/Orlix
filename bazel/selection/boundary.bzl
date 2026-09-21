"""Project origin artifacts onto stable selected_* coordinates.

Downstream compilation sees only these declared outputs. Origin-specific
bazel-out prefixes, proof files, and unused provider fields stay off the
consumer input set.
"""

load(
    "//bazel/providers:kernel_info.bzl",
    "OrlixInstalledUapiInfo",
    "OrlixLinuxArchiveInfo",
)
load("//bazel/providers:rootfs_info.bzl", "OrlixRootfsInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")

def _stage_file(ctx, name, src):
    out = ctx.actions.declare_file(ctx.label.name + "/" + name)
    ctx.actions.symlink(output = out, target_file = src)
    return out

def _stage_tree(ctx, name, src, mnemonic):
    out = ctx.actions.declare_directory(ctx.label.name + "/" + name)
    ctx.actions.run_shell(
        mnemonic = mnemonic,
        progress_message = "Projecting %s onto %s" % (src.short_path, ctx.label.name),
        command = """
set -euo pipefail
src="$1"
dest="$2"
/bin/mkdir -p "$dest"
if [ -s "$src/include/linux/unistd.h" ]; then
  /bin/cp -R "$src/." "$dest/"
elif [ -s "$src/linux/unistd.h" ]; then
  /bin/mkdir -p "$dest/include"
  /bin/cp -R "$src/." "$dest/include/"
elif [ -d "$src" ]; then
  /bin/cp -R "$src/." "$dest/"
else
  echo "selected boundary missing tree: $src" >&2
  exit 1
fi
""",
        arguments = [src.path, out.path],
        inputs = [src],
        outputs = [out],
        use_default_shell_env = False,
        execution_requirements = {
            "block-network": "1",
            "no-remote-exec": "1",
        },
    )
    return out

def _selected_uapi_impl(ctx):
    origin = ctx.attr.origin[OrlixInstalledUapiInfo]
    headers = _stage_tree(ctx, "headers", origin.headers, "OrlixSelectUapiHeaders")
    digest = _stage_file(ctx, "uapi.sha256", origin.uapi_digest)
    return [
        DefaultInfo(files = depset([headers, digest])),
        OrlixInstalledUapiInfo(
            arch = origin.arch,
            artifact_identity_digest = None,
            artifact_identity_manifest = None,
            headers = headers,
            linux_revision = origin.linux_revision,
            uapi_digest = digest,
        ),
    ]

orlix_selected_uapi = rule(
    implementation = _selected_uapi_impl,
    attrs = {
        "origin": attr.label(mandatory = True, providers = [OrlixInstalledUapiInfo]),
    },
)

def _selected_sysroot_impl(ctx):
    origin = ctx.attr.origin[OrlixLibcSysrootInfo]
    headers = _stage_tree(ctx, "headers", origin.headers, "OrlixSelectSysrootHeaders")
    libraries = _stage_tree(ctx, "libraries", origin.libraries, "OrlixSelectSysrootLibraries")
    runtime = _stage_file(ctx, "libcompiler_rt.a", origin.compiler_runtime)
    loader = _stage_file(ctx, "ld.so", origin.dynamic_loader)
    sysroot_digest = _stage_file(ctx, "sysroot.sha256", origin.sysroot_digest)
    consumed = _stage_file(ctx, "consumed_uapi.sha256", origin.consumed_uapi_digest)
    abi = _stage_file(ctx, "abi.txt", origin.abi_manifest)
    return [
        DefaultInfo(files = depset([
            headers,
            libraries,
            runtime,
            loader,
            sysroot_digest,
            consumed,
            abi,
        ])),
        OrlixLibcSysrootInfo(
            abi_manifest = abi,
            artifact_identity_digest = None,
            artifact_identity_manifest = None,
            compiler_runtime = runtime,
            compiler_runtime_identity_digest = None,
            compiler_runtime_identity_manifest = None,
            consumed_uapi_digest = consumed,
            dynamic_loader = loader,
            headers = headers,
            libraries = libraries,
            sysroot_digest = sysroot_digest,
            target_triple = origin.target_triple,
        ),
    ]

orlix_selected_sysroot = rule(
    implementation = _selected_sysroot_impl,
    attrs = {
        "origin": attr.label(mandatory = True, providers = [OrlixLibcSysrootInfo]),
    },
)

def _selected_rootfs_impl(ctx):
    origin = ctx.attr.origin[OrlixRootfsInfo]
    initramfs = _stage_file(ctx, "initramfs.cpio.gz", origin.initramfs)
    base_ext4 = _stage_file(ctx, "base.ext4", origin.base_ext4)
    state_ext4 = _stage_file(ctx, "state.ext4", origin.state_ext4)
    file_manifest = _stage_file(ctx, "file-manifest.txt", origin.file_manifest)
    payload_metadata = _stage_file(ctx, "payload-metadata.txt", origin.payload_metadata)
    source_input_digest = _stage_file(ctx, "source-input.sha256", origin.source_input_digest)
    return [
        DefaultInfo(files = depset([
            initramfs,
            base_ext4,
            state_ext4,
            file_manifest,
            payload_metadata,
            source_input_digest,
        ])),
        OrlixRootfsInfo(
            artifact_identity_digest = None,
            artifact_identity_manifest = None,
            base_ext4 = base_ext4,
            file_manifest = file_manifest,
            initramfs = initramfs,
            package_closure = depset(),
            payload_metadata = payload_metadata,
            source_input_digest = source_input_digest,
            state_ext4 = state_ext4,
        ),
    ]

orlix_selected_rootfs = rule(
    implementation = _selected_rootfs_impl,
    attrs = {
        "origin": attr.label(mandatory = True, providers = [OrlixRootfsInfo]),
    },
)

def _selected_linux_archive_impl(ctx):
    origin = ctx.attr.origin[OrlixLinuxArchiveInfo]
    archive = _stage_file(ctx, "OrlixKernel.a", origin.archive)
    digest = _stage_file(ctx, "archive.sha256", origin.source_input_digest)
    boot = []
    for resource in origin.boot_resources.to_list():
        boot.append(_stage_file(ctx, "arch/orlix/boot/dts/" + resource.basename, resource))
    return [
        DefaultInfo(files = depset([archive, digest] + boot)),
        OutputGroupInfo(
            archive = depset([archive]),
            boot_resources = depset(boot),
        ),
        OrlixLinuxArchiveInfo(
            archive = archive,
            artifact_identity_digest = None,
            artifact_identity_manifest = None,
            boot_resources = depset(boot),
            build_manifest = None,
            destination = origin.destination,
            product = None,
            profile = origin.profile,
            source_input_digest = digest,
            symbol_manifest = None,
        ),
    ]

orlix_selected_linux_archive = rule(
    implementation = _selected_linux_archive_impl,
    attrs = {
        "origin": attr.label(mandatory = True, providers = [OrlixLinuxArchiveInfo]),
    },
)

def _refuse_unmatched_promoted_kernel_impl(ctx):
    fail("promoted kernel has no matching slice for this platform and profile")

orlix_refuse_unmatched_promoted_kernel = rule(
    implementation = _refuse_unmatched_promoted_kernel_impl,
    provides = [OrlixLinuxArchiveInfo],
)
