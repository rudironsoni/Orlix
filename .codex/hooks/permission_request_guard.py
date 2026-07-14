#!/usr/bin/env python3
from orlix_hook_common import (
    block,
    flattened_text,
    parse_json,
    read_stdin_text,
    warn,
)

payload = parse_json(read_stdin_text())
text = flattened_text(payload)

if 'prefix_rule": ["python3"]' in text or "prefix_rule = [\"python3\"]" in text:
    block("Do not request broad Python escalation rules. Request a narrow command prefix.")

if "rm -rf" in text:
    warn("Destructive deletion requires explicit task scope and should be as narrow as possible.")
