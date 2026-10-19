"""Record jq's configure line with stable paths before it is compiled."""

from __future__ import annotations

from pathlib import Path


def jq_config_text(config: str, work: str, tool_bin: str) -> str:
    """Rewrite config.status --config the way the compiler prefix maps do."""
    text = config
    for old, new in sorted(
        ((tool_bin, "/orlix/tools"), (work, ".")),
        key=lambda item: len(item[0]),
        reverse=True,
    ):
        if old:
            text = text.replace(old, new)
    if "output-base" in text or "/Users/" in text:
        raise ValueError("jq configure line still records a host path")
    rendered = []
    for line in text.splitlines() or [""]:
        escaped = line.replace("\\", "\\\\").replace('"', '\\"')
        rendered.append(f'#define JQ_CONFIG "{escaped}"')
    return "\n".join(rendered) + "\n"


def write_config_opts(config: str, work: str, tool_bin: str, path: str) -> None:
    Path(path).write_text(jq_config_text(config, work, tool_bin))
