from __future__ import annotations

import hashlib
import json
import tempfile
import unittest
from pathlib import Path

import substitute
from content_digest import artifact_manifest_v2
from locked_buildset import KERNEL_COMPONENTS, buildset_digest_v2

KERNEL_PRODUCT = {
    "OrlixKernel.a": b"kernel\n",
    "arch/orlix/boot/dts/development.dtb": b"development\n",
    "arch/orlix/boot/dts/release.dtb": b"release\n",
}

PRODUCTS = {
    "uapi": {
        "include/linux/unistd.h": b"uapi\n",
        "kbuild-archive.tar": b"archive\n",
        "manifest.json": b"{}\n",
        "uapi.sha256": b"1" * 64 + b"\n",
    },
    "mlibc": {
        "usr/include/stdio.h": b"stdio\n",
        "usr/lib/libc.a": b"libc\n",
        "libcompiler_rt.a": b"runtime\n",
        "abi.txt": b"_start\n",
        "manifest.json": b"{}\n",
        "sysroot.sha256": b"2" * 64 + b"\n",
    },
    "rootfs": {
        "initramfs.cpio.gz": b"initramfs\n",
        "base.ext4": b"base\n",
        "state.ext4": b"state\n",
        "file-manifest.txt": b"./bin\n",
        "payload-metadata.txt": b"init=/init\n",
        "source-input.sha256": b"3" * 64 + b"\n",
    },
}
for _kernel in KERNEL_COMPONENTS:
    PRODUCTS[_kernel] = dict(KERNEL_PRODUCT)


def _component(root: Path, name: str, files: dict[str, bytes]) -> dict:
    component = root / name
    product = component / "product"
    for relative, content in files.items():
        target = product / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(content)
        target.chmod(0o644)
    manifest = artifact_manifest_v2(product, files_only=True)
    digest = hashlib.sha256(manifest).hexdigest()
    (component / "artifact-identity-v2.json").write_bytes(manifest)
    (component / "artifact-identity-v2.sha256").write_text(digest + "\n", encoding="ascii")
    return {
        "unsigned_digest": digest,
        "artifact_identity": {
            "format": "artifact-identity-v2",
            "version": 2,
            "digest": digest,
        },
        "oci_digest": "sha256:" + "a" * 64,
        "oci_reference": "ghcr.io/rudironsoni/orlix/%s@sha256:%s" % (name, "a" * 64),
    }


def _lock(reconstruct: Path, products: dict[str, dict[str, bytes]] = PRODUCTS) -> dict:
    staging = reconstruct / "pre"
    components = {
        name: _component(staging, name, files) for name, files in products.items()
    }
    buildset = buildset_digest_v2(components)
    root = reconstruct / buildset
    root.mkdir(parents=True)
    for name in components:
        (staging / name).rename(root / name)
    return {"schema": 2, "buildset": buildset, "components": components}


class SubstituteTests(unittest.TestCase):
    def test_projects_real_directories_and_preserves_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            reconstruct = base / "reconstruct"
            lock_payload = _lock(reconstruct)
            lock_path = base / "artifacts.lock.json"
            lock_path.write_text(json.dumps(lock_payload) + "\n", encoding="utf-8")
            before = lock_path.read_bytes()
            payload = substitute.substitute(
                str(lock_path), str(reconstruct), str(base / "out.json")
            )
            self.assertEqual(payload["kind"], "promoted-components")
            self.assertEqual(set(payload["components"]), set(PRODUCTS))
            self.assertEqual(lock_path.read_bytes(), before)

            stage = base / "imported"
            substitute.stage_imported(payload, str(stage))
            self.assertFalse((stage / "uapi").is_symlink())
            self.assertTrue((stage / "uapi").is_dir())
            self.assertEqual(
                (stage / "uapi" / "product" / "kbuild-archive.tar").read_bytes(),
                b"archive\n",
            )
            self.assertEqual(
                (stage / "rootfs" / "product" / "base.ext4").read_bytes(), b"base\n"
            )
            self.assertTrue((stage / "uapi" / "artifact-identity-v2.sha256").is_file())

    def test_stale_staging_entries_do_not_survive(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            reconstruct = base / "reconstruct"
            lock_payload = _lock(reconstruct)
            lock_path = base / "artifacts.lock.json"
            lock_path.write_text(json.dumps(lock_payload) + "\n", encoding="utf-8")
            payload = substitute.substitute(
                str(lock_path), str(reconstruct), str(base / "out.json")
            )
            stage = base / "imported"
            substitute.stage_imported(payload, str(stage))
            stale = stage / "uapi" / "product" / "stale.bin"
            stale.write_bytes(b"stale\n")
            substitute.stage_imported(payload, str(stage))
            self.assertFalse(stale.exists())

    def test_staging_does_not_mutate_reconstructed_content(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            reconstruct = base / "reconstruct"
            lock_payload = _lock(reconstruct)
            lock_path = base / "artifacts.lock.json"
            lock_path.write_text(json.dumps(lock_payload) + "\n", encoding="utf-8")
            payload = substitute.substitute(
                str(lock_path), str(reconstruct), str(base / "out.json")
            )
            tree = Path(payload["components"]["uapi"]["tree"]) / "product"

            def digests() -> dict:
                return {
                    path.relative_to(tree).as_posix(): hashlib.sha256(
                        path.read_bytes()
                    ).hexdigest()
                    for path in tree.rglob("*")
                    if path.is_file()
                }

            before_digests = digests()
            substitute.stage_imported(payload, str(base / "imported"))
            self.assertEqual(before_digests, digests())

    def test_v2_product_tampering_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            reconstruct = base / "reconstruct"
            lock_payload = _lock(reconstruct)
            lock_path = base / "artifacts.lock.json"
            lock_path.write_text(json.dumps(lock_payload) + "\n", encoding="utf-8")
            component = (
                reconstruct
                / lock_payload["buildset"]
                / "uapi"
                / "product"
                / "kbuild-archive.tar"
            )
            component.write_bytes(b"tampered\n")
            with self.assertRaises(substitute.SubstituteError):
                substitute.substitute(
                    str(lock_path), str(reconstruct), str(base / "out.json")
                )


if __name__ == "__main__":
    unittest.main()
