---
name: rulesync
description: >-
  Use when changing or validating portable agent configuration under
  .rulesync or generated agent-tool files.
targets: ["*"]
---

# RuleSync

Use `.rulesync/` and `rulesync.jsonc` as source. Do not edit generated tool files.

Run `make agent-rules-check` before a source change is complete. Use the [live file-format reference](https://rulesync.dyoshikawa.com/reference/file-formats) for current syntax.

The repository pins the RuleSync executable version in `.rulesync/VERSION`. Do not import generated output over canonical source.
