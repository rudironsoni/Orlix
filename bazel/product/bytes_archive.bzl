"""Provider stand-in so composition action keys can be queried without Xcode.

The product app does not depend on this rule. OrlixKernelMachOArchive still
owns the real archive and still requires DEVELOPER_DIR.
"""

load(
    "//bazel/providers:kernel_info.bzl",
    "OrlixLinuxArchiveInfo",
)

def _bytes_archive_impl(ctx):
    return [OrlixLinuxArchiveInfo(
        archive = ctx.file.archive,
        artifact_identity_digest = ctx.file.digest,
        artifact_identity_manifest = None,
        boot_resources = depset(ctx.files.boot),
        build_manifest = None,
        destination = "iphonesimulator",
        product = None,
        profile = "release",
        source_input_digest = None,
        symbol_manifest = None,
    )]

orlix_bytes_archive = rule(
    implementation = _bytes_archive_impl,
    attrs = {
        "archive": attr.label(allow_single_file = True, mandatory = True),
        "digest": attr.label(allow_single_file = True, mandatory = True),
        "boot": attr.label_list(allow_files = True, mandatory = True),
    },
)
