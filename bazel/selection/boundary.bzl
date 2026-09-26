"""Project selected bytes onto stable logical names.

Consumer inputs are copied files and trees (`cp -L`, `cp -R -L`). Symlinks
are not consumer inputs. Producer digest sidecars stay off the selected
provider. `artifact_identity_digest` is the selected bytes. DTB paths stay
under `arch/orlix/boot/dts/`.
"""

load(
    "//bazel/providers:kernel_info.bzl",
    "OrlixInstalledUapiInfo",
    "OrlixLinuxArchiveInfo",
)
load("//bazel/providers:rootfs_info.bzl", "OrlixRootfsInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")

def _tool_attrs(origin):
    return {
        "origin": origin,
        "_content_digest": attr.label(
            allow_single_file = True,
            default = Label("//bazel:content_digest.py"),
        ),
        "_materialize": attr.label(
            allow_single_file = True,
            default = Label("//bazel/selection:materialize_bytes.py"),
        ),
    }

def _run(ctx, mnemonic, arguments, inputs, outputs):
    ctx.actions.run_shell(
        mnemonic = mnemonic,
        progress_message = "Materializing selected bytes for %s" % ctx.label,
        command = """
set -euo pipefail
/usr/bin/python3 -B "$1" "${@:2}"
""",
        arguments = arguments,
        inputs = inputs,
        outputs = outputs,
        use_default_shell_env = False,
        execution_requirements = {
            "block-network": "1",
            "no-remote-exec": "1",
        },
    )

def _tools(ctx):
    return [ctx.file._materialize, ctx.file._content_digest]

def _identity_files(ctx):
    return (
        ctx.actions.declare_file(ctx.label.name + "/artifact-identity-v2.json"),
        ctx.actions.declare_file(ctx.label.name + "/artifact-identity-v2.sha256"),
    )

def _selected_result(product, manifest, digest, provider):
    return [
        DefaultInfo(files = depset(product)),
        OutputGroupInfo(artifact_identity = depset([manifest, digest])),
        provider,
    ]

def _selected_uapi_impl(ctx):
    origin = ctx.attr.origin[OrlixInstalledUapiInfo]
    headers = ctx.actions.declare_directory(ctx.label.name + "/headers")
    manifest, digest = _identity_files(ctx)
    _run(
        ctx,
        "OrlixMaterializeUapi",
        [
            ctx.file._materialize.path,
            "project-tree",
            "--src",
            origin.headers.path,
            "--dest",
            headers.path,
            "--manifest",
            manifest.path,
            "--digest",
            digest.path,
        ],
        _tools(ctx) + [origin.headers],
        [headers, manifest, digest],
    )
    return _selected_result(
        [headers],
        manifest,
        digest,
        OrlixInstalledUapiInfo(
            arch = origin.arch,
            artifact_identity_digest = digest,
            artifact_identity_manifest = manifest,
            headers = headers,
            linux_revision = origin.linux_revision,
            uapi_digest = digest,
        ),
    )

orlix_selected_uapi = rule(
    implementation = _selected_uapi_impl,
    attrs = _tool_attrs(attr.label(mandatory = True, providers = [OrlixInstalledUapiInfo])),
)

def _selected_sysroot_impl(ctx):
    origin = ctx.attr.origin[OrlixLibcSysrootInfo]
    headers = ctx.actions.declare_directory(ctx.label.name + "/headers")
    libraries = ctx.actions.declare_directory(ctx.label.name + "/libraries")
    runtime = ctx.actions.declare_file(ctx.label.name + "/libcompiler_rt.a")
    loader = ctx.actions.declare_file(ctx.label.name + "/ld.so")
    manifest, digest = _identity_files(ctx)
    _run(
        ctx,
        "OrlixMaterializeSysroot",
        [
            ctx.file._materialize.path,
            "project-sysroot",
            "--headers-src",
            origin.headers.path,
            "--headers-dest",
            headers.path,
            "--libraries-src",
            origin.libraries.path,
            "--libraries-dest",
            libraries.path,
            "--runtime-src",
            origin.compiler_runtime.path,
            "--runtime-dest",
            runtime.path,
            "--loader-src",
            origin.dynamic_loader.path,
            "--loader-dest",
            loader.path,
            "--manifest",
            manifest.path,
            "--digest",
            digest.path,
        ],
        _tools(ctx) + [
            origin.headers,
            origin.libraries,
            origin.compiler_runtime,
            origin.dynamic_loader,
        ],
        [headers, libraries, runtime, loader, manifest, digest],
    )
    return _selected_result(
        [headers, libraries, runtime, loader],
        manifest,
        digest,
        OrlixLibcSysrootInfo(
            abi_manifest = None,
            artifact_identity_digest = digest,
            artifact_identity_manifest = manifest,
            compiler_runtime = runtime,
            compiler_runtime_identity_digest = None,
            compiler_runtime_identity_manifest = None,
            consumed_uapi_digest = None,
            dynamic_loader = loader,
            headers = headers,
            libraries = libraries,
            sysroot_digest = digest,
            target_triple = origin.target_triple,
        ),
    )

