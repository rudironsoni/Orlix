"""Product Xcode pin versus allowed local cache identities."""

from __future__ import annotations

import json
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
