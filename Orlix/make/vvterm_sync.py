"""Three-way vvterm snapshot synchronization for the tracked Orlix app source."""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import tempfile
from typing import Iterable


UPSTREAM_REPOSITORY = "https://github.com/vivy-company/vvterm.git"
FULL_SHA = re.compile(r"^[0-9a-f]{40}$")
BRAND_TOKEN = re.compile(r"VVTerm|vvterm")
CONFLICT_MARKERS = (b"<<<<<<< ", b"=======", b">>>>>>> ")
IGNORED_SCAN_PARTS = {".build", "__pycache__", "xcuserdata"}


class SyncError(RuntimeError):
    pass


def run(
    command: list[str],
    *,
    cwd: Path | None = None,
    check: bool = True,
    capture: bool = True,
) -> subprocess.CompletedProcess[bytes]:
    result = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
    )
    if check and result.returncode != 0:
        stderr = result.stderr.decode("utf-8", errors="replace") if result.stderr else ""
        raise SyncError(f"command failed ({result.returncode}): {' '.join(command)}\n{stderr}")
    return result


def repository_root() -> Path:
    return Path(__file__).resolve().parents[2]


def load_json(path: Path) -> dict:
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise SyncError(f"cannot read JSON file {path}: {error}") from error


def write_json(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2, sort_keys=False) + "\n", encoding="utf-8")


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_commit(commit: str) -> None:
    if not FULL_SHA.fullmatch(commit):
        raise SyncError(f"VVTERM_COMMIT must be a full lowercase 40-character SHA, got: {commit!r}")


def path_matches(path: str, patterns: Iterable[str]) -> bool:
    return any(fnmatch.fnmatchcase(path, pattern) for pattern in patterns)


def mapped_path(path: str, policy: dict) -> str:
    result = path
    for old, new in policy["path_replacements"]:
        result = result.replace(old, new)
    return result


def transform_text(data: bytes, relative_path: str, policy: dict) -> bytes:
    if path_matches(relative_path, policy.get("preserve_text_paths", [])):
        return data
    if b"\0" in data:
        return data
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        return data
    for old, new in policy["text_replacements"]:
        text = text.replace(old, new)
    return text.encode("utf-8")


def normalize_tree(source: Path, destination: Path, policy: dict) -> None:
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)
    seen: dict[str, str] = {}
    entries = sorted(source.rglob("*"), key=lambda item: (len(item.relative_to(source).parts), str(item)))
    for entry in entries:
        relative = entry.relative_to(source).as_posix()
        if relative == ".git" or relative.startswith(".git/"):
            continue
        mapped = mapped_path(relative, policy)
        previous = seen.get(mapped)
        if previous is not None and previous != relative:
            raise SyncError(f"branding path collision: {previous!r} and {relative!r} map to {mapped!r}")
        seen[mapped] = relative
        target = destination / mapped
        if entry.is_symlink():
            target.parent.mkdir(parents=True, exist_ok=True)
            link_target = os.readlink(entry)
            for old, new in policy["path_replacements"]:
                link_target = link_target.replace(old, new)
            target.symlink_to(link_target)
        elif entry.is_dir():
            target.mkdir(parents=True, exist_ok=True)
            shutil.copystat(entry, target, follow_symlinks=False)
        elif entry.is_file():
            target.parent.mkdir(parents=True, exist_ok=True)
            data = transform_text(entry.read_bytes(), mapped, policy)
            target.write_bytes(data)
            shutil.copystat(entry, target, follow_symlinks=False)


def remove_tree_contents(path: Path, *, keep_git: bool = False) -> None:
    path.mkdir(parents=True, exist_ok=True)
    for child in path.iterdir():
        if keep_git and child.name == ".git":
            continue
        if child.is_symlink() or child.is_file():
            child.unlink()
        else:
            shutil.rmtree(child)


def copy_tree(source: Path, destination: Path, *, keep_git: bool = False) -> None:
    remove_tree_contents(destination, keep_git=keep_git)
    for child in source.iterdir():
        if child.name == ".git":
            continue
        target = destination / child.name
        if child.is_symlink():
            target.symlink_to(os.readlink(child))
        elif child.is_dir():
            shutil.copytree(child, target, symlinks=True, copy_function=shutil.copy2)
        else:
            shutil.copy2(child, target, follow_symlinks=False)


