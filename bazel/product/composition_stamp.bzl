"""Record the locked buildset on a sibling of the byte-hashed composition.

Inputs are the already-hashed composition.json plus OrlixLockedBuildset.
The digest in the promoted command comes from @orlix_lock_digest, which reads
artifacts.lock.json. OrlixKernelComposition does not load that repository, so
a lock-only edit does not change its action key or the kernel archive key.
"""

load("@orlix_lock_digest//:digest.bzl", "LOCKED_BUILDSET")

def _kernel_composition_stamp_impl(ctx):
    composition = ctx.file.composition
    out = ctx.actions.declare_file(ctx.label.name + "/composition.json")
    tool = ctx.file._tool
    if ctx.attr.record_promoted:
        locked = ctx.file.locked_buildset
        if locked == None:
            fail("promoted composition stamp requires OrlixLockedBuildset")
        if len(LOCKED_BUILDSET) != 64:
            fail("promoted composition stamp requires the locked buildset digest")
        command = """
set -euo pipefail
exec_root="$PWD"
/usr/bin/python3 -B "$exec_root/$1" --promoted --composition "$exec_root/$2" --locked "$exec_root/$3" --expected "$4" --out "$exec_root/$5"
"""
        arguments = [tool.path, composition.path, locked.path, LOCKED_BUILDSET, out.path]
        inputs = [tool, composition, locked]
        progress = "Recording locked buildset on the kernel composition stamp"
    else:
        command = """
set -euo pipefail
exec_root="$PWD"
/usr/bin/python3 -B "$exec_root/$1" --source --composition "$exec_root/$2" --out "$exec_root/$3"
"""
        arguments = [tool.path, composition.path, out.path]
        inputs = [tool, composition]
        progress = "Copying source kernel composition without promoted provenance"
    ctx.actions.run_shell(
        mnemonic = "OrlixKernelCompositionStamp",
        progress_message = progress,
        command = command,
        arguments = arguments,
        inputs = inputs,
        outputs = [out],
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )
    return [DefaultInfo(files = depset([out]))]

orlix_kernel_composition_stamp = rule(
    implementation = _kernel_composition_stamp_impl,
    attrs = {
        "composition": attr.label(allow_single_file = True, mandatory = True),
        "locked_buildset": attr.label(allow_single_file = True),
        "record_promoted": attr.bool(mandatory = True),
        "_tool": attr.label(
            allow_single_file = True,
            default = Label("//bazel/product:composition_stamp.py"),
        ),
    },
)
