#!/usr/bin/env python3
"""Assemble an XCTRunner-based xctestrun for xcodebuild test-without-building.

UI tests are XCTRunner-hosted: the test host is a <Module>-Runner.app copied
from the selected Xcode's XCTRunner.app (never the app under test), the test
bundle lives in the runner's PlugIns, and the app under test is referenced as
UITargetAppPath. Test selection travels inside the xctestrun as
OnlyTestIdentifiers/OnlyTestingIdentifiers (Class[/method]); no
-only-testing flag is used. Paths derived from the verified developer
directory only; nothing constructs an /Applications path. Mirrors the
structure rules_apple's ios_xctestrun_runner emits for XCUITEST.
"""

from __future__ import annotations

import argparse
import os
import plistlib
import shutil
import stat
import sys
import zipfile
from pathlib import Path


ARCH = "arm64"


def _make_writable(tree: Path) -> None:
    for root, dirs, files in os.walk(tree, followlinks=False):
        for name in dirs + files:
            path = Path(root) / name
            if path.is_symlink():
                continue
            mode = path.stat().st_mode
            os.chmod(path, mode | stat.S_IWUSR | stat.S_IXUSR if path.is_dir() else mode | stat.S_IWUSR)


def _require_dir(path: Path, label: str) -> Path:
    if not path.is_dir():
        raise ValueError(f"xctestrun {label} is not a directory: {path}")
    return path


def _copy_frameworks(developer_dir: Path, runner_app: Path) -> bool:
    """Copy the UI-testing frameworks into the runner. Returns Testing.framework presence."""
    libs = developer_dir / "Platforms/iPhoneSimulator.platform/Developer/Library"
    frameworks = runner_app / "Frameworks"
    frameworks.mkdir(parents=True, exist_ok=True)
    required = [
        libs / "Frameworks/XCTest.framework",
        libs / "PrivateFrameworks/XCTestCore.framework",
        libs / "PrivateFrameworks/XCTAutomationSupport.framework",
        libs / "PrivateFrameworks/XCUnit.framework",
    ]
    optional = [libs / "PrivateFrameworks/XCTestSupport.framework"]
    for source in required + optional:
        if not source.exists():
            if source in required:
                raise ValueError(f"xctestrun required framework is missing: {source}")
            continue
        target = frameworks / source.name
        if target.exists():
            shutil.rmtree(target, ignore_errors=True)
        shutil.copytree(source, target, symlinks=True)
    automation = libs / "Frameworks/XCUIAutomation.framework"
    if not automation.is_dir():
        automation = libs / "PrivateFrameworks/XCUIAutomation.framework"
    _require_dir(automation, "XCUIAutomation.framework")
    target = frameworks / "XCUIAutomation.framework"
    if target.exists():
        shutil.rmtree(target, ignore_errors=True)
    shutil.copytree(automation, target, symlinks=True)
    usr_lib = developer_dir / "Platforms/iPhoneSimulator.platform/Developer/usr/lib"
    for dylib in ("libXCTestSwiftSupport.dylib", "libXCTestBundleInject.dylib"):
        source = usr_lib / dylib
        if not source.is_file():
            raise ValueError(f"xctestrun required library is missing: {source}")
        shutil.copy2(source, frameworks / dylib)
    concurrency = (
        developer_dir / "Platforms/iPhoneOS.platform/Library/Developer/CoreSimulator/Profiles"
        / "Runtimes/iOS.simruntime/Contents/Resources/RuntimeRoot/usr/lib/swift/libswift_Concurrency.dylib"
    )
    if concurrency.is_file():
        shutil.copy2(concurrency, frameworks / "libswift_Concurrency.dylib")
    testing = libs / "Frameworks/Testing.framework"
    if testing.is_dir():
        target = frameworks / "Testing.framework"
        if target.exists():
            shutil.rmtree(target, ignore_errors=True)
        shutil.copytree(testing, target, symlinks=True)
        return True
    return False