def export_tracked_subtree(repo_root: Path, prefix: str, destination: Path) -> None:
    remove_tree_contents(destination)
    output = run(["git", "ls-files", "-z", "--", prefix], cwd=repo_root).stdout
    for raw_path in output.split(b"\0"):
        if not raw_path:
            continue
        tracked_path = raw_path.decode("utf-8")
        relative = Path(tracked_path).relative_to(prefix)
        source = repo_root / tracked_path
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if source.is_symlink():
            target.symlink_to(os.readlink(source))
        else:
            shutil.copy2(source, target, follow_symlinks=False)


def replace_tracked_subtree(repo_root: Path, prefix: str, candidate: Path) -> None:
    output = run(["git", "ls-files", "-z", "--", prefix], cwd=repo_root).stdout
    tracked_paths = [raw.decode("utf-8") for raw in output.split(b"\0") if raw]
    for tracked_path in tracked_paths:
        target = repo_root / tracked_path
        if target.is_symlink() or target.is_file():
            target.unlink()
    destination = repo_root / prefix
    for child in candidate.iterdir():
        if child.name == ".git":
            continue
        target = destination / child.name
        if target.exists() and target.is_dir() and child.is_dir():
            shutil.copytree(child, target, dirs_exist_ok=True, symlinks=True, copy_function=shutil.copy2)
        elif child.is_symlink():
            if target.exists() or target.is_symlink():
                if target.is_dir() and not target.is_symlink():
                    shutil.rmtree(target)
                else:
                    target.unlink()
            target.symlink_to(os.readlink(child))
        elif child.is_dir():
            if target.exists() and not target.is_dir():
                target.unlink()
            shutil.copytree(child, target, symlinks=True, copy_function=shutil.copy2)
        else:
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(child, target, follow_symlinks=False)


def require_clean_worktree(repo_root: Path) -> None:
    status = run(
        ["git", "status", "--porcelain=v1", "--untracked-files=all"],
        cwd=repo_root,
    ).stdout.decode("utf-8")
    if status:
        raise SyncError("vvterm sync requires a clean worktree")


def fetch_snapshot(repository: str, commit: str, destination: Path) -> str:
    validate_commit(commit)
    destination.mkdir(parents=True)
    run(["git", "init", "--quiet"], cwd=destination)
    run(["git", "remote", "add", "origin", repository], cwd=destination)
    run(
        ["git", "fetch", "--quiet", "--depth=1", "--filter=blob:none", "origin", commit],
        cwd=destination,
    )
    resolved = run(["git", "rev-parse", "FETCH_HEAD^{commit}"], cwd=destination).stdout.decode().strip()
    if resolved != commit:
        raise SyncError(f"fetched commit mismatch: expected {commit}, got {resolved}")
    tree = run(["git", "rev-parse", "FETCH_HEAD^{tree}"], cwd=destination).stdout.decode().strip()
    run(["git", "checkout", "--quiet", "--detach", "FETCH_HEAD"], cwd=destination)
    return tree


def git_commit_all(repo: Path, message: str) -> str:
    run(["git", "add", "-f", "-A"], cwd=repo)
    run(["git", "commit", "--quiet", "--allow-empty", "--no-verify", "-m", message], cwd=repo)
    return run(["git", "rev-parse", "HEAD"], cwd=repo).stdout.decode().strip()


