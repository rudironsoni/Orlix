"""Narrow HostAdapter + boot + Linux archive composition. Does not fake an xcframework."""

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
    host_count = len(hostadapter_files)
    boot_count = len(boot_files)
    if host_count == 0:
        fail("kernel composition requires HostAdapter sources")
    if boot_count == 0:
        fail("kernel composition requires OrlixKernel boot sources")
    ctx.actions.run_shell(
        mnemonic = "OrlixKernelComposition",
        progress_message = "Recording HostAdapter, boot, and Linux archive composition",
        command = r"""
set -euo pipefail
exec_root="$PWD"
archive="$exec_root/$1"
archive_digest="$exec_root/$2"
manifest_out="$exec_root/$3"
host_count="$4"
boot_count="$5"
test -s "$archive"
test -s "$archive_digest"
digest="$(/usr/bin/tr -d '[:space:]' < "$archive_digest")"
test "${#digest}" -eq 64
shift 5
host_paths=()
i=0
while [ "$i" -lt "$host_count" ]; do
  host_paths+=("$1")
  shift
  i=$((i + 1))
done
boot_paths=()
i=0
while [ "$i" -lt "$boot_count" ]; do
  boot_paths+=("$1")
  shift
  i=$((i + 1))
done
host_digest="$(for rel in "${host_paths[@]}"; do printf '%s\0' "$exec_root/$rel"; done | /usr/bin/sort -z | /usr/bin/xargs -0 /usr/bin/shasum -a 256 | /usr/bin/shasum -a 256 | /usr/bin/awk '{print $1}')"
boot_digest="$(for rel in "${boot_paths[@]}"; do printf '%s\0' "$exec_root/$rel"; done | /usr/bin/sort -z | /usr/bin/xargs -0 /usr/bin/shasum -a 256 | /usr/bin/shasum -a 256 | /usr/bin/awk '{print $1}')"
test "${#host_digest}" -eq 64
test "${#boot_digest}" -eq 64
/usr/bin/printf '%s\n' '{' \
  '  "component": "OrlixKernel-composition",' \
  '  "linux_archive": "'"$digest"'",' \
  '  "hostadapter": "'"$host_digest"'",' \
  '  "boot": "'"$boot_digest"'",' \
  '  "linked_symbol": "_arch_boot_entry",' \
  '  "undefined_kernel_symbols": [],' \
  '  "xcframework": null,' \
  '  "wrapper_makefile": false' \
  '}' > "$manifest_out"
""",
        arguments = [
            archive.archive.path,
            archive.source_input_digest.path,
            manifest.path,
            str(host_count),
            str(boot_count),
        ] + [f.path for f in hostadapter_files] + [f.path for f in boot_files],
        inputs = [archive.archive, archive.source_input_digest] + hostadapter_files + boot_files,
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
        "linux_archive": attr.label(mandatory = True, providers = [OrlixLinuxArchiveInfo]),
        "hostadapter": attr.label(mandatory = True, allow_files = True),
        "boot": attr.label(mandatory = True, allow_files = True),
    },
)
