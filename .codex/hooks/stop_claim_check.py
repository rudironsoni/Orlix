#!/usr/bin/env python3

from orlix_hook_common import (
    doing_work_pages,
    flattened_text,
    invented_mechanism_without_scope,
    load_plan_context_state,
    macos_runtime_wording,
    oci_compatible_from_image_only,
    oci_runtime_claim_without_lifecycle,
    orlix_run_claim_without_lifecycle,
    parse_json,
    plan_context_loaded,
    read_stdin_text,
    repo_root,
    vague_container_support,
    warn,
    workflow_without_authorization,
)


payload = parse_json(read_stdin_text())
text = flattened_text(payload)
root = repo_root()

if doing_work_pages(root) and not plan_context_loaded(root, load_plan_context_state(root)):
    warn("Final status was written without loading AGENTS.md, docs/index.md, and every active epic page.")
if macos_runtime_wording(text):
    warn("macOS runtime wording detected. Orlix product runtime proof requires the app-hosted destination defined by the owning work hierarchy.")
if vague_container_support(text):
    warn("Generic container support wording detected. Name the exact OCI or Docker compatibility boundary.")
if invented_mechanism_without_scope(text):
    warn("A new named mechanism appears without a canonical ownership boundary.")
if workflow_without_authorization(text):
    warn("Delegated workflow wording appears without explicit authorization.")
if oci_runtime_claim_without_lifecycle(text) or orlix_run_claim_without_lifecycle(text):
    warn("OCI runtime wording requires create, start, state, kill, and delete lifecycle evidence.")
if oci_compatible_from_image_only(text):
    warn("Image import evidence cannot establish OCI Runtime Spec compatibility.")