def merge_trees(base: Path, ours: Path, theirs: Path, merge_repo: Path) -> list[str]:
    merge_repo.mkdir(parents=True)
    run(["git", "init", "--quiet"], cwd=merge_repo)
    run(["git", "config", "user.name", "Orlix vvterm sync"], cwd=merge_repo)
    run(["git", "config", "user.email", "vvterm-sync@orlix.invalid"], cwd=merge_repo)
    run(["git", "config", "commit.gpgsign", "false"], cwd=merge_repo)
    copy_tree(base, merge_repo, keep_git=True)
    git_commit_all(merge_repo, "normalized old upstream")
    run(["git", "branch", "-M", "ours"], cwd=merge_repo)
    copy_tree(ours, merge_repo, keep_git=True)
    git_commit_all(merge_repo, "current Orlix source")
    run(["git", "checkout", "--quiet", "-b", "theirs", "HEAD~1"], cwd=merge_repo)
    copy_tree(theirs, merge_repo, keep_git=True)
    git_commit_all(merge_repo, "normalized new upstream")
    run(["git", "checkout", "--quiet", "ours"], cwd=merge_repo)
    result = run(
        ["git", "merge", "--no-commit", "--no-ff", "-Xfind-renames=50%", "theirs"],
        cwd=merge_repo,
        check=False,
    )
    if result.returncode not in (0, 1):
        stderr = result.stderr.decode("utf-8", errors="replace")
        raise SyncError(f"temporary three-way merge failed ({result.returncode}): {stderr}")
    conflicts = run(["git", "diff", "--name-only", "--diff-filter=U"], cwd=merge_repo).stdout
    return [path for path in conflicts.decode("utf-8").splitlines() if path]


def policy_path(repo_root: Path, version: int) -> Path:
    return repo_root / "Orlix" / "make" / "vvterm-branding" / f"v{version}.json"


def manifest_path(repo_root: Path) -> Path:
    return repo_root / "docs" / "sources" / "release" / "orlix-app-release-inputs.json"


def state_path(repo_root: Path) -> Path:
    return repo_root / "Orlix" / ".vvterm-sync-state.json"


def conflict_path(repo_root: Path) -> Path:
    return repo_root / "Orlix" / ".vvterm-sync-conflicts.txt"


def source_import(manifest: dict) -> dict:
    value = manifest.get("source_import")
    if not isinstance(value, dict):
        raise SyncError("release manifest is missing source_import")
    return value


def current_policy_version(source: dict) -> int:
    return int(source.get("branding_policy_version", 1))


def perform_sync(repo_root: Path, target_commit: str) -> None:
    validate_commit(target_commit)
    require_clean_worktree(repo_root)
    manifest = load_json(manifest_path(repo_root))
    source = source_import(manifest)
    old_commit = str(source.get("commit", ""))
    validate_commit(old_commit)
    if old_commit == target_commit:
        raise SyncError(f"vvterm is already pinned to {target_commit}")
    old_policy_version = current_policy_version(source)
    new_policy_version = old_policy_version
    old_policy_file = policy_path(repo_root, old_policy_version)
    new_policy_file = policy_path(repo_root, new_policy_version)
    old_policy = load_json(old_policy_file)
    new_policy = load_json(new_policy_file)
    prefix = str(source.get("path", "Orlix"))

    with tempfile.TemporaryDirectory(prefix="orlix-vvterm-sync.") as temporary:
        work = Path(temporary)
        old_checkout = work / "old-upstream"
        new_checkout = work / "new-upstream"
        old_tree = fetch_snapshot(UPSTREAM_REPOSITORY, old_commit, old_checkout)
        expected_old_tree = source.get("tree")
        if expected_old_tree and old_tree != expected_old_tree:
            raise SyncError(f"old upstream tree mismatch: expected {expected_old_tree}, got {old_tree}")
        new_tree = fetch_snapshot(UPSTREAM_REPOSITORY, target_commit, new_checkout)

        old_normalized = work / "old-normalized"
        new_normalized = work / "new-normalized"
        ours = work / "ours"
        normalize_tree(old_checkout, old_normalized, old_policy)
        normalize_tree(new_checkout, new_normalized, new_policy)
        export_tracked_subtree(repo_root, prefix, ours)

        merge_repo = work / "merge"
        conflicts = merge_trees(old_normalized, ours, new_normalized, merge_repo)
        replace_tracked_subtree(repo_root, prefix, merge_repo)

        state = {
            "repository": UPSTREAM_REPOSITORY,
            "old_commit": old_commit,
            "old_tree": old_tree,
            "new_commit": target_commit,
            "new_tree": new_tree,
            "path": prefix,
            "old_branding_policy_version": old_policy_version,
            "new_branding_policy_version": new_policy_version,
            "new_branding_policy_sha256": file_sha256(new_policy_file),
            "conflicts": conflicts,
        }
        write_json(state_path(repo_root), state)
        if conflicts:
            conflict_path(repo_root).write_text("\n".join(conflicts) + "\n", encoding="utf-8")
            raise SyncError(
                f"vvterm sync produced {len(conflicts)} conflict(s); resolve paths listed in {conflict_path(repo_root)}"
            )
        conflict_path(repo_root).unlink(missing_ok=True)
        print(f"vvterm candidate ready: {old_commit} -> {target_commit}")
        print("run make vvterm-sync-complete VVTERM_COMMIT=" + target_commit)


