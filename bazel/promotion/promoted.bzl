"""Consume artifacts.lock.json in promoted mode. Never accept mutable latest."""

load("//bazel/providers:kernel_info.bzl", "OrlixInstalledUapiInfo", "OrlixLinuxArchiveInfo")
load("//bazel/providers:rootfs_info.bzl", "OrlixRootfsInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")

def _locked_buildset_impl(ctx):
    out = ctx.actions.declare_file(ctx.label.name + "/locked-buildset.json")
    ctx.actions.run_shell(
        mnemonic = "OrlixLockedBuildset",
        progress_message = "Validating signed artifacts.lock.json buildset",
        command = r"""
set -euo pipefail
exec_root="$PWD"
/usr/bin/python3 "$exec_root/$1" --lock "$exec_root/$2" --out "$exec_root/$3"
""",
        arguments = [
            ctx.file._tool.path,
            ctx.file.lock.path,
            out.path,
        ],
        inputs = [ctx.file._tool, ctx.file.lock],
        outputs = [out],
        use_default_shell_env = False,
        execution_requirements = {
            "block-network": "1",
            "no-remote-exec": "1",
        },
    )
    return [DefaultInfo(files = depset([out]))]

orlix_locked_buildset = rule(
    implementation = _locked_buildset_impl,
    attrs = {
        "lock": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
        "_tool": attr.label(
            allow_single_file = True,
            default = Label("//bazel/promotion:locked_buildset.py"),
        ),
    },
)

def _require_one(files, name):
    if len(files) != 1:
        fail("promoted %s missing reconstructed OCI tree; run make __bazel-substitute-promoted" % name)
    return files[0]

def _promoted_rootfs_impl(ctx):
    initramfs = _require_one(ctx.files.initramfs, "rootfs initramfs")
    base_ext4 = _require_one(ctx.files.base_ext4, "rootfs base.ext4")
    state_ext4 = _require_one(ctx.files.state_ext4, "rootfs state.ext4")
    digest = _require_one(ctx.files.digest, "rootfs digest")
    metadata = _require_one(ctx.files.payload_metadata, "rootfs payload metadata")
    manifest = _require_one(ctx.files.file_manifest, "rootfs file manifest")
    stamp = ctx.actions.declare_file(ctx.label.name + "/rootfs.verified.sha256")
    ctx.actions.run_shell(
        mnemonic = "OrlixPromotedRootfs",
        progress_message = "Verifying reconstructed rootfs against artifacts.lock.json",
        command = r"""
set -euo pipefail
exec_root="$PWD"
lock="$exec_root/$1"
digest_file="$exec_root/$2"
stamp="$exec_root/$3"
got="$(/usr/bin/tr -d '[:space:]' < "$digest_file")"
want="$(/usr/bin/python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["components"]["rootfs"]["unsigned_digest"])' "$lock")"
test "$got" = "$want" || { echo "promoted rootfs digest $got does not match lock $want" >&2; exit 1; }
/bin/cp "$digest_file" "$stamp"
""",
        arguments = [ctx.file.lock.path, digest.path, stamp.path],
        inputs = [ctx.file.lock, digest],
        outputs = [stamp],
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )
    return [
        DefaultInfo(files = depset([initramfs, base_ext4, state_ext4, digest, stamp])),
        OrlixRootfsInfo(
            base_ext4 = base_ext4,
            base_tree = base_ext4,
            file_manifest = manifest,
            initramfs = initramfs,
            package_closure = digest,
            payload_metadata = metadata,
            source_input_digest = digest,
            state_ext4 = state_ext4,
            state_tree = state_ext4,
        ),
    ]

def _verify_component(ctx, component, digest):
    stamp = ctx.actions.declare_file(ctx.label.name + "/%s.verified.sha256" % component)
    ctx.actions.run_shell(
        mnemonic = "OrlixPromoted" + component.title(),
        progress_message = "Verifying reconstructed %s against artifacts.lock.json" % component,
        command = r"""
set -euo pipefail
exec_root="$PWD"
lock="$exec_root/$1"
digest_file="$exec_root/$2"
stamp="$exec_root/$3"
component="$4"
got="$(/usr/bin/tr -d '[:space:]' < "$digest_file")"
want="$(/usr/bin/python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["components"][sys.argv[2]]["unsigned_digest"])' "$lock" "$component")"
test "$got" = "$want" || { echo "promoted $component digest $got does not match lock $want" >&2; exit 1; }
/bin/cp "$digest_file" "$stamp"
""",
        arguments = [ctx.file.lock.path, digest.path, stamp.path, component],
        inputs = [ctx.file.lock, digest],
        outputs = [stamp],
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )
    return stamp

def _copy_sibling_tree(ctx, digest, relative, files, out_dir, mnemonic):
    ctx.actions.run_shell(
        mnemonic = mnemonic,
        command = r"""
set -euo pipefail
digest_file="$1"
dest="$2"
rel="$3"
src="$(/usr/bin/dirname "$digest_file")/$rel"
test -d "$src" || { echo "missing reconstructed tree $src" >&2; exit 1; }
/bin/mkdir -p "$dest"
/bin/cp -R "$src/." "$dest/"
""",
        arguments = [digest.path, out_dir.path, relative],
        inputs = [digest] + files,
        outputs = [out_dir],
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )

