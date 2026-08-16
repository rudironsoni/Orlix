#!/usr/bin/env python3

from orlix_hook_common import (
    doing_work_pages,
    block,
    command_repo_root,
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

if doing_work_pages(root) and not plan_context_loaded(root, state) and tool_requires_plan_context(payload):
    required = ", ".join(str(path.relative_to(root)) for path in required_plan_context_paths(root))
    block(f"Doing epic, story, and task context must be read before mutation: {required}.")

if is_git_commit_or_push(payload):
    command_root = command_repo_root(payload, root)
    state = load_plan_context_state(command_root)
    if not knowledge_updates_current(command_root, state):
        block("Knowledge changes require docs/log.md and regenerated docs/index.md before git commit or push.")
    for message in oversized_goal_messages(command_root):
        block(message)

for variable in ("RUN_" + "VERY_EXPENSIVE_TESTS", "RUN_" + "EXPENSIVE_TESTS"):
    if f"{variable}=no" in text:
        block("Do not disable upstream expensive tests for an upstream conformance claim.")
