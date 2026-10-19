"""Forward a configured compiler-object cache into a hermetic action environment.

The cache directory is not an action input. ccache hashes the compiler binary
(`CCACHE_COMPILERCHECK=content`), the compiler flags, and the preprocessed
translation unit, which includes transitive headers. A missing, corrupt, or
disabled cache does not change the compiler result.

Keep this control flow matched with `forward_shell_env` in
`bazel/compiler_cache.py`.
"""

def compiler_cache_env(shell):
    """Return CCACHE_DIR and the launcher when the cache is configured.

    `CCACHE_DISABLE` wins. The returned dict is the only cache state an action
    may copy into `env` while `use_default_shell_env` stays false.
    """
    disabled = shell.get("CCACHE_DISABLE")
    if disabled:
        return {"CCACHE_DISABLE": disabled}
    env = {}
    cache = shell.get("CCACHE_DIR")
    if cache:
        env["CCACHE_DIR"] = cache
    launcher = shell.get("ORLIX_COMPILER_LAUNCHER")
    if launcher:
        env["ORLIX_COMPILER_LAUNCHER"] = launcher
    return env
