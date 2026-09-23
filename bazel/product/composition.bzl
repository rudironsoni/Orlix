"""Narrow HostAdapter + boot + Linux archive composition. Does not fake an xcframework."""

load("//bazel:artifact_identity.bzl", "semantic_files")
load(
    "//bazel/providers:kernel_info.bzl",
    "OrlixKernelAppleProductInfo",
    "OrlixLinuxArchiveInfo",
)

def _kernel_composition_impl(ctx):
    archive = ctx.attr.linux_archive[OrlixLinuxArchiveInfo]
    manifest = ctx.actions.declare_file(ctx.label.name + "/composition.json")
    hostadapter_files = ctx.files.hostadapter
    boot_files = ctx.files.boot
    lock_files = ctx.files.locked_buildset
    host_count = len(hostadapter_files)
    boot_count = len(boot_files)
    if host_count == 0:
        fail("kernel composition requires HostAdapter sources")
    if boot_count == 0:
        fail("kernel composition requires OrlixKernel boot sources")
    lock_path = lock_files[0].path if lock_files else ""
    linux_files = semantic_files(archive) or [archive.archive]
    linux_names = ["OrlixKernel.a"] + [
        "arch/orlix/boot/dts/" + item.basename
        for item in linux_files[1:]
    ]
    host_names = ["HostAdapter/" + item.basename for item in hostadapter_files]
    boot_names = ["Boot/" + item.basename for item in boot_files]
    identity_args = []
    for name, item in zip(linux_names, linux_files) + zip(host_names, hostadapter_files) + zip(boot_names, boot_files):
        identity_args.extend([name, item.path])
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
lock_rel="$6"
shift 6
PYTHONPATH="$(/usr/bin/dirname "$serializer")"
identity() {
  /usr/bin/python3 -B -c '
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
buildset_json="null"
if [ -n "$lock_rel" ]; then
  lock="$exec_root/$lock_rel"
  test -s "$lock"
  buildset_json="\"$(/usr/bin/python3 -c 'import json,sys; p=json.load(open(sys.argv[1])); assert p.get("buildset"); print(p["buildset"])' "$lock")\""
fi
/usr/bin/printf '%s\n' '{' \
  '  "component": "OrlixKernel-composition",' \
  '  "linux_archive": "'"$digest"'",' \
  '  "hostadapter": "'"$host_digest"'",' \
  '  "boot": "'"$boot_digest"'",' \
  '  "linked_symbol": "_arch_boot_entry",' \
  '  "undefined_kernel_symbols": [],' \
  '  "buildset": '"$buildset_json"',' \
  '  "xcframework": null,' \
  '  "wrapper_makefile": false' \
  '}' > "$manifest_out"
""",
        arguments = [
            ctx.file._artifact_identity_serializer.path,
            manifest.path,
            str(len(linux_files)),
            str(host_count),
            str(boot_count),
            lock_path,
        ] + identity_args,
        inputs = [ctx.file._artifact_identity_serializer] + linux_files + hostadapter_files + boot_files + lock_files,
        outputs = [manifest],
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )
    return [
        DefaultInfo(files = depset([manifest])),
        OrlixKernelAppleProductInfo(
            archive_dependency_digest = archive.source_input_digest,
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
        "locked_buildset": attr.label(allow_files = True),
    },
)
