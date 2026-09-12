"""Consume artifacts.lock.json in promoted mode. Never accept mutable latest."""

load("//bazel:artifact_identity.bzl", "declare_artifact_identity")
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
            artifact_identity_digest = None,
            artifact_identity_manifest = None,
            base_ext4 = base_ext4,
            file_manifest = manifest,
            initramfs = initramfs,
            package_closure = depset(),
            payload_metadata = metadata,
            source_input_digest = digest,
            state_ext4 = state_ext4,
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
            artifact_identity_digest = None,
            artifact_identity_manifest = None,
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
            artifact_identity_digest = None,
            artifact_identity_manifest = None,
            compiler_runtime = runtime,
            compiler_runtime_identity_digest = None,
            compiler_runtime_identity_manifest = None,
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

_PROMOTED_KERNEL_COMPONENTS = [
    "kernel-release-iphoneos",
    "kernel-release-iphonesimulator",
    "kernel-development-iphoneos",
    "kernel-development-iphonesimulator",
]

def _promoted_kernel_impl(ctx):
    archive = _require_one(ctx.files.archive, "%s archive" % ctx.attr.component)
    release_dtb = _require_one(ctx.files.release_dtb, "%s release DTB" % ctx.attr.component)
    development_dtb = _require_one(ctx.files.development_dtb, "%s development DTB" % ctx.attr.component)
    identity_manifest = _require_one(ctx.files.identity_manifest, "%s artifact identity manifest" % ctx.attr.component)
    identity_digest = _require_one(ctx.files.identity_digest, "%s artifact identity digest" % ctx.attr.component)
    recomputed = declare_artifact_identity(
        ctx,
        "kernel",
        ctx.file._artifact_identity_serializer,
        artifacts = {
            "OrlixKernel.a": archive,
            "arch/orlix/boot/dts/development.dtb": development_dtb,
            "arch/orlix/boot/dts/release.dtb": release_dtb,
        },
    )
    verification = ctx.actions.declare_file(ctx.label.name + "/kernel.verified.sha256")
    verified_archive = ctx.actions.declare_file(ctx.label.name + "/verified/OrlixKernel.a")
    verified_release_dtb = ctx.actions.declare_file(ctx.label.name + "/verified/arch/orlix/boot/dts/release.dtb")
    verified_development_dtb = ctx.actions.declare_file(ctx.label.name + "/verified/arch/orlix/boot/dts/development.dtb")
    ctx.actions.run_shell(
        mnemonic = "OrlixPromotedKernel",
        progress_message = "Verifying promoted %s against artifacts.lock.json" % ctx.attr.component,
        command = r"""
set -euo pipefail
exec_root="$PWD"
lock="$exec_root/$1"
component="$2"
imported_manifest="$exec_root/$3"
imported_digest="$exec_root/$4"
computed_manifest="$exec_root/$5"
computed_digest="$exec_root/$6"
stamp="$exec_root/$7"
verified_archive="$exec_root/$8"
verified_release_dtb="$exec_root/$9"
verified_development_dtb="$exec_root/${10}"
/usr/bin/cmp -s "$imported_manifest" "$computed_manifest" || {
  echo "promoted $component artifact identity manifest differs from imported manifest" >&2
  exit 1
}
got="$(/usr/bin/tr -d '[:space:]' < "$imported_digest")"
computed="$(/usr/bin/tr -d '[:space:]' < "$computed_digest")"
test "$got" = "$computed" || {
  echo "promoted $component artifact identity digest differs from recomputed digest" >&2
  exit 1
}
want="$(/usr/bin/python3 -c 'import json,sys; payload=json.load(open(sys.argv[1])); assert payload["schema"] == 2; identity=payload["components"][sys.argv[2]]["artifact_identity"]; assert identity["format"] == "artifact-identity-v2" and identity["version"] == 2; print(identity["digest"])' "$lock" "$component")"
test "$got" = "$want" || {
  echo "promoted $component artifact identity digest differs from schema-2 lock" >&2
  exit 1
}
/bin/cp "$imported_digest" "$stamp"
/bin/mkdir -p "$(/usr/bin/dirname "$verified_archive")" "$(/usr/bin/dirname "$verified_release_dtb")" "$(/usr/bin/dirname "$verified_development_dtb")"
/bin/cp "$exec_root/${11}" "$verified_archive"
/bin/cp "$exec_root/${12}" "$verified_release_dtb"
/bin/cp "$exec_root/${13}" "$verified_development_dtb"
""",
        arguments = [
            ctx.file.lock.path,
            ctx.attr.component,
            identity_manifest.path,
            identity_digest.path,
            recomputed.manifest.path,
            recomputed.digest.path,
            verification.path,
            verified_archive.path,
            verified_release_dtb.path,
            verified_development_dtb.path,
            archive.path,
            release_dtb.path,
            development_dtb.path,
        ],
        inputs = [
            ctx.file.lock,
            archive,
            release_dtb,
            development_dtb,
            identity_manifest,
            identity_digest,
            recomputed.manifest,
            recomputed.digest,
        ],
        outputs = [verification, verified_archive, verified_release_dtb, verified_development_dtb],
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )
    profile = "release" if ctx.attr.component.startswith("kernel-release-") else "development"
    destination = "iphoneos" if ctx.attr.component.endswith("-iphoneos") else "iphonesimulator"
    boot_resources = depset([verified_release_dtb, verified_development_dtb])
    return [
        DefaultInfo(files = depset([
            verified_archive,
            verified_release_dtb,
            verified_development_dtb,
            identity_manifest,
            identity_digest,
            recomputed.manifest,
            recomputed.digest,
            verification,
        ])),
        OutputGroupInfo(
            archive = depset([verified_archive]),
            boot_resources = boot_resources,
            verification = depset([verification]),
        ),
        OrlixLinuxArchiveInfo(
            archive = verified_archive,
            artifact_identity_digest = identity_digest,
            artifact_identity_manifest = identity_manifest,
            boot_resources = boot_resources,
            build_manifest = None,
            destination = destination,
            product = None,
            profile = profile,
            source_input_digest = identity_digest,
            symbol_manifest = None,
        ),
    ]

orlix_promoted_kernel = rule(
    implementation = _promoted_kernel_impl,
    attrs = {
        "component": attr.string(mandatory = True, values = _PROMOTED_KERNEL_COMPONENTS),
        "lock": attr.label(allow_single_file = True, mandatory = True),
        "archive": attr.label_list(allow_files = True, mandatory = True),
        "release_dtb": attr.label_list(allow_files = True, mandatory = True),
        "development_dtb": attr.label_list(allow_files = True, mandatory = True),
        "identity_manifest": attr.label_list(allow_files = True, mandatory = True),
        "identity_digest": attr.label_list(allow_files = True, mandatory = True),
        "_artifact_identity_serializer": attr.label(
            allow_single_file = True,
            default = Label("//bazel:content_digest.py"),
        ),
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
