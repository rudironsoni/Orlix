from __future__ import annotations

import hashlib
import json
import os
import tempfile
import unittest
from pathlib import Path

import compare
from bazel.content_digest import artifact_identity_v2, artifact_manifest_v2
from locked_buildset import (
    ARTIFACT_IDENTITY_V2_FORMAT,
    ARTIFACT_IDENTITY_V2_VERSION,
    V2_DIGEST_FILENAME,
    V2_MANIFEST_FILENAME,
)


class PromotionCompareTests(unittest.TestCase):
    def test_artifact_identity_v2_is_canonical_and_tracks_product_mutations(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            vector_payload = base / "vector"
            vector_payload.mkdir()
            vector_file = vector_payload / "payload"
            vector_file.write_bytes(b"artifact-vector\n")
            vector_file.chmod(0o755)
            vector_artifacts = {"payload": vector_file}
            self.assertEqual(
                artifact_manifest_v2(artifacts=vector_artifacts),
                b'{"domain":"orlix.artifact.identity","entries":[{"content_sha256":"34c691454bd53ef9ded2c6559e92ea259c2234fcb1849da0e5f40dfdbc21544e","mode":493,"path":"payload","type":"file"}],"format":"artifact-identity-v2","version":2}\n',
            )
            self.assertEqual(
                artifact_identity_v2(artifacts=vector_artifacts),
                "c37f1577d77a12919a6814cbda1ff49170729044b5acf786cb273f91346085b3",
            )
            outside = base / "provenance.json"
            outside.write_text("first", encoding="utf-8")

            def make_tree(name: str, reverse: bool = False) -> Path:
                root = base / name
                if reverse:
                    (root / "bin").mkdir(parents=True)
                    (root / "bin" / "tool").write_bytes(b"same")
                    (root / "bin" / "tool").chmod(0o755)
                    (root / "empty").mkdir()
                    (root / "empty").chmod(0o755)
                else:
                    (root / "empty").mkdir(parents=True)
                    (root / "empty").chmod(0o755)
                    (root / "bin").mkdir()
                    (root / "bin" / "tool").write_bytes(b"same")
                    (root / "bin" / "tool").chmod(0o755)
                (root / "alias").symlink_to("bin/tool")
                (root / "escape").symlink_to(outside)
                return root

            first = make_tree("first")
            second = make_tree("second", reverse=True)
            first_digest = artifact_identity_v2(first)
            second_digest = artifact_identity_v2(second)
            self.assertEqual(first_digest, second_digest)
            manifest = json.loads(artifact_manifest_v2(first))
            self.assertEqual(manifest["domain"], "orlix.artifact.identity")
            self.assertEqual(manifest["version"], 2)
            self.assertEqual(manifest["format"], "artifact-identity-v2")
            self.assertEqual(
                [entry["path"] for entry in manifest["entries"]],
                sorted(entry["path"] for entry in manifest["entries"]),
            )
            self.assertIn(
                {"mode": 0o755, "path": "bin/tool", "type": "file"},
                [
                    {key: entry[key] for key in ("mode", "path", "type")}
                    for entry in manifest["entries"]
                ],
            )
            self.assertIn(
                {"mode": 0o755, "path": "empty", "type": "directory"},
                [
                    {key: entry[key] for key in ("mode", "path", "type")}
                    for entry in manifest["entries"]
                ],
            )
            outside.write_text("updated", encoding="utf-8")
            self.assertEqual(first_digest, artifact_identity_v2(first))
            os.utime(first / "bin" / "tool", ns=(1, 1))
            self.assertEqual(first_digest, artifact_identity_v2(first))

            mutations = (
                ("content", lambda root: (root / "bin" / "tool").write_bytes(b"changed")),
                ("rename", lambda root: (root / "bin" / "tool").rename(root / "renamed")),
                ("mode", lambda root: (root / "bin" / "tool").chmod(0o1755)),
                ("file-to-symlink", lambda root: ((root / "bin" / "tool").unlink(), (root / "bin" / "tool").symlink_to("other"))),
                ("symlink-target", lambda root: ((root / "alias").unlink(), (root / "alias").symlink_to("missing"))),
            )
            for name, mutate in mutations:
                with self.subTest(name=name):
                    root = make_tree(name)
                    before = artifact_identity_v2(root)
                    mutate(root)
                    self.assertNotEqual(before, artifact_identity_v2(root))

            selected = {
                "images/base.ext4": first / "bin" / "tool",
                "images/initramfs.cpio": first / "alias",
            }
            selected_digest = artifact_identity_v2(artifacts=selected)
            selected_manifest = json.loads(
                artifact_manifest_v2(artifacts=selected)
            )
            self.assertEqual(
                [entry["path"] for entry in selected_manifest["entries"]],
                ["images/base.ext4", "images/initramfs.cpio"],
            )
            single = base / "single"
            single.mkdir()
            single_payload = single / "payload"
            single_payload.write_bytes(b"payload")
            single_payload.chmod(0o755)
            self.assertEqual(
                artifact_identity_v2(single),
                artifact_identity_v2(artifacts={"payload": single_payload}),
            )
            (first / "bin" / "tool").write_bytes(b"large" * (1024 * 1024))
            self.assertNotEqual(selected_digest, artifact_identity_v2(artifacts=selected))

    def test_artifact_identity_v2_rejects_unknown_formats_invalid_paths_and_types(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            root = base / "tree"
            root.mkdir()
            source = root / "file"
            source.write_bytes(b"data")
            with self.assertRaisesRegex(ValueError, "unknown artifact identity format"):
                artifact_manifest_v2(root, format="legacy")
            with self.assertRaisesRegex(ValueError, "invalid artifact path"):
                artifact_identity_v2(artifacts={"../escape": source})
            fifo = root / "fifo"
            os.mkfifo(fifo)
            with self.assertRaisesRegex(ValueError, "unsupported artifact entry"):
                artifact_identity_v2(artifacts={"fifo": fifo})
            root_link = base / "root-link"
            root_link.symlink_to(root, target_is_directory=True)
            with self.assertRaisesRegex(ValueError, "not a real directory"):
                artifact_identity_v2(root_link)

    def test_component_bytes_and_symlinks_must_match(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            first, second = Path(tmp) / "a", Path(tmp) / "b"
            for tree in (first, second):
                tree.mkdir()
                (tree / "digest.sha256").write_text("a" * 64)
                (tree / "binary").write_bytes(b"same")
                (tree / "link").symlink_to("binary")
            self.assertEqual(compare.compare_trees(str(first), str(second)), compare.tree_digest(first))
            (second / "binary").write_bytes(b"different")
            with self.assertRaisesRegex(ValueError, "contents differ"):
                compare.compare_trees(str(first), str(second))

    def test_v2_component_contract_recomputes_product_identity(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            roots = [Path(tmp) / name for name in ("a", "b")]
            for root in roots:
                product = root / "product"
                product.mkdir(parents=True)
                archive = product / "OrlixKernel.a"
                archive.write_bytes(b"kernel")
                dts = product / "arch" / "orlix" / "boot" / "dts"
                dts.mkdir(parents=True)
                development_dtb = dts / "development.dtb"
                release_dtb = dts / "release.dtb"
                development_dtb.write_bytes(b"development")
                release_dtb.write_bytes(b"release")
                manifest = artifact_manifest_v2(
                    artifacts={
                        "OrlixKernel.a": archive,
                        "arch/orlix/boot/dts/development.dtb": development_dtb,
                        "arch/orlix/boot/dts/release.dtb": release_dtb,
                    }
                )
                (root / V2_MANIFEST_FILENAME).write_bytes(manifest)
                (root / V2_DIGEST_FILENAME).write_text(
                    hashlib.sha256(manifest).hexdigest() + "\n", encoding="ascii"
                )
            digest = compare.compare_trees(
                str(roots[0]),
                str(roots[1]),
                artifact_format=ARTIFACT_IDENTITY_V2_FORMAT,
                component="kernel-release-iphoneos",
            )
            self.assertEqual(len(digest), 64)
            (roots[1] / "product" / "extra.bin").write_bytes(b"unexpected")
            with self.assertRaisesRegex(ValueError, "product files differ"):
                compare.compare_trees(
                    str(roots[0]),
                    str(roots[1]),
                    artifact_format=ARTIFACT_IDENTITY_V2_FORMAT,
                    component="kernel-release-iphoneos",
                )

    def test_v2_proposal_preserves_typed_identity(self) -> None:
        digest = "a" * 64
        with tempfile.TemporaryDirectory() as tmp:
            proposal = Path(tmp) / "proposal.json"
            compare.write_proposal(
                str(proposal),
                "kernel-release-iphoneos",
                digest,
                artifact_identity={
                    "format": ARTIFACT_IDENTITY_V2_FORMAT,
                    "version": ARTIFACT_IDENTITY_V2_VERSION,
                    "digest": digest,
                },
            )
            payload = json.loads(proposal.read_text(encoding="utf-8"))
        self.assertEqual(payload["schema"], 2)
        self.assertEqual(payload["artifact_identity"]["format"], ARTIFACT_IDENTITY_V2_FORMAT)
        self.assertEqual(payload["artifact_identity"]["version"], ARTIFACT_IDENTITY_V2_VERSION)

    def test_matching_digests_write_unsigned_proposal(self) -> None:
        digest = "a" * 64
        with tempfile.TemporaryDirectory() as tmp:
            first_tree, second_tree = Path(tmp) / "a", Path(tmp) / "b"
            first_tree.mkdir()
            second_tree.mkdir()
            first = first_tree / "digest.sha256"
            second = second_tree / "digest.sha256"
            proposal = Path(tmp) / "proposal.json"
            first.write_text(digest + "\n", encoding="utf-8")
            second.write_text(digest + "\n", encoding="utf-8")
            self.assertEqual(compare.compare_digests(str(first), str(second)), digest)
            env = os.environ.get("ORLIX_COSIGN_KEY")
            if env is not None:
                del os.environ["ORLIX_COSIGN_KEY"]
            try:
                self.assertEqual(
                    compare.main(
                        [str(first), str(second), "--first-tree", str(first_tree), "--second-tree", str(second_tree), "--component", "uapi", "--proposal", str(proposal)]
                    ),
                    0,
                )
            finally:
                if env is not None:
                    os.environ["ORLIX_COSIGN_KEY"] = env
            payload = json.loads(proposal.read_text(encoding="utf-8"))
            self.assertEqual(payload["unsigned_digest"], digest)
            self.assertIs(payload["signed"], False)
            self.assertIsNone(payload["oci_digest"])
            self.assertEqual(payload["component"], "uapi")
            self.assertEqual(payload["output_tree_digest"], compare.tree_digest(first_tree))
            self.assertNotIn("signature", payload)
            self.assertNotIn("ghcr", json.dumps(payload))

    def test_mismatch_fails_loud(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            first_tree, second_tree = Path(tmp) / "a", Path(tmp) / "b"
            first_tree.mkdir()
            second_tree.mkdir()
            first = first_tree / "digest.sha256"
            second = second_tree / "digest.sha256"
            first.write_text("a" * 64 + "\n", encoding="utf-8")
            second.write_text("b" * 64 + "\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                compare.compare_digests(str(first), str(second))

    def test_unsigned_lock_proposal_does_not_mutate_signed_lock(self) -> None:
        digest = "d" * 64
        signed_lock = {
            "schema": 1,
            "buildset": "e" * 64,
            "components": {
                "uapi": {
                    "unsigned_digest": digest,
                    "oci_digest": "sha256:" + ("f" * 64),
                    "oci_reference": "localhost:5001/orlix/uapi@sha256:" + ("f" * 64),
                }
            },
        }
        with tempfile.TemporaryDirectory() as tmp:
            first_tree, second_tree = Path(tmp) / "a", Path(tmp) / "b"
            first_tree.mkdir()
            second_tree.mkdir()
            first = first_tree / "digest.sha256"
            second = second_tree / "digest.sha256"
            proposal = Path(tmp) / "lock-proposal.json"
            lock_path = Path(tmp) / "artifacts.lock.json"
            first.write_text(digest + "\n", encoding="utf-8")
            second.write_text(digest + "\n", encoding="utf-8")
            lock_path.write_text(json.dumps(signed_lock, indent=2) + "\n", encoding="utf-8")
            before = lock_path.read_text(encoding="utf-8")
            self.assertEqual(
                compare.main(
                    [
                        str(first),
                        str(second),
                        "--first-tree", str(first_tree),
                        "--second-tree", str(second_tree),
                        "--component",
                        "uapi",
                        "--lock-proposal",
                        str(proposal),
                        "--lock",
                        str(lock_path),
                    ]
                ),
                0,
            )
            self.assertEqual(lock_path.read_text(encoding="utf-8"), before)
            payload = json.loads(proposal.read_text(encoding="utf-8"))
            self.assertIs(payload["signed"], False)
            self.assertIsNone(payload["buildset"])

    def test_unset_cosign_key_does_not_invent_signature(self) -> None:
        digest = "c" * 64
        with tempfile.TemporaryDirectory() as tmp:
            first = Path(tmp) / "a.sha256"
            proposal = Path(tmp) / "proposal.json"
            first.write_text(digest + "\n", encoding="utf-8")
            os.environ.pop("ORLIX_COSIGN_KEY", None)
            compare.write_proposal(str(proposal), "uapi", digest)
            payload = json.loads(proposal.read_text(encoding="utf-8"))
            self.assertIs(payload["signed"], False)
            self.assertIsNone(payload["oci_digest"])

    def test_make_promote_uses_unique_component_output_roots(self) -> None:
        text = Path(__file__).resolve().parents[2] / "make" / "bazel-migration.mk"
        body = text.read_text(encoding="utf-8")
        self.assertIn('promote="$(ORLIX_BUILD_ROOT)/Bazel/promote/$(1)"', body)
        self.assertIn("--nouse_action_cache", body)
        promote = body.split("define ORLIX_BAZEL_PROMOTE")[1].split("endef")[0]
        self.assertIn("--remote_cache= --remote_executor=", promote)
        self.assertNotIn("ORLIX_KBUILD_PERSIST", body)
        self.assertIn("bazel/promotion/sign.py", body)
        self.assertIn("unsigned promote must not Cosign-sign", body)
        self.assertIn("env -u ORLIX_COSIGN_KEY -u ORLIX_PROMOTE_ARTIFACT", body)
        self.assertIn("unsigned promote mutated artifacts.lock.json", body)
        self.assertIn("__bazel-reconstruct-source", body)
        self.assertIn("--config=promoted", body)
        self.assertIn("--lock", body)
        self.assertIn("ORLIX_BAZEL_PROMOTE,uapi,", body)
        self.assertIn("ORLIX_BAZEL_PROMOTE,mlibc,", body)
        self.assertIn("ORLIX_BAZEL_PROMOTE,rootfs,", body)
        self.assertIn("feasibility/rootfs/rootfs/source-input.sha256", body)
        self.assertIn("ORLIX_BAZEL_PUBLISH,uapi,feasibility/kernel/uapi/uapi.sha256", body)
        self.assertIn("--out-dir", body)
        self.assertNotIn("ORLIX_GHCR_REPOSITORY", body.split("ORLIX_BAZEL_PROMOTE")[1].split("endef")[0])

    def test_package_digest_hashes_relative_paths(self) -> None:
        root = Path(__file__).resolve().parents[2]
        for rel in (
            "bazel/feasibility/packages/autotools.bzl",
            "bazel/feasibility/packages/coreutils.bzl",
            "bazel/feasibility/packages/bash.bzl",
        ):
            body = (root / rel).read_text(encoding="utf-8")
            self.assertIn('cd "$install_out"', body)
            self.assertIn("-ffile-prefix-map", body)
        rootfs = (root / "bazel/feasibility/rootfs/rootfs.bzl").read_text(encoding="utf-8")
        self.assertIn('cd "$base_tree"', rootfs)
        self.assertIn('shasum -a 256 < "$payload_metadata"', rootfs)
        self.assertIn('"$gen_init_cpio" -t 1', rootfs)
        self.assertIn("package_closure = depset(transitive = [pkg.artifact_identity_closure for pkg in pkgs])", rootfs)
        self.assertIn('"MKE2FS_CONFIG": "/opt/homebrew/etc/mke2fs.conf"', rootfs)
        self.assertIn('"rootfs",', rootfs)
        self.assertIn('rootfs_tools.add(Path("/opt/homebrew/etc/mke2fs.conf"))', (root / "bazel/config/toolchain_pin.py").read_text(encoding="utf-8"))
        self.assertIn("@orlix_kernel_toolchain//:rootfs-identity.json", rootfs)
        self.assertIn('"rootfs-identity.json"', (root / "bazel/extensions/native_sources.bzl").read_text(encoding="utf-8"))
        artifact_rule = (root / "bazel/artifact_identity.bzl").read_text(encoding="utf-8")
        self.assertIn('"no-sandbox": "1"', artifact_rule)
        self.assertNotIn("tool_identity", artifact_rule)
        self.assertIn("kbuild-archive.tar", (root / "bazel/feasibility/analysis/providers.bzl").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
