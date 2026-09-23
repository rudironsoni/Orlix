"""Source and promoted action identity from the shipped Bazel graph."""

from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
BAZEL = Path.home() / "Library/Caches/Orlix/Tools/bazel/9.2.0/bazel"
OUTPUT_BASE = REPO / "Build/Bazel/output-base"
PIN = json.loads((REPO / "bazel/config/toolchain-pin.json").read_text(encoding="utf-8"))


def _xcode() -> tuple[str, str, str]:
    developer_dir = subprocess.check_output(["/usr/bin/xcode-select", "-p"], text=True).strip()
    text = subprocess.check_output(
        ["/usr/bin/xcodebuild", "-version"],
        text=True,
        env={**os.environ, "DEVELOPER_DIR": developer_dir},
    )
    version = ""
    build = ""
    for line in text.splitlines():
        if line.startswith("Xcode "):
            version = line.split()[1]
        elif line.startswith("Build version "):
            build = line.split()[2]
    if not version or not build:
        raise AssertionError(f"unreadable xcodebuild version:\n{text}")
    return developer_dir, version, build


def _disk_namespace(build: str) -> str:
    for entry in [PIN["product_pin"], *PIN["allowed_local"]]:
        if entry["xcode_build"] == build:
            return entry["disk_cache_namespace"]
    raise AssertionError(f"toolchain pin has no disk cache namespace for {build}")


def _bazel(developer_dir: str, version: str, build: str, verb: str, expr: list[str], config: str, extra: list[str]) -> subprocess.CompletedProcess[str]:
    namespace = _disk_namespace(build)
    cache_root = Path.home() / "Library/Caches/Orlix/Bazel"
    command = [
        str(BAZEL),
        f"--output_base={OUTPUT_BASE}",
        verb,
        *expr,
        "--config=release",
        f"--config={config}",
        "--compilation_mode=dbg",
        "--apple_platform_type=ios",
        "--ios_multi_cpus=sim_arm64",
        "--platforms=@build_bazel_apple_support//platforms:ios_sim_arm64",
        f"--xcode_version={version}",
        f"--repo_env=DEVELOPER_DIR={developer_dir}",
        f"--host_action_env=ORLIX_PINNED_DEVELOPER_DIR={developer_dir}",
        f"--action_env=ORLIX_PINNED_DEVELOPER_DIR={developer_dir}",
        f"--disk_cache={cache_root / 'disk-cache' / namespace}",
        f"--repository_cache={cache_root / 'repository-cache'}",
        *extra,
    ]
    return subprocess.run(
        command,
        text=True,
        capture_output=True,
        env={**os.environ, "DEVELOPER_DIR": developer_dir, "ORLIX_PINNED_DEVELOPER_DIR": developer_dir},
        check=False,
    )


def _actions(text: str) -> list[dict[str, list[str]]]:
    found: list[dict[str, list[str]]] = []
    current: dict[str, list[str]] | None = None
    for line in text.splitlines():
        if line.startswith("action "):
            if current is not None:
                found.append(current)
            current = {"header": [line]}
            continue
        if current is None or not line.startswith("  "):
            continue
        if line.startswith("    ") and current:
            key = next(reversed(current))
            if key != "header":
                current[key][-1] += " " + line.strip()
            continue
        if ": " not in line:
            continue
        key, value = line.strip().split(": ", 1)
        current.setdefault(key, []).append(value)
    if current is not None:
        found.append(current)
    return found


def _one(actions: list[dict[str, list[str]]], mnemonic: str) -> dict[str, list[str]]:
    chosen = [item for item in actions if item.get("Mnemonic") == [mnemonic]]
    if len(chosen) != 1:
        raise AssertionError(f"expected one {mnemonic}, found {len(chosen)}")
    return chosen[0]


