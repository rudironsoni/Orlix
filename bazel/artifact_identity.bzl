"""Declare a separate canonical artifact identity action."""

def declare_artifact_identity(ctx, name, serializer, tool_identity = None, root = None, artifacts = None):
    if not name:
        fail("artifact identity output name is required")
    if (root == None) == (artifacts == None):
        fail("artifact identity requires exactly one of root or artifacts")
    if artifacts != None and type(artifacts) != "dict":
        fail("artifact identity artifacts must be a dict of path to File")

    manifest = ctx.actions.declare_file(
        "%s/%s.artifact-identity-v2.json" % (ctx.label.name, name),
    )
    digest = ctx.actions.declare_file(
        "%s/%s.artifact-identity-v2.sha256" % (ctx.label.name, name),
    )
    arguments = [
        serializer.path,
        manifest.path,
        digest.path,
    ]
    inputs = [serializer]
    if tool_identity != None:
        arguments.append(tool_identity.path)
        inputs.append(tool_identity)
    else:
        arguments.append("")
    arguments.append("artifact-identity-v2")
    if root != None:
        arguments.extend(["tree", root.path])
        inputs.append(root)
    else:
        arguments.append("artifacts")
        for relative in sorted(artifacts.keys()):
            arguments.extend([relative, artifacts[relative].path])
            inputs.append(artifacts[relative])

    ctx.actions.run_shell(
        mnemonic = "OrlixArtifactIdentityV2",
        progress_message = "Serializing artifact identity for %s" % ctx.label,
        command = r"""
set -euo pipefail
exec_root="$PWD"
serializer="$exec_root/$1"
manifest="$exec_root/$2"
digest="$exec_root/$3"
tool_identity="$4"
format="$5"
mode="$6"
shift 6
serializer_dir="$(/usr/bin/dirname "$serializer")"
if [ -n "$tool_identity" ]; then test -s "$exec_root/$tool_identity"; fi
PYTHONPATH="$serializer_dir" /usr/bin/python3 -B -c '
import hashlib
import sys
from pathlib import Path

from content_digest import artifact_manifest_v2

mode, manifest_path, digest_path, format = sys.argv[1:5]
if mode == "tree":
    manifest = artifact_manifest_v2(Path(sys.argv[5]), format=format)
elif mode == "artifacts":
    values = sys.argv[5:]
    if len(values) % 2:
        raise ValueError("artifact identity arguments require path and source pairs")
    artifacts = dict(zip(values[0::2], (Path(value) for value in values[1::2])))
    manifest = artifact_manifest_v2(artifacts=artifacts, format="artifact-identity-v2")
else:
    raise ValueError("unknown artifact identity selection mode: %s" % mode)
Path(manifest_path).write_bytes(manifest)
Path(digest_path).write_text(hashlib.sha256(manifest).hexdigest() + "\n", encoding="ascii")
' "$mode" "$manifest" "$digest" "$format" "$@"
""",
        arguments = arguments,
        inputs = inputs,
        outputs = [manifest, digest],
        env = {
            "DEVELOPER_DIR": ctx.configuration.default_shell_env.get("ORLIX_PINNED_DEVELOPER_DIR", ""),
            "PATH": "/usr/bin:/bin",
            "PYTHONDONTWRITEBYTECODE": "1",
        },
        use_default_shell_env = False,
        execution_requirements = {
            "block-network": "1",
            "no-remote-cache": "1",
            "no-remote-exec": "1",
            "no-sandbox": "1",
        },
    )
    return struct(manifest = manifest, digest = digest)
