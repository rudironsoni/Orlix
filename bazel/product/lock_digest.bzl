"""Read artifacts.lock.json at fetch time so the stamp command can name its digest.

Bazel 9 aquery action keys omit source-byte digests. Interpolating this digest
into OrlixKernelCompositionStamp makes a lock-only edit a new stamp action.
OrlixKernelComposition does not load this repository.
"""

_HEX = "0123456789abcdef"

def _lock_digest_impl(repository_ctx):
    path = repository_ctx.path(repository_ctx.attr.lock)
    repository_ctx.watch(path)
    payload = json.decode(repository_ctx.read(path))
    buildset = payload.get("buildset")
    if type(buildset) != "string" or len(buildset) != 64:
        fail("artifacts.lock.json buildset must be 64 hex characters")
    for character in buildset.elems():
        if character not in _HEX:
            fail("artifacts.lock.json buildset must be lowercase hex")
    repository_ctx.file("digest.bzl", 'LOCKED_BUILDSET = "%s"\n' % buildset)
    repository_ctx.file("BUILD.bazel", 'exports_files(["digest.bzl"])\n')

lock_digest = repository_rule(
    implementation = _lock_digest_impl,
    attrs = {
        "lock": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
    },
)