class OriginActionIdentityTests(unittest.TestCase):
    def _require_promoted_kernel(self) -> None:
        archive = REPO / "bazel/promotion/imported/kernel-release-iphonesimulator/product/OrlixKernel.a"
        if not archive.is_file():
            self.skipTest("promoted kernel archive is absent; run make __bazel-substitute-promoted")

    @classmethod
    def setUpClass(cls) -> None:
        if not BAZEL.is_file():
            raise AssertionError(f"missing Bazel binary: {BAZEL}")
        cls.developer_dir, cls.version, cls.build = _xcode()

    def _aquery(self, config: str, expression: str) -> str:
        result = _bazel(
            self.developer_dir,
            self.version,
            self.build,
            "aquery",
            [expression, "--output=text"],
            config,
            ["--include_commandline"],
        )
        if result.returncode != 0:
            raise AssertionError(result.stderr[-4000:])
        return result.stdout

    def test_composition_template_ignores_origin(self) -> None:
        self._require_promoted_kernel()
        expression = 'mnemonic("OrlixKernelComposition", //bazel/product:kernel_composition)'
        source_text = self._aquery("source", expression)
        promoted_text = self._aquery("promoted", expression)
        source = _one(_actions(source_text), "OrlixKernelComposition")
        promoted = _one(_actions(promoted_text), "OrlixKernelComposition")
        self.assertEqual(source["ActionKey"], promoted["ActionKey"])
        for mode, text in ((source, source_text), (promoted, promoted_text)):
            inputs = " ".join(mode.get("Inputs", []))
            self.assertIn("selected_macho/OrlixKernel.a", inputs)
            self.assertIn("artifact_identity_v2", text)
            self.assertIn("OrlixKernel.a", text)

    def test_rootfs_payload_ignores_origin(self) -> None:
        expression = 'mnemonic("OrlixRootfsPayload", //bazel/feasibility/rootfs:payload)'
        source = _one(_actions(self._aquery("source", expression)), "OrlixRootfsPayload")
        promoted = _one(_actions(self._aquery("promoted", expression)), "OrlixRootfsPayload")
        self.assertEqual(source["ActionKey"], promoted["ActionKey"])
        for action in (source, promoted):
            text = " ".join(action.get("Inputs", []) + action.get("Arguments", []))
            self.assertIn("selected_rootfs/initramfs.cpio.gz", text)
            self.assertIn("selected_rootfs/base.ext4", text)
            self.assertIn("selected_rootfs/state.ext4", text)
            self.assertNotIn("promoted_rootfs", text)

    def test_getconf_archives_ignore_origin(self) -> None:
        expression = 'mnemonic("OrlixGuestPackageTreeArchive", //bazel/feasibility/packages:getconf)'
        source = _actions(self._aquery("source", expression))
        promoted = _actions(self._aquery("promoted", expression))
        self.assertEqual(
            sorted(item["ActionKey"][0] for item in source),
            sorted(item["ActionKey"][0] for item in promoted),
        )
        def blob(action: dict[str, list[str]]) -> str:
            parts: list[str] = []
            for values in action.values():
                parts.extend(values)
            return " ".join(parts)

        headers = [item for item in source if "headers.tar" in blob(item)]
        libraries = [item for item in source if "libraries.tar" in blob(item)]
        self.assertEqual(len(headers), 1)
        self.assertEqual(len(libraries), 1)
        self.assertIn("-chf", blob(headers[0]))
        self.assertIn("-chf", blob(libraries[0]))
        uapi = [item for item in source if "uapi.tar" in blob(item)]
        self.assertEqual(len(uapi), 1)
        self.assertIn("-chf", blob(uapi[0]))
        self.assertIn("selected_uapi", " ".join(uapi[0]["Inputs"]))
        self.assertNotIn("promoted_uapi", " ".join(uapi[0]["Inputs"]))
        stage = 'mnemonic("OrlixSelectSysrootHeaders", //bazel/feasibility/mlibc:selected_sysroot)'
        staged = _one(_actions(self._aquery("source", stage)), "OrlixSelectSysrootHeaders")
        self.assertIn("-R -L", blob(staged))
        package = 'mnemonic("OrlixGuestPackage", //bazel/feasibility/packages:getconf)'
        source_package = _one(_actions(self._aquery("source", package)), "OrlixGuestPackage")
        promoted_package = _one(_actions(self._aquery("promoted", package)), "OrlixGuestPackage")
        source_inputs = " ".join(source_package.get("Inputs", []))
        promoted_inputs = " ".join(promoted_package.get("Inputs", []))
        for inputs, action in (
            (source_inputs, source_package),
            (promoted_inputs, promoted_package),
        ):
            self.assertIn("selected_uapi/uapi.sha256", inputs)
            self.assertIn("selected_sysroot/sysroot.sha256", inputs)
            self.assertNotIn("promoted_uapi", inputs)
            self.assertNotIn("promoted_sysroot", inputs)
            command = " ".join(action.get("Command Line", []) + action.get("Arguments", []))
            self.assertIn("--consumer", command)
            self.assertIn("--consumer-file", command)

    def test_selected_macho_keeps_origin_on_the_producer_side(self) -> None:
        self._require_promoted_kernel()
        expression = "//bazel/feasibility/kernel:selected_macho"
        source = _actions(self._aquery("source", expression))
        promoted = _actions(self._aquery("promoted", expression))

        def archive(actions: list[dict[str, list[str]]]) -> dict[str, list[str]]:
            chosen = [
                item
                for item in actions
                if item.get("Mnemonic") == ["Symlink"]
                and any("selected_macho/OrlixKernel.a" in value for value in item.get("Outputs", []))
            ]
            self.assertEqual(len(chosen), 1)
            return chosen[0]

        source_archive = archive(source)
        promoted_archive = archive(promoted)
        self.assertEqual(source_archive["Outputs"], promoted_archive["Outputs"])
        self.assertIn("/kernel/macho/OrlixKernel.a", source_archive["Inputs"][0])
        self.assertIn(
            "promoted_kernel_release_iphonesimulator/verified/OrlixKernel.a",
            promoted_archive["Inputs"][0],
        )

    def test_promoted_selection_cache_miss_preserves_archive_bytes(self) -> None:
        self._require_promoted_kernel()
        target = ["//bazel/feasibility/kernel:selected_macho"]
        first = _bazel(
            self.developer_dir,
            self.version,
            self.build,
            "build",
            target,
            "promoted",
            [],
        )
        self.assertEqual(first.returncode, 0, first.stderr[-4000:])
        listed = _bazel(
            self.developer_dir,
            self.version,
            self.build,
            "cquery",
            ["//bazel/feasibility/kernel:selected_macho", "--output=files"],
            "promoted",
            [],
        )
        self.assertEqual(listed.returncode, 0, listed.stderr[-4000:])
        archive = next(
            line
            for line in listed.stdout.splitlines()
            if line.endswith("selected_macho/OrlixKernel.a")
        )
        path = OUTPUT_BASE / "execroot" / "_main" / archive
        before = hashlib.sha256(path.read_bytes()).hexdigest()
        path.unlink()
        probe = REPO / "Build/Bazel/cache-miss-probe"
        probe.mkdir(parents=True, exist_ok=True)
        second = _bazel(
            self.developer_dir,
            self.version,
            self.build,
            "build",
            target,
            "promoted",
            [f"--disk_cache={probe}", "--nouse_action_cache"],
        )
        self.assertEqual(second.returncode, 0, second.stderr[-4000:])
        executed = re.search(r"INFO: (\d+) process", second.stderr)
        self.assertIsNotNone(executed, second.stderr[-2000:])
        self.assertGreater(int(executed.group(1)), 0, second.stderr[-2000:])
        after = hashlib.sha256(path.read_bytes()).hexdigest()
        self.assertEqual(before, after)


if __name__ == "__main__":
    unittest.main()
