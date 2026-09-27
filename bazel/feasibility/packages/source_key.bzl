"""Tar-plus-stamp inputs so a C edit cannot stale-hit the output-base action cache."""

def _source_key_script(ctx):
    return ctx.file._source_key

def declare_source_key(ctx, name, files):
    """Return (tar, stamp) whose bytes are the declared files, in stable order."""
    if not files:
        fail("source key requires files: %s" % name)
    archive = ctx.actions.declare_file(ctx.label.name + "/" + name + ".tar")
    stamp = ctx.actions.declare_file(ctx.label.name + "/" + name + ".stamp")
    script = _source_key_script(ctx)
    arguments = [script.path, "files", archive.path, stamp.path]
    for source in files:
        arguments.append(source.short_path)
        arguments.append(source.path)
    ctx.actions.run_shell(
        mnemonic = "OrlixGuestPackageSourceKey",
        progress_message = "Recording declared bytes for %s" % name,
        command = "/usr/bin/python3 -B \"$1\" \"$2\" \"$3\" \"$4\" \"${@:5}\"",
        arguments = arguments,
        inputs = [script] + files,
        outputs = [archive, stamp],
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )
    return archive, stamp

def declare_tree_tar(ctx, name, tree):
    """Return a tar of one dependency directory so the package action keys on bytes."""
    archive = ctx.actions.declare_file(ctx.label.name + "/" + name + ".tar")
    script = _source_key_script(ctx)
    ctx.actions.run_shell(
        mnemonic = "OrlixGuestPackageTreeArchive",
        progress_message = "Archiving %s for %s" % (name, ctx.label.name),
        command = "/usr/bin/python3 -B \"$1\" tree \"$2\" \"$3\"",
        arguments = [script.path, archive.path, tree.path],
        inputs = [script, tree],
        outputs = [archive],
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )
    return archive
