#!/usr/bin/env python3

from orlix_hook_common import (
    active_plan_dirs,
    block,
    is_git_commit_or_push,
    knowledge_updates_current,
    load_plan_context_state,
    oversized_goal_messages,
    parse_json,
    plan_context_loaded,
    read_stdin_text,
    repo_root,
    required_plan_context_paths,
    tool_requires_plan_context,
    flattened_text,
)


payload = parse_json(read_stdin_text())
text = flattened_text(payload)
root = repo_root()
state = load_plan_context_state(root)

if active_plan_dirs(root) and not plan_context_loaded(root, state) and tool_requires_plan_context(payload):
    required = ", ".join(str(path.relative_to(root)) for path in required_plan_context_paths(root))
    block(f"Active initiative context must be read before mutation: {required}.")

if is_git_commit_or_push(payload):
    if not knowledge_updates_current(state):
        block("Knowledge changes require docs/log.md and regenerated docs/index.md before git commit or push.")
    for message in oversized_goal_messages(root):
        block(message)

for variable in ("RUN_" + "VERY_EXPENSIVE_TESTS", "RUN_" + "EXPENSIVE_TESTS"):
    if f"{variable}=no" in text:
        block("Do not disable upstream expensive tests for an upstream conformance claim.")