def text_files(root: Path) -> Iterable[tuple[Path, bytes]]:
    for path in root.rglob("*"):
        if any(part in IGNORED_SCAN_PARTS for part in path.relative_to(root).parts):
            continue
        if not path.is_file() or path.is_symlink():
            continue
        data = path.read_bytes()
        if b"\0" in data:
            continue
        try:
            data.decode("utf-8")
        except UnicodeDecodeError:
            continue
        yield path, data


def check_conflict_markers(root: Path) -> list[str]:
    failures: list[str] = []
    for path, data in text_files(root):
        if any(line.startswith(CONFLICT_MARKERS) for line in data.splitlines()):
            failures.append(path.relative_to(root).as_posix())
    return failures


def check_branding(source_root: Path, policy: dict) -> list[str]:
    failures: list[str] = []
    allowed = policy.get("allowed_text_paths", [])
    allowed_paths = policy.get("allowed_path_patterns", [])
    for path in source_root.rglob("*"):
        relative = path.relative_to(source_root).as_posix()
        if any(part in IGNORED_SCAN_PARTS for part in path.relative_to(source_root).parts):
            continue
        if BRAND_TOKEN.search(relative) and not path_matches(relative, allowed_paths):
            failures.append(f"unbranded path: {relative}")
    for path, data in text_files(source_root):
        relative = path.relative_to(source_root).as_posix()
        if path_matches(relative, allowed):
            continue
        text = data.decode("utf-8")
        if BRAND_TOKEN.search(text):
            failures.append(f"unbranded text: {relative}")
    return failures


def validate_source(repo_root: Path, *, allow_pending_state: bool = False) -> list[str]:
    failures: list[str] = []
    manifest = load_json(manifest_path(repo_root))
    source = source_import(manifest)
    commit = str(source.get("commit", ""))
    if not FULL_SHA.fullmatch(commit):
        failures.append("source_import.commit is not a full lowercase SHA")
    tree = source.get("tree")
    if manifest.get("schema_version") == 2 and not (isinstance(tree, str) and FULL_SHA.fullmatch(tree)):
        failures.append("source_import.tree is not a full lowercase SHA")
    version = current_policy_version(source)
    selected_policy_path = policy_path(repo_root, version)
    if not selected_policy_path.is_file():
        failures.append(f"missing branding policy: {selected_policy_path}")
        return failures
    policy = load_json(selected_policy_path)
    recorded_hash = source.get("branding_policy_sha256")
    if manifest.get("schema_version") == 2 and recorded_hash != file_sha256(selected_policy_path):
        failures.append("source_import.branding_policy_sha256 does not match the selected policy")
    source_root = repo_root / str(source.get("path", "Orlix"))
    failures.extend(check_branding(source_root, policy))
    failures.extend(f"conflict marker: {path}" for path in check_conflict_markers(source_root))
    if not allow_pending_state and state_path(repo_root).exists():
        failures.append("unfinished vvterm sync state exists")
    project_spec = (repo_root / "project.yml").read_text(encoding="utf-8")
    if "Orlix/Orlix" not in project_spec:
        failures.append("project.yml does not reference tracked Orlix application source")
    return failures


