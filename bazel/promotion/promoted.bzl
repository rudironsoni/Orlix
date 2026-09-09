"""Consume artifacts.lock.json in promoted mode. Never accept mutable latest."""

load("//bazel/providers:rootfs_info.bzl", "OrlixRootfsInfo")

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
    stamp = ctx.actions.declare_file(ctx.label.name + "/verified.sha256")
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