def _patch_runner_info(runner_app: Path, runner_bundle_id: str) -> None:
    info = runner_app / "Info.plist"
    raw = info.read_bytes()
    raw = raw.replace(b"$(WRAPPEDPRODUCTNAME)", b"XCTRunner").replace(b"WRAPPEDPRODUCTNAME", b"XCTRunner")
    raw = raw.replace(b"$(WRAPPEDPRODUCTBUNDLEIDENTIFIER)", runner_bundle_id.encode())
    raw = raw.replace(b"WRAPPEDPRODUCTBUNDLEIDENTIFIER", runner_bundle_id.encode())
    info.write_bytes(raw)


def _resolve_bundle_source(test_bundle_src: Path, work_dir: Path) -> Path:
    if test_bundle_src.is_dir() and test_bundle_src.suffix == ".xctest":
        return test_bundle_src
    if test_bundle_src.is_file() and test_bundle_src.suffix == ".zip":
        staging = work_dir / "_bundle_src"
        if staging.exists():
            shutil.rmtree(staging)
        staging.mkdir(parents=True)
        zipfile.ZipFile(str(test_bundle_src)).extractall(str(staging))
        found = sorted(str(p) for p in staging.rglob("*.xctest") if p.is_dir())
        if len(found) != 1:
            raise ValueError(f"expected one test bundle in {test_bundle_src}, found: {found}")
        return Path(found[0])
    raise ValueError(f"xctestrun test bundle is neither an .xctest dir nor a .zip: {test_bundle_src}")


def assemble_xctrunner(
    developer_dir: Path,
    test_bundle_src: Path,
    product_module: str,
    work_dir: Path,
) -> dict:
    """Assemble <Module>-Runner.app in work_dir. Returns runner/bundle paths and ids."""
    if not product_module or not product_module[0].isalpha():
        raise ValueError(f"xctestrun product module is invalid: {product_module!r}")
    work_dir.mkdir(parents=True, exist_ok=True)
    bundle_src = _resolve_bundle_source(test_bundle_src, work_dir)
    runner_name = f"{product_module}-Runner"
    runner_app = work_dir / f"{runner_name}.app"
    if runner_app.exists():
        shutil.rmtree(runner_app)
    agents = developer_dir / "Platforms/iPhoneSimulator.platform/Developer/Library/Xcode/Agents"
    _require_dir(agents / "XCTRunner.app", "XCTRunner.app")
    shutil.copytree(agents / "XCTRunner.app", runner_app, symlinks=True)
    runner_bundle_id = f"com.apple.test.{runner_name}"
    _patch_runner_info(runner_app, runner_bundle_id)
    has_testing_framework = _copy_frameworks(developer_dir, runner_app)
    plugins = runner_app / "PlugIns"
    plugins.mkdir(parents=True, exist_ok=True)
    staged_bundle = plugins / bundle_src.name
    if staged_bundle.exists():
        shutil.rmtree(staged_bundle)
    shutil.copytree(bundle_src, staged_bundle, symlinks=True)
    (staged_bundle / "Frameworks").mkdir(parents=True, exist_ok=True)
    _make_writable(runner_app)
    return {
        "runner_app": runner_app,
        "runner_name": runner_name,
        "runner_bundle_id": runner_bundle_id,
        "bundle_name": bundle_src.name,
        "has_testing_framework": has_testing_framework,
    }


def _testing_identifiers(only: list[str]) -> tuple[list[str], dict]:
    legacy = list(only)
    suites: dict[str, list[str]] = {}
    order: list[str] = []
    for identifier in only:
        if "/" in identifier:
            cls, method = identifier.split("/", 1)
        else:
            cls, method = identifier, ""
        if cls not in suites:
            suites[cls] = []
            order.append(cls)
        if method:
            suites[cls].append(method)
    modern = {
        "suites": [
            {"name": cls, **({"testFunctions": suites[cls]} if suites[cls] else {})} for cls in order
        ],
        "xctestClasses": [
            {"name": cls, **({"xctestMethods": suites[cls]} if suites[cls] else {})} for cls in order
        ],
    }
    return legacy, modern


