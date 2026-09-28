"""Narrow HostAdapter + boot + Linux archive composition. Does not fake an xcframework.

Consumer identity is artifact-identity-v2 of stable logical paths and file bytes.
Origin exec paths, source_input_digest, the lock, and proof sidecars are not
action inputs. Profile, destination, linux revision, and target triple stay on
the archive provider for the fail-closed kernel check and are not interpolated
into this command. Promoted buildset provenance is the sibling stamp in
composition_stamp.bzl. This action always writes "buildset": null.

The product composition records the HostAdapter edge from nm of the Linux
archive, the HostAdapter link outputs, and the boot link outputs. The action-key
probe does not claim that edge.
"""

load("@rules_cc//cc/common:cc_info.bzl", "CcInfo")
load(
    "//bazel/providers:kernel_info.bzl",
    "OrlixKernelAppleProductInfo",
    "OrlixLinuxArchiveInfo",
)

_GUEST_MARKERS = ("OrlixMLibC", "OrlixCoreUtils", "libc.a", "libcoreutils")
_ARCHIVE_ORDER = ["OrlixKernel.a", "OrlixHostAdapter", "OrlixKernelBoot"]
_FRAMEWORKS = ["CoreFoundation", "Foundation"]

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

def _reject_guest(files):
    for item in files:
        for marker in _GUEST_MARKERS:
            if marker in item.path:
                fail("guest archive is not a HostAdapter link input: %s" % item.path)

def _file_list(value):
    if value == None:
        return []
    if hasattr(value, "to_list"):
        return value.to_list()
    return [value]

def _extend_objects(files, library):
    static = getattr(library, "static_library", None)
    if static:
        files.append(static)
        return
    pic_static = getattr(library, "pic_static_library", None)
    if pic_static:
        files.append(pic_static)
        return
    objects = _file_list(getattr(library, "objects", None))
    if not objects:
        objects = _file_list(getattr(library, "pic_objects", None))
    files.extend(objects)

def _own_link_objects(target):
    files = []
    if CcInfo in target:
        for linker_input in target[CcInfo].linking_context.linker_inputs.to_list():
            if linker_input.owner != target.label:
                continue
            for library in linker_input.libraries:
                _extend_objects(files, library)
    if not files:
        files = [
            item
            for item in target[DefaultInfo].files.to_list()
            if item.extension in ["a", "o", "lo"]
        ]
    if not files:
        fail("symbol edge requires link objects from %s" % target.label)
    _reject_guest(files)
    return files

def _identity_preamble():
    return r"""
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
"""

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
    inputs = [ctx.file._artifact_identity_serializer] + linux_files + hostadapter_files + boot_files
    if ctx.attr.record_symbol_edge:
        if ctx.attr.archive_order != _ARCHIVE_ORDER:
            fail("HostAdapter composition archive order is fixed")
        if ctx.attr.frameworks != _FRAMEWORKS:
            fail("HostAdapter composition frameworks are CoreFoundation and Foundation")
        if ctx.attr.hostadapter_library == None or ctx.attr.boot_library == None:
            fail("symbol edge requires HostAdapter and boot link outputs")
        host_objects = _own_link_objects(ctx.attr.hostadapter_library)
        boot_objects = _own_link_objects(ctx.attr.boot_library)
        _reject_guest([archive.archive])
        command = _identity_preamble() + r"""
edge_py="$exec_root/$1"
linux_archive="$exec_root/$2"
host_object_count="$3"
boot_object_count="$4"
shift 4
host_objects=()
i=0
while [ "$i" -lt "$host_object_count" ]; do
  host_objects+=("$exec_root/$1")
  shift
  i=$((i + 1))
done
boot_objects=()
i=0
while [ "$i" -lt "$boot_object_count" ]; do
  boot_objects+=("$exec_root/$1")
  shift
  i=$((i + 1))
done
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
nm_cmd="$(/usr/bin/command -v llvm-nm || true)"
if [ -z "$nm_cmd" ]; then nm_cmd=/usr/bin/nm; fi
test -x "$nm_cmd"
temporary="$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/orlix-hostadapter-edge.XXXXXX")"
trap '/bin/rm -rf "$temporary"' EXIT
dump_nm() {
  out="$1"
  shift
  : > "$out"
  for file in "$@"; do
    test -s "$file"
    "$nm_cmd" -P "$file" >> "$out"
  done
  test -s "$out"
}
dump_nm "$temporary/linux.txt" "$linux_archive"
dump_nm "$temporary/host.txt" "${host_objects[@]}"
dump_nm "$temporary/boot.txt" "${boot_objects[@]}"
link_args=()
for file in "$linux_archive" "${host_objects[@]}" "${boot_objects[@]}"; do
  link_args+=(--link-path "$file")
done
/usr/bin/python3 -B "$edge_py" \
  --linux-nm "$temporary/linux.txt" \
  --host-nm "$temporary/host.txt" \
  --boot-nm "$temporary/boot.txt" \
  --linux-digest "$digest" \
  --host-digest "$host_digest" \
  --boot-digest "$boot_digest" \
  --archive-order OrlixKernel.a \
  --archive-order OrlixHostAdapter \
  --archive-order OrlixKernelBoot \
  --framework CoreFoundation \
  --framework Foundation \
  --out "$manifest_out" \
  "${link_args[@]}"
"""
        arguments = [
            ctx.file._artifact_identity_serializer.path,
            manifest.path,
            str(len(linux_files)),
            str(len(hostadapter_files)),
            str(len(boot_files)),
            ctx.file._edge_tool.path,
            archive.archive.path,
            str(len(host_objects)),
            str(len(boot_objects)),
        ] + [item.path for item in host_objects] + [item.path for item in boot_objects] + identity_args
        inputs = inputs + host_objects + boot_objects + [ctx.file._edge_tool]
        env = {
            "PATH": "/opt/homebrew/opt/llvm/bin:/usr/bin:/bin",
            "PYTHONDONTWRITEBYTECODE": "1",
        }
    else:
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
  '  "undefined_kernel_symbols": null,' \
  '  "buildset": null,' \
  '  "xcframework": null,' \
  '  "wrapper_makefile": false,' \
  '  "symbol_edge": false' \
  '}' > "$manifest_out"
"""
        arguments = [
            ctx.file._artifact_identity_serializer.path,
            manifest.path,
            str(len(linux_files)),
            str(len(hostadapter_files)),
            str(len(boot_files)),
        ] + identity_args
        env = {}
    ctx.actions.run_shell(
        mnemonic = "OrlixKernelComposition",
        progress_message = "Recording HostAdapter, boot, and Linux archive composition",
        command = command,
        arguments = arguments,
        inputs = inputs,
        outputs = [manifest],
        env = env,
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
        "_edge_tool": attr.label(
            allow_single_file = True,
            default = Label("//bazel/product:hostadapter_edge.py"),
        ),
        "linux_archive": attr.label(mandatory = True, providers = [OrlixLinuxArchiveInfo]),
        "hostadapter": attr.label(mandatory = True, allow_files = True),
        "boot": attr.label(mandatory = True, allow_files = True),
        "hostadapter_library": attr.label(),
        "boot_library": attr.label(),
        "archive_order": attr.string_list(),
        "frameworks": attr.string_list(),
        "record_symbol_edge": attr.bool(default = False),
    },
)