orlix_selected_sysroot = rule(
    implementation = _selected_sysroot_impl,
    attrs = _tool_attrs(attr.label(mandatory = True, providers = [OrlixLibcSysrootInfo])),
)

def _selected_rootfs_impl(ctx):
    origin = ctx.attr.origin[OrlixRootfsInfo]
    initramfs = ctx.actions.declare_file(ctx.label.name + "/initramfs.cpio.gz")
    base_ext4 = ctx.actions.declare_file(ctx.label.name + "/base.ext4")
    state_ext4 = ctx.actions.declare_file(ctx.label.name + "/state.ext4")
    manifest, digest = _identity_files(ctx)
    _run(
        ctx,
        "OrlixMaterializeRootfs",
        [
            ctx.file._materialize.path,
            "project-files",
            "--manifest",
            manifest.path,
            "--digest",
            digest.path,
            "--map",
            "initramfs.cpio.gz",
            origin.initramfs.path,
            initramfs.path,
            "--map",
            "base.ext4",
            origin.base_ext4.path,
            base_ext4.path,
            "--map",
            "state.ext4",
            origin.state_ext4.path,
            state_ext4.path,
        ],
        _tools(ctx) + [origin.initramfs, origin.base_ext4, origin.state_ext4],
        [initramfs, base_ext4, state_ext4, manifest, digest],
    )
    return _selected_result(
        [initramfs, base_ext4, state_ext4],
        manifest,
        digest,
        OrlixRootfsInfo(
            artifact_identity_digest = digest,
            artifact_identity_manifest = manifest,
            base_ext4 = base_ext4,
            file_manifest = None,
            initramfs = initramfs,
            package_closure = depset(),
            payload_metadata = None,
            source_input_digest = None,
            state_ext4 = state_ext4,
        ),
    )

orlix_selected_rootfs = rule(
    implementation = _selected_rootfs_impl,
    attrs = _tool_attrs(attr.label(mandatory = True, providers = [OrlixRootfsInfo])),
)

def _selected_linux_archive_impl(ctx):
    origin = ctx.attr.origin[OrlixLinuxArchiveInfo]
    archive = ctx.actions.declare_file(ctx.label.name + "/OrlixKernel.a")
    manifest, digest = _identity_files(ctx)
    arguments = [
        ctx.file._materialize.path,
        "project-files",
        "--manifest",
        manifest.path,
        "--digest",
        digest.path,
        "--map",
        "OrlixKernel.a",
        origin.archive.path,
        archive.path,
    ]
    boot = []
    seen = {}
    inputs = [origin.archive]
    for resource in sorted(origin.boot_resources.to_list(), key = lambda item: item.basename):
        if not resource.basename.endswith(".dtb"):
            fail("DTB path must stay under arch/orlix/boot/dts")
        logical = "arch/orlix/boot/dts/" + resource.basename
        if logical in seen:
            fail("duplicate DTB logical path: %s" % logical)
        seen[logical] = True
        dest = ctx.actions.declare_file(ctx.label.name + "/" + logical)
        boot.append(dest)
        inputs.append(resource)
        arguments.extend(["--map", logical, resource.path, dest.path])
    _run(
        ctx,
        "OrlixMaterializeKernel",
        arguments,
        _tools(ctx) + inputs,
        [archive, manifest, digest] + boot,
    )
    return [
        DefaultInfo(files = depset([archive] + boot)),
        OutputGroupInfo(
            archive = depset([archive]),
            artifact_identity = depset([manifest, digest]),
            boot_resources = depset(boot),
        ),
        OrlixLinuxArchiveInfo(
            archive = archive,
            artifact_identity_digest = digest,
            artifact_identity_manifest = manifest,
            boot_resources = depset(boot),
            build_manifest = None,
            destination = origin.destination,
            product = None,
            profile = origin.profile,
            source_input_digest = None,
            symbol_manifest = None,
        ),
    ]

orlix_selected_linux_archive = rule(
    implementation = _selected_linux_archive_impl,
    attrs = _tool_attrs(attr.label(mandatory = True, providers = [OrlixLinuxArchiveInfo])),
)

def _refuse_unmatched_promoted_kernel_impl(ctx):
    fail("unmatched promoted kernel fails analysis")

orlix_refuse_unmatched_promoted_kernel = rule(
    implementation = _refuse_unmatched_promoted_kernel_impl,
    provides = [OrlixLinuxArchiveInfo],
)