def build_xctestrun(
    test_bundle: Path,
    app: Path,
    app_bundle_id: str,
    product_module: str,
    runner_name: str,
    runner_bundle_id: str,
    developer_dir: Path,
    only: list[str],
) -> dict:
    for label, path in (("test bundle", test_bundle), ("app", app)):
        if not path.exists():
            raise ValueError(f"xctestrun {label} does not exist: {path}")
    if not app_bundle_id or "." not in app_bundle_id:
        raise ValueError(f"xctestrun app bundle id is invalid: {app_bundle_id!r}")
    if not product_module or not product_module[0].isalpha():
        raise ValueError(f"xctestrun product module is invalid: {product_module!r}")
    if not only:
        raise ValueError("xctestrun requires at least one OnlyTestIdentifiers entry")
    bundle_name = test_bundle.name
    if not bundle_name.endswith(".xctest"):
        raise ValueError(f"xctestrun test bundle is not an .xctest: {test_bundle}")
    legacy_only, modern_only = _testing_identifiers(only)
    libs = "__PLATFORMS__/iPhoneSimulator.platform/Developer"
    testing_env: dict[str, str] = {
        "DYLD_INSERT_LIBRARIES": "",
        "DYLD_LIBRARY_PATH": f"{libs}/usr/lib",
        "__XCODE_BUILT_PRODUCTS_DIR_PATHS": "/DUMMY_SRCROOT/",
        "XCInjectBundleInto": f"__TESTHOST__/{runner_name}.app/{runner_name}",
    }
    if (developer_dir / "Platforms/iPhoneSimulator.platform/Developer/Library/Frameworks/Testing.framework").is_dir():
        testing_env["DYLD_FRAMEWORK_PATH"] = f"{libs}/Library/Frameworks"
    entry = {
        "ProductModuleName": product_module,
        "TestBundlePath": f"__TESTHOST__/PlugIns/{bundle_name}",
        "TestExecutionOrdering": "defined",
        "IsAppHostedTestBundle": True,
        "TestHostPath": f"__TESTROOT__/{runner_name}.app",
        "TestHostBundleIdentifier": runner_bundle_id,
        "IsUITestBundle": True,
        "IsXCTRunnerHostedTestBundle": True,
        "UITargetAppPath": str(app.resolve()),
        "UITargetAppBundleIdentifier": app_bundle_id,
        "DependentProductPaths": [
            f"__TESTHOST__/PlugIns/{bundle_name}",
            str(app.resolve()),
            f"__TESTROOT__/{runner_name}.app",
        ],
        "TestingEnvironmentVariables": testing_env,
        "OnlyTestIdentifiers": legacy_only,
        "OnlyTestingIdentifiers": modern_only,
    }
    metadata = {
        "CodeCoverageBuildableInfos": [
            {
                "Architecture": ARCH,
                "BuildableIdentifier": "000000000000000000000000:primary",
                "IncludeInReport": True,
                "IsStatic": False,
                "Name": bundle_name,
                "ProductPath": f"__TESTHOST__/PlugIns/{bundle_name}",
                "Toolchains": ["com.apple.dt.toolchain.XcodeDefault"],
            }
        ],
        "FormatVersion": 1,
    }
    return {product_module: entry, "__xctestrun_metadata__": metadata}


def write_xctestrun(path: Path, content: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as stream:
        plistlib.dump(content, stream, fmt=plistlib.FMT_XML)


def resolve_test_bundle(exec_root: Path, outputs_file: Path, work_dir: Path) -> str:
    """Resolve exactly one .xctest bundle from cquery --output=files lines."""
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
    parser.add_argument("--developer-dir", required=False, default=None)
    parser.add_argument("--only", action="append", default=[])
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
    for required in ("test_bundle", "app", "app_bundle_id", "product_module", "developer_dir", "out"):
        if not getattr(args, required):
            parser.error(f"--{required.replace('_', '-')} is required to write an xctestrun")
    if not args.only:
        parser.error("--only Class[/method] is required at least once")
    only = [item for value in args.only for item in value.split(",") if item]
    out = Path(args.out)
    work_dir = out.parent
    runner = assemble_xctrunner(
        Path(args.developer_dir), Path(args.test_bundle), args.product_module, work_dir
    )
    content = build_xctestrun(
        runner["runner_app"] / "PlugIns" / runner["bundle_name"],
        Path(args.app),
        args.app_bundle_id,
        args.product_module,
        runner["runner_name"],
        runner["runner_bundle_id"],
        Path(args.developer_dir),
        only,
    )
    write_xctestrun(out, content)
    print(str(out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