def _promoted_uapi_impl(ctx):
    digest = _require_one(ctx.files.digest, "uapi digest")
    archive = _require_one(ctx.files.archive, "uapi archive")
    manifest = _require_one(ctx.files.manifest, "uapi manifest")
    if not ctx.files.headers:
        fail("promoted uapi missing reconstructed headers; run make __bazel-substitute-promoted")
    stamp = _verify_component(ctx, "uapi", digest)
    headers = ctx.actions.declare_directory(ctx.label.name + "/headers")
    _copy_sibling_tree(ctx, digest, "uapi", ctx.files.headers, headers, "OrlixPromotedUapiHeaders")
    return [
        DefaultInfo(files = depset([digest, archive, manifest, stamp, headers])),
        OrlixInstalledUapiInfo(
            arch = "arm64",
            headers = headers,
            linux_revision = "6.12.105",
            uapi_digest = digest,
        ),
        OrlixLinuxArchiveInfo(
            archive = archive,
            build_manifest = manifest,
            destination = "host",
            profile = "release",
            source_input_digest = digest,
            symbol_manifest = manifest,
        ),
    ]

orlix_promoted_uapi = rule(
    implementation = _promoted_uapi_impl,
    attrs = {
        "lock": attr.label(allow_single_file = True, mandatory = True),
        "digest": attr.label(allow_files = True, mandatory = True),
        "headers": attr.label(allow_files = True, mandatory = True),
        "archive": attr.label(allow_files = True, mandatory = True),
        "manifest": attr.label(allow_files = True, mandatory = True),
    },
)

def _promoted_sysroot_impl(ctx):
    digest = _require_one(ctx.files.digest, "mlibc digest")
    uapi_digest = _require_one(ctx.files.uapi_digest, "mlibc consumed uapi digest")
    abi = _require_one(ctx.files.abi, "mlibc abi")
    loader = _require_one(ctx.files.loader, "mlibc loader")
    runtime = _require_one(ctx.files.runtime, "mlibc runtime")
    if not ctx.files.headers or not ctx.files.libraries:
        fail("promoted mlibc missing reconstructed sysroot; run make __bazel-substitute-promoted")
    stamp = _verify_component(ctx, "mlibc", digest)
    headers = ctx.actions.declare_directory(ctx.label.name + "/headers")
    libraries = ctx.actions.declare_directory(ctx.label.name + "/libraries")
    _copy_sibling_tree(ctx, digest, "headers", ctx.files.headers, headers, "OrlixPromotedMlibcHeaders")
    _copy_sibling_tree(ctx, digest, "libraries", ctx.files.libraries, libraries, "OrlixPromotedMlibcLibraries")
    return [
        DefaultInfo(files = depset([digest, abi, loader, runtime, stamp, headers, libraries])),
        OrlixLibcSysrootInfo(
            abi_manifest = abi,
            compiler_runtime = runtime,
            consumed_uapi_digest = uapi_digest,
            dynamic_loader = loader,
            headers = headers,
            libraries = libraries,
            sysroot_digest = digest,
            target_triple = "aarch64-linux-gnu",
        ),
    ]

orlix_promoted_sysroot = rule(
    implementation = _promoted_sysroot_impl,
    attrs = {
        "lock": attr.label(allow_single_file = True, mandatory = True),
        "digest": attr.label(allow_files = True, mandatory = True),
        "uapi_digest": attr.label(allow_files = True, mandatory = True),
        "headers": attr.label(allow_files = True, mandatory = True),
        "libraries": attr.label(allow_files = True, mandatory = True),
        "abi": attr.label(allow_files = True, mandatory = True),
        "loader": attr.label(allow_files = True, mandatory = True),
        "runtime": attr.label(allow_files = True, mandatory = True),
    },
)

def _promoted_apple_inputs_impl(ctx):
    stamps = []
    for dep in [ctx.attr.uapi, ctx.attr.sysroot, ctx.attr.rootfs]:
        for f in dep[DefaultInfo].files.to_list():
            if f.basename.endswith(".verified.sha256"):
                stamps.append(f)
    if len(stamps) != 3:
        fail("promoted Apple inputs need uapi, mlibc, and rootfs verified stamps, got %s" % [s.basename for s in stamps])
    return [DefaultInfo(files = depset(stamps))]

orlix_promoted_apple_inputs = rule(
    implementation = _promoted_apple_inputs_impl,
    attrs = {
        "uapi": attr.label(mandatory = True, providers = [OrlixInstalledUapiInfo]),
        "sysroot": attr.label(mandatory = True, providers = [OrlixLibcSysrootInfo]),
        "rootfs": attr.label(mandatory = True, providers = [OrlixRootfsInfo]),
    },
)

orlix_promoted_rootfs = rule(
    implementation = _promoted_rootfs_impl,
    attrs = {
        "lock": attr.label(allow_single_file = True, mandatory = True),
        "initramfs": attr.label(allow_files = True, mandatory = True),
        "base_ext4": attr.label(allow_files = True, mandatory = True),
        "state_ext4": attr.label(allow_files = True, mandatory = True),
        "digest": attr.label(allow_files = True, mandatory = True),
        "payload_metadata": attr.label(allow_files = True, mandatory = True),
        "file_manifest": attr.label(allow_files = True, mandatory = True),
    },
)
