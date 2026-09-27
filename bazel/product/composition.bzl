"""Narrow HostAdapter + boot + Linux archive composition. Does not fake an xcframework.

Consumer identity is artifact-identity-v2 of stable logical paths and file bytes.
Origin exec paths, source_input_digest, the lock, and proof sidecars are not
action inputs. Profile, destination, linux revision, and target triple stay on
the archive provider for the fail-closed kernel check and are not interpolated
into this command.
"""

load(
    "//bazel/providers:kernel_info.bzl",
    "OrlixKernelAppleProductInfo",
    "OrlixLinuxArchiveInfo",
)

def _logical_source_path(file):
    logical = file.short_path
    if (
        logical.startswith("/") or
        logical.startswith("../") or
        logical.startswith("bazel-out/") or
        logical.startswith("external/")
    ):
        fail("origin exec path is not a consumer logical path: %s" % logical)
    return logical

def _unique_names(names):
    seen = {}
    for name in names:
        if name in seen:
            fail("duplicate logical path: %s" % name)
        seen[name] = True

def _pairs(names, files):
    args = []
    for name, item in zip(names, files):
        args.extend([name, item.path])
    return args

def _kernel_composition_impl(ctx):
    archive = ctx.attr.linux_archive[OrlixLinuxArchiveInfo]
    if archive.artifact_identity_digest == None:
        fail("kernel composition requires artifact identity of the selected archive bytes")
    if archive.boot_resources == None:
        fail("kernel composition requires selected boot resources")
    manifest = ctx.actions.declare_file(ctx.label.name + "/composition.json")
    hostadapter_files = sorted(ctx.files.hostadapter, key = lambda item: item.short_path)
    boot_files = sorted(ctx.files.boot, key = lambda item: item.short_path)
    if len(hostadapter_files) == 0:
        fail("kernel composition requires HostAdapter sources")
    if len(boot_files) == 0:
        fail("kernel composition requires OrlixKernel boot sources")
    linux_boot = sorted(archive.boot_resources.to_list(), key = lambda item: item.basename)
    linux_names = ["OrlixKernel.a"] + [
        "arch/orlix/boot/dts/" + item.basename
        for item in linux_boot
    ]
    linux_files = [archive.archive] + linux_boot
    host_names = [_logical_source_path(item) for item in hostadapter_files]
    boot_names = [_logical_source_path(item) for item in boot_files]
    _unique_names(linux_names + host_names + boot_names)
    identity_args = _pairs(linux_names, linux_files) + _pairs(host_names, hostadapter_files) + _pairs(boot_names, boot_files)
    ctx.actions.run_shell(
        mnemonic = "OrlixKernelComposition",
        progress_message = "Recording HostAdapter, boot, and Linux archive composition",
        command = r"""
set -euo pipefail
exec_root="$PWD"
serializer="$exec_root/$1"
manifest_out="$exec_root/$2"
linux_count="$3"
host_count="$4"
boot_count="$5"
shift 5
identity() {
  PYTHONPATH="$(/usr/bin/dirname "$serializer")" /usr/bin/python3 -B -c '
import sys
from pathlib import Path
from content_digest import artifact_identity_v2
values = sys.argv[1:]
if len(values) % 2:
    raise SystemExit("identity arguments require logical path and source pairs")
pairs = list(zip(values[0::2], (Path(path) for path in values[1::2])))
print(artifact_identity_v2(artifacts=pairs))
' "$@"
}
linux_args=()
i=0
while [ "$i" -lt "$linux_count" ]; do
  linux_args+=("$1" "$exec_root/$2")
  shift 2
  i=$((i + 1))
done
host_args=()
i=0
while [ "$i" -lt "$host_count" ]; do
  host_args+=("$1" "$exec_root/$2")
  shift 2
  i=$((i + 1))
done
boot_args=()
i=0
while [ "$i" -lt "$boot_count" ]; do
  boot_args+=("$1" "$exec_root/$2")
  shift 2
  i=$((i + 1))
done
digest="$(identity "${linux_args[@]}")"
host_digest="$(identity "${host_args[@]}")"
boot_digest="$(identity "${boot_args[@]}")"
test "${#digest}" -eq 64
test "${#host_digest}" -eq 64
test "${#boot_digest}" -eq 64
/usr/bin/printf '%s\n' '{' \
  '  "component": "OrlixKernel-composition",' \
  '  "linux_archive": "'"$digest"'",' \
  '  "hostadapter": "'"$host_digest"'",' \
  '  "boot": "'"$boot_digest"'",' \
  '  "linked_symbol": "_arch_boot_entry",' \
  '  "undefined_kernel_symbols": [],' \
  '  "buildset": null,' \
  '  "xcframework": null,' \
  '  "wrapper_makefile": false' \
  '}' > "$manifest_out"
""",
        arguments = [
            ctx.file._artifact_identity_serializer.path,
            manifest.path,
            str(len(linux_files)),
            str(len(hostadapter_files)),
            str(len(boot_files)),
        ] + identity_args,
        inputs = [ctx.file._artifact_identity_serializer] + linux_files + hostadapter_files + boot_files,
        outputs = [manifest],
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )
    return [
        DefaultInfo(files = depset([manifest])),
        OrlixKernelAppleProductInfo(
            archive_dependency_digest = archive.artifact_identity_digest,
            apple_metadata = manifest,
            destination_slices = ["iphonesimulator", "iphoneos"],
            xcframework = None,
        ),
    ]

orlix_kernel_composition = rule(
    implementation = _kernel_composition_impl,
    attrs = {
        "_artifact_identity_serializer": attr.label(
            allow_single_file = True,
            default = Label("//bazel:content_digest.py"),
        ),
        "linux_archive": attr.label(mandatory = True, providers = [OrlixLinuxArchiveInfo]),
        "hostadapter": attr.label(mandatory = True, allow_files = True),
        "boot": attr.label(mandatory = True, allow_files = True),
        # Retained so the product graph can still name a lock target. The lock
        # is not an action input: a lock-record change is not a content change.
        "locked_buildset": attr.label(allow_files = True),
    },
)
