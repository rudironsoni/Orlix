from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

from .context import repository_root
from .state import active_envelope, agent_root, read_json


PROOF_ORDER = (
    "kernel dependency",
    "kselftest",
    "mlibc",
    "orlixmlibc syscall/uapi",
    "posix shell",
    "jq",
    "curl",
    "zsh",
)


def revision(root: Path) -> str:
    return subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=root, check=True, text=True, capture_output=True
    ).stdout.strip()


def proof_state(root: Path) -> dict:
    value = read_json(agent_root(root) / "proof.json", {})
    if not isinstance(value, dict):
        raise ValueError("proof state must be an object")
    return value


def passed(records: object, requirement: str, current: str) -> bool:
    if not isinstance(records, list):
        return False
    for record in records:
        if not isinstance(record, dict) or record.get("requirement") != requirement:
            continue
        required = ("evidence_id", "artifact_identity", "destination", "profile")
        if (
            record.get("status") == "passed"
            and record.get("repository_revision") == current
            and all(record.get(key) for key in required)
        ):
            return True
    return False


def validate_completion(root: Path, envelope: dict | None = None, evidence: dict | None = None) -> list[str]:
    envelope = envelope or active_envelope(root)
    evidence = evidence or proof_state(root)
    current = revision(root)
    failures = [str(failure) for failure in evidence.get("known_failures", [])]
    resolved = set(evidence.get("resolved_failure_ids", []))
    lifecycle = read_json(agent_root(root) / "state.json", {})
    for failure in lifecycle.get("known_failures", []):
        if isinstance(failure, dict) and failure.get("id") not in resolved:
            failures.append(f"unresolved tool failure: {failure.get('id')}")
    for field, record_key in (
        ("required_proof", "proof"),
        ("build_intents", "build_intents"),
        ("verification_intents", "verification_intents"),
    ):
        for requirement in envelope.get(field, []):
            if not passed(evidence.get(record_key), requirement, current):
                failures.append(f"missing current {field}: {requirement}")
    required_text = " ".join(envelope.get("required_proof", [])).lower()
    highest = max((index for index, tier in enumerate(PROOF_ORDER) if tier in required_text), default=-1)
    hierarchy = evidence.get("proof_hierarchy", [])
    for tier in PROOF_ORDER[: highest + 1]:
        if not passed(hierarchy, tier, current):
            failures.append(f"missing proof hierarchy tier: {tier}")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=("check",))
    parser.parse_args()
    root = repository_root()
    try:
        failures = validate_completion(root)
    except ValueError as error:
        raise SystemExit(str(error)) from None
    if failures:
        raise SystemExit("completion proof failed:\n" + "\n".join(failures))
    print("completion proof current")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