def update_provenance(repo_root: Path, state: dict) -> None:
    path = repo_root / "docs" / "sources" / "release" / "app-source-provenance.md"
    text = path.read_text(encoding="utf-8")
    replacements = {
        r"^- Pinned upstream commit: `[^`]+`$": f"- Pinned upstream commit: `{state['new_commit']}`",
        r"^- Pinned upstream tree: `[^`]+`$": f"- Pinned upstream tree: `{state['new_tree']}`",
        r"^- Import method: .+$": "- Import method: three-way vendored source snapshot",
    }
    for pattern, replacement in replacements.items():
        text, count = re.subn(pattern, replacement, text, flags=re.MULTILINE)
        if count != 1:
            raise SyncError(f"provenance field did not match exactly once: {pattern}")
    path.write_text(text, encoding="utf-8")


def complete_sync(repo_root: Path, target_commit: str) -> None:
    validate_commit(target_commit)
    pending_path = state_path(repo_root)
    if not pending_path.is_file():
        raise SyncError("no pending vvterm sync state exists")
    state = load_json(pending_path)
    if state.get("new_commit") != target_commit:
        raise SyncError(f"pending sync targets {state.get('new_commit')}, not {target_commit}")
    marker_failures = check_conflict_markers(repo_root / str(state["path"]))
    if marker_failures:
        raise SyncError("unresolved conflict markers: " + ", ".join(marker_failures))

    manifest_file = manifest_path(repo_root)
    manifest = load_json(manifest_file)
    source = source_import(manifest)
    source.clear()
    source.update(
        {
            "repository": state["repository"],
            "commit": state["new_commit"],
            "tree": state["new_tree"],
            "path": state["path"],
            "method": "three_way_vendored_snapshot",
            "branding_policy_version": state["new_branding_policy_version"],
            "branding_policy_sha256": state["new_branding_policy_sha256"],
            "provenance": "docs/sources/release/app-source-provenance.md",
        }
    )
    manifest["schema_version"] = 2
    write_json(manifest_file, manifest)
    update_provenance(repo_root, state)
    conflict_path(repo_root).unlink(missing_ok=True)
    pending_path.unlink()
    failures = validate_source(repo_root)
    if failures:
        raise SyncError("source validation failed after completion:\n" + "\n".join(failures))
    print(f"vvterm source pin advanced to {target_commit}")


def check_source(repo_root: Path) -> None:
    failures = validate_source(repo_root)
    if failures:
        raise SyncError("vvterm source check failed:\n" + "\n".join(failures))
    source = source_import(load_json(manifest_path(repo_root)))
    tree = source.get("tree", "not-recorded")
    print(f"vvterm source check passed: {source['commit']} tree {tree}")


def upstream_tests(repo_root: Path, destination: str) -> None:
    source = source_import(load_json(manifest_path(repo_root)))
    commit = str(source.get("commit", ""))
    validate_commit(commit)
    with tempfile.TemporaryDirectory(prefix="orlix-vvterm-upstream-tests.") as temporary:
        checkout = Path(temporary) / "upstream"
        tree = fetch_snapshot(UPSTREAM_REPOSITORY, commit, checkout)
        expected_tree = source.get("tree")
        if expected_tree and tree != expected_tree:
            raise SyncError(f"upstream test tree mismatch: expected {expected_tree}, got {tree}")
        run(
            [
                "xcodebuild",
                "-project",
                "VVTerm.xcodeproj",
                "-scheme",
                "VVTerm",
                "-destination",
                destination,
                "test",
            ],
            cwd=checkout,
            capture=False,
        )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    sync_parser = subparsers.add_parser("sync")
    sync_parser.add_argument("--commit", required=True)
    complete_parser = subparsers.add_parser("complete")
    complete_parser.add_argument("--commit", required=True)
    subparsers.add_parser("check")
    tests_parser = subparsers.add_parser("upstream-tests")
    tests_parser.add_argument("--destination", required=True)
    return parser.parse_args()


def main() -> int:
    arguments = parse_args()
    root = repository_root()
    try:
        if arguments.command == "sync":
            perform_sync(root, arguments.commit)
        elif arguments.command == "complete":
            complete_sync(root, arguments.commit)
        elif arguments.command == "check":
            check_source(root)
        elif arguments.command == "upstream-tests":
            upstream_tests(root, arguments.destination)
        else:
            raise SyncError(f"unknown command: {arguments.command}")
    except SyncError as error:
        print(f"vvterm sync error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
