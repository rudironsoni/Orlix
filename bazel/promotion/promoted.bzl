"""Consume artifacts.lock.json in promoted mode. Never accept mutable latest."""

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
