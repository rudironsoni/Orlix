"""Read-only compiler runtime cache consumer.

The product archive is configured by rules_apple's apple_platform_split_transition.
Command-line copies of that transition do not reproduce its output directory.
"""

load("@rules_apple//apple/internal:transition_support.bzl", "transition_support")

def _orlix_compiler_runtime_cache_consumer_impl(ctx):
    files = []
    for dep in ctx.split_attr.runtime.values():
        files.append(dep[DefaultInfo].files)
    return [DefaultInfo(files = depset(transitive = files))]

orlix_compiler_runtime_cache_consumer = rule(
    implementation = _orlix_compiler_runtime_cache_consumer_impl,
    doc = "Configures //bazel/feasibility/mlibc:compiler_runtime with the Apple platform split.",
    attrs = {
        "runtime": attr.label(
            cfg = transition_support.apple_platform_split_transition,
            doc = "Compiler runtime configured by apple_platform_split_transition.",
            mandatory = True,
        ),
        "minimum_os_version": attr.string(
            default = "15.0",
            doc = "Minimum OS passed through the Apple split transition.",
        ),
        "platform_type": attr.string(
            default = "ios",
            doc = "Apple platform type passed through the Apple split transition.",
        ),
        "_allowlist_function_transition": attr.label(
            default = "@bazel_tools//tools/allowlists/function_transition_allowlist",
        ),
    },
)
