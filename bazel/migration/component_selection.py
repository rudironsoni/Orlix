#!/usr/bin/env python3
"""Select the Bazel component mode from changed paths and the committed lock.

Promoted mode embeds the activated, signed buildset from artifacts.lock.json.
A change is eligible for promoted mode only when every changed path is proven
to be outside the promoted foundations: the component sources, the packaging
and promotion machinery, and the build toolchain. Anything that cannot be
proven app-only selects source mode.

Promoted foundations:
- component sources: OrlixKernel/, OrlixMLibc/, OrlixCoreUtils/; OrlixOS/Sources/make
  feeds a promoted rootfs package;
- packaging and promotion: bazel/feasibility/, bazel/promotion/, bazel/config/,
  bazel/extensions/, bazel/migration/, artifacts.lock.json, upstreams.lock.json;
- build and CI machinery: make/, Makefile, .bazelrc*, MODULE.bazel*,
  .github/workflows/, project.yml, xcode/, Gemfile*, Brewfile, Tools/.

Everything else is treated as app-only. Promotion eligibility never overrides
a source requirement, and an empty or unknown path set always selects source.
"""

from __future__ import annotations

PROMOTED_FOUNDATION_PREFIXES = (
    "OrlixKernel/",
    "OrlixMLibc/",
    "OrlixCoreUtils/",
    "OrlixOS/Sources/make",
    "OrlixOS/Sources/init",
    "OrlixOS/Sources/patches",
    "OrlixOS/Sources/distribution",
    "OrlixOS/Makefile",
    "OrlixOS/Tests",
    "bazel/feasibility/",
    "bazel/promotion/",
    "bazel/config/",
    "bazel/extensions/",
    "bazel/migration/",
    "make/",
    ".github/workflows/",
    "third_party/patches/",
    "xcode/",
)

PROMOTED_FOUNDATION_FILES = (
    "artifacts.lock.json",
    "upstreams.lock.json",
    "MODULE.bazel",
    "MODULE.bazel.lock",
    ".bazelrc",
    ".bazelrc.local.example",
    "Makefile",
    "Brewfile",
    "Gemfile",
    "Gemfile.lock",
    "project.yml",
    "project.local.yml",
)

SOURCE_MODE = "source"
PROMOTED_MODE = "promoted"


def select_component_mode(
    changed_paths,
    lock_schema,
    event_name="pull_request",
):
    if event_name != "pull_request":
        return PROMOTED_MODE if lock_schema == 2 else SOURCE_MODE
    if lock_schema != 2:
        return SOURCE_MODE
    paths = [str(path).strip() for path in changed_paths if str(path).strip()]
    if not paths:
        return SOURCE_MODE
    for path in paths:
        if _affects_promoted_foundations(path):
            return SOURCE_MODE
    return PROMOTED_MODE


def _affects_promoted_foundations(path: str) -> bool:
    normalized = path.lstrip("./")
    if normalized in PROMOTED_FOUNDATION_FILES:
        return True
    for prefix in PROMOTED_FOUNDATION_PREFIXES:
        if normalized == prefix.rstrip("/") or normalized.startswith(prefix):
            return True
    return not _is_proven_app_only(normalized)


def _is_proven_app_only(path: str) -> bool:
    app_only_prefixes = (
        "Orlix/",
        "OrlixHostAdapter/",
        "OrlixOSTestApp/",
        "OrlixTestApp/",
        "OrlixOS/Sources/include",
        "OrlixOS/Sources/Session",
        "third_party/swift/",
        "docs/",
        "Resources/",
        "fastlane/",
    )
    for prefix in app_only_prefixes:
        if path.startswith(prefix):
            return True
    app_only_files = (
        "AGENTS.md",
        "README.md",
        "IMPLEMENT.md",
        "CLAUDE.md",
        "rulesync.jsonc",
        "skills-lock.json",
        "opencode.jsonc",
        "buildServer.json",
    )
    return path in app_only_files
