#!/usr/bin/env python3
"""Generate a minimal xctestrun plist for xcodebuild test-without-building.

The test bundle and test host app are Bazel-built products passed in by path;
nothing here builds anything. Paths are recorded verbatim so the caller can
assert the test host is the canonical application under test.
"""

from __future__ import annotations

import argparse
import plistlib
import sys
from pathlib import Path


def build_xctestrun(
    test_bundle: Path,
    app: Path,
    app_bundle_id: str,
    product_module: str,
) -> dict:
    for label, path in (("test bundle", test_bundle), ("app", app)):
        if not path.exists():
            raise ValueError(f"xctestrun {label} does not exist: {path}")
    if not app_bundle_id or "." not in app_bundle_id:
        raise ValueError(f"xctestrun app bundle id is invalid: {app_bundle_id!r}")
    if not product_module or not product_module[0].isalpha():
        raise ValueError(f"xctestrun product module is invalid: {product_module!r}")
    test_bundle = test_bundle.resolve()
    app = app.resolve()
    return {
        product_module: {
            "ProductModuleName": product_module,
            "TestBundlePath": str(test_bundle),
            "TestExecutionOrdering": "defined",
            "IsAppHostedTestBundle": True,
            "TestHostPath": str(app),
            "TestHostBundleIdentifier": app_bundle_id,
            "IsUITestBundle": True,
            "IsXCTRunnerHostedTestBundle": False,
            "UITargetAppPath": str(app),
            "DependentProductPaths": [str(test_bundle), str(app)],
            "TestingEnvironmentVariables": {},
        }
    }


def write_xctestrun(path: Path, content: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as stream:
        plistlib.dump(content, stream, fmt=plistlib.FMT_XML)


def resolve_test_bundle(exec_root: Path, outputs_file: Path, work_dir: Path) -> str:
    """Resolve exactly one .xctest bundle from cquery --output=files lines."""
    import shutil
    import zipfile

    lines = [line.strip() for line in outputs_file.read_text(encoding="utf-8").splitlines() if line.strip()]
    dirs = [p for p in lines if p.endswith(".xctest") and (exec_root / p).is_dir()]
    zips = [p for p in lines if p.endswith(".zip") and (exec_root / p).is_file()]
    if len(dirs) == 1 and not zips:
        return str(exec_root / dirs[0])
    if len(zips) == 1 and not dirs:
        work_dir.mkdir(parents=True, exist_ok=True)
        for existing in list(work_dir.iterdir()):
            if existing.is_dir() and not existing.is_symlink():
                shutil.rmtree(existing)
            else:
                existing.unlink(missing_ok=True)
        zipfile.ZipFile(str(exec_root / zips[0])).extractall(str(work_dir))
        found = sorted(str(p) for p in work_dir.rglob("*.xctest") if p.is_dir())
        if len(found) != 1:
            raise ValueError(f"expected one test bundle in {zips[0]}, found: {found}")
        return found[0]
    raise ValueError(f"cannot resolve exactly one test bundle: dirs={dirs} zips={zips}")


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--test-bundle", required=False, default=None)
    parser.add_argument("--app", required=False, default=None)
    parser.add_argument("--app-bundle-id", required=False, default=None)
    parser.add_argument("--product-module", required=False, default=None)
    parser.add_argument("--out", required=False, default=None)
    parser.add_argument("--resolve-test-bundle", action="store_true")
    parser.add_argument("--exec-root", default=None)
    parser.add_argument("--outputs-file", default=None)
    parser.add_argument("--work-dir", default=None)
    args = parser.parse_args(argv)
    if args.resolve_test_bundle:
        if not args.exec_root or not args.outputs_file or not args.work_dir:
            parser.error("--resolve-test-bundle requires --exec-root, --outputs-file, and --work-dir")
        print(resolve_test_bundle(Path(args.exec_root), Path(args.outputs_file), Path(args.work_dir)))
        return 0
    for required in ("test_bundle", "app", "app_bundle_id", "product_module", "out"):
        if not getattr(args, required):
            parser.error(f"--{required.replace('_', '-')} is required to write an xctestrun")
    content = build_xctestrun(
        Path(args.test_bundle),
        Path(args.app),
        args.app_bundle_id,
        args.product_module,
    )
    write_xctestrun(Path(args.out), content)
    print(str(Path(args.out)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
