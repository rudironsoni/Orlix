#!/usr/bin/env python3
from orlix_hook_common import (
    block,
    external_ssd_bypass_violation,
    flattened_text,
    generated_tree_write_violation,
    parse_json,
    read_stdin_text,
    repo_root,
    selected_task_policy,
    unauthorized_physical_command,
    unauthorized_release_command,
    unauthorized_tcti_runtime_write,
    warn,
)

payload = parse_json(read_stdin_text())
text = flattened_text(payload)
policy = selected_task_policy(repo_root())

if generated_tree_write_violation(payload):
    block("Do not request permission to mutate generated upstream/build trees.")

if unauthorized_tcti_runtime_write(payload, policy):
    block("Permission cannot override runtime_patch_allowed=false or the selected runtime path scope.")

if unauthorized_physical_command(payload, policy):
    block("Permission cannot override physical_device_allowed=false. Complete the simulator frontier first.")

if unauthorized_release_command(payload, policy):
    block("Permission cannot override release_gate_eligible=false. Complete simulator and device promotion first.")

if external_ssd_bypass_violation(payload):
    block("Permission cannot bypass the external-SSD Xcode storage contract. Use the configured wrappers and xcode-offload doctor --root \"$(external-ssd-root)\" --require-shims --strict.")

if 'prefix_rule": ["python3"]' in text or "prefix_rule = [\"python3\"]" in text:
    block("Do not request broad Python escalation rules. Request a narrow command prefix.")

if "rm -rf" in text:
    warn("Destructive deletion requires explicit task scope and should be as narrow as possible.")
