"""Forward a configured compiler-object cache into a hermetic action environment.

The cache directory is not an action input. ccache hashes the compiler binary
(`CCACHE_COMPILERCHECK=content`), the compiler flags, and the preprocessed
translation unit, which includes transitive headers. A missing, corrupt, or
disabled cache does not change the compiler result.

Keep this control flow matched with `forward_shell_env` in
`bazel/compiler_cache.py`.
"""

def compiler_cache_env(shell, include_directory = True):
    """Return cache switches an action may copy into its fixed environment.

    `CCACHE_DISABLE` wins. Guest packages pass `include_directory = False` so
    `CCACHE_DIR` stays an inherited client variable and is not an action key.
    The statistics log is not an action input.
    """
    disabled = shell.get("CCACHE_DISABLE")
    if disabled:
        return {"CCACHE_DISABLE": disabled}
    env = {}
    if include_directory:
        cache = shell.get("CCACHE_DIR")
        if cache:
            env["CCACHE_DIR"] = cache
    launcher = shell.get("ORLIX_COMPILER_LAUNCHER")
    if launcher:
        env["ORLIX_COMPILER_LAUNCHER"] = launcher
    return env
