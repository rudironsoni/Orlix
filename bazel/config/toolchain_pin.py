"""Product Xcode pin versus allowed local cache identities."""

from __future__ import annotations

import json
import hashlib
import os
import platform
import subprocess
from pathlib import Path

PIN_PATH = Path(__file__).with_name("toolchain-pin.json")


class PinError(RuntimeError):
    pass


def load_pin(path: Path = PIN_PATH) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def product_pin(data: dict | None = None) -> dict:
    pin = (data or load_pin())["product_pin"]
    if pin["xcode_version"] == "27.0":
        raise PinError("Xcode 27.0 is not the product pin")
    return pin


def allowed_identities(data: dict | None = None) -> tuple[tuple[str, str], ...]:
    payload = data or load_pin()
    return tuple(
        (entry["xcode_version"], entry["xcode_build"]) for entry in payload["allowed_local"]
    )


def namespace_for(version: str, build: str, data: dict | None = None) -> str:
    payload = data or load_pin()
    for entry in payload["allowed_local"]:
        if entry["xcode_version"] == version and entry["xcode_build"] == build:
            return entry["disk_cache_namespace"]
    raise PinError(f"unsupported Xcode identity {version} {build}")


def require_identity(version: str, build: str, disk_cache: str, data: dict | None = None) -> None:
    payload = data or load_pin()
    pin = product_pin(payload)
    if version == "27.0" and pin["xcode_version"] != "26.6":
        raise PinError("Xcode 27.0 is not the product pin")
    try:
        expected = namespace_for(version, build, payload)
    except PinError:
        raise PinError(f"unsupported Xcode identity {version} {build}")
    if expected not in disk_cache:
        raise PinError(
            f"disk cache {disk_cache} is not namespaced as {expected}"
        )


def capture_manifest(developer_dir: str, bazel: str, output: str) -> dict:
    developer = str(Path(developer_dir).resolve(strict=True))
    env = {**os.environ, "DEVELOPER_DIR": developer}

    def observe(*command: str) -> str:
        return subprocess.run(
            command, env=env, check=True, capture_output=True, text=True, timeout=30,
        ).stdout.strip()

    xcode = observe("/usr/bin/xcodebuild", "-version").splitlines()
    version = xcode[0].removeprefix("Xcode ")
    build = xcode[1].removeprefix("Build version ")
    namespace_for(version, build)
    tools = {}
    for name in ("clang", "ld", "swift", "metal", "bazel"):
        path = Path(bazel if name == "bazel" else observe("/usr/bin/xcrun", "--find", name)).resolve(strict=True)
        with path.open("rb") as stream:
            digest = hashlib.file_digest(stream, "sha256").hexdigest()
        tools[name] = {"path": str(path), "sha256": digest}
    payload = {
        "schema": 1,
        "kind": "observed-toolchain",
        "developer_dir": developer,
        "xcode_version": version,
        "xcode_build": build,
        "bazel_version": observe(bazel, "--version"),
        "host_macos": platform.mac_ver()[0],
        "host_arch": platform.machine(),
        "tools": tools,
        "sdks": {
            sdk: {
                "version": observe("/usr/bin/xcrun", "--sdk", sdk, "--show-sdk-version"),
                "build": observe("/usr/bin/xcrun", "--sdk", sdk, "--show-sdk-build-version"),
                "path": observe("/usr/bin/xcrun", "--sdk", sdk, "--show-sdk-path"),
            }
            for sdk in ("iphoneos", "iphonesimulator", "macosx")
        },
    }
    if payload["bazel_version"] != f"bazel {product_pin()['bazel']}":
        raise PinError("observed Bazel does not match the product pin")
    destination = Path(output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    return payload
