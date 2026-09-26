"""Stage the rootfs cache product.

The product is initramfs.cpio.gz, base.ext4, and state.ext4. The semantic
digest stays on OrlixRootfsInfo and is not written here. Provenance files are
not payload members.
"""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

PRODUCT_IMAGES = ("base.ext4", "initramfs.cpio.gz", "state.ext4")
EXCLUDED_NAMES = frozenset(
    {
        "artifacts.lock.json",
        "file-manifest.txt",
        "payload-metadata.txt",
        "rootfs.artifact-identity-v2.json",
        "rootfs.artifact-identity-v2.sha256",
        "semantic.sha256",
        "source-input.sha256",
    }
)


def stage_payload(destination: Path, images: dict[str, Path]) -> None:
    """Copy the three rootfs images into ``destination``."""
    if set(PRODUCT_IMAGES) & EXCLUDED_NAMES:
        raise ValueError("rootfs payload product overlaps provenance")
    if set(images) != set(PRODUCT_IMAGES):
        raise ValueError("rootfs payload product is the three images")
    for name in PRODUCT_IMAGES:
        if not images[name].is_file():
            raise ValueError(f"missing rootfs image {name}")
    destination.mkdir(parents=True, exist_ok=True)
    for name in PRODUCT_IMAGES:
        shutil.copyfile(images[name], destination / name)
    present = {path.name for path in destination.iterdir()}
    if present != set(PRODUCT_IMAGES):
        raise ValueError("rootfs payload directory is not the three images")


def _parse_images(values: list[str]) -> dict[str, Path]:
    images: dict[str, Path] = {}
    for value in values:
        name, separator, raw_path = value.partition("=")
        if not separator or not name or not raw_path or name in images:
            raise ValueError(f"invalid rootfs image mapping {value}")
        images[name] = Path(raw_path)
    return images


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dest", required=True, type=Path)
    parser.add_argument("--image", action="append", required=True)
    args = parser.parse_args(argv)
    try:
        stage_payload(args.dest, _parse_images(args.image))
    except ValueError as error:
        print(f"rootfs payload: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
