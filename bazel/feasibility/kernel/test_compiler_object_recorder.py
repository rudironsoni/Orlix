"""The recorder reads ccache statistics. A wrapper exit of 0 is not a hit."""

from __future__ import annotations

import json
import os
from pathlib import Path
import platform
import tempfile
import unittest
from unittest import mock

from bazel.compiler_cache import forward_shell_env
from bazel.feasibility.kernel.compiler_object_recorder import (
    classify,
    join_records,
    parse_print_stats,
    record,
    summarize,
)
from bazel.feasibility.kernel.compiler_remote_experiment import (
    _blocked,
    bazel_flags,
    host_blocker,
    select_storage,
)


ROOT = Path(__file__).resolve().parents[3]


def _stats(**values: int) -> dict[str, int]:
    base = {name: 0 for name in (
        "direct_cache_hit",
        "preprocessed_cache_hit",
        "cache_miss",
        "compile_failed",
        "compiler_produced_stdout",
        "compiler_produced_no_output",
        "compiler_produced_empty_output",
        "called_for_preprocessing",
        "remote_storage_hit",
        "remote_storage_read_hit",
        "local_storage_hit",
    )}
    base.update(values)
    return base


class RecorderTests(unittest.TestCase):
    def test_exit_zero_without_a_hit_counter_is_not_a_hit(self) -> None:
        before = _stats()
        row = classify(before, before, 0)
        self.assertEqual(row["wrapper"], "unobserved")
        self.assertFalse(row["compiler_executed"])
        self.assertEqual(row["exit_code"], 0)
        self.assertEqual(row["stats_source"], "ccache --print-stats")

    def test_parse_print_stats_requires_counters(self) -> None:
        parsed = parse_print_stats("direct_cache_hit\t1\ncache_miss\t0\n")
        self.assertEqual(parsed["direct_cache_hit"], 1)
        with self.assertRaises(ValueError):
            parse_print_stats("Hits: 1\nMisses: 0\n")

    def test_disabled_cache_does_not_forward_remote_storage(self) -> None:
        forwarded = forward_shell_env({
            "CCACHE_DIR": "/var/cache/orlix",
            "ORLIX_COMPILER_REMOTE_STORAGE": "file:/var/cache/remote",
            "ORLIX_COMPILER_REMOTE_ONLY": "1",
            "CCACHE_DISABLE": "1",
        })
        self.assertEqual(forwarded, {"CCACHE_DISABLE": "1"})
        enabled = forward_shell_env({
            "CCACHE_DIR": "/var/cache/orlix",
            "ORLIX_COMPILER_REMOTE_STORAGE": "file:/var/cache/remote",
            "ORLIX_COMPILER_REMOTE_ONLY": "1",
        })
        self.assertEqual(enabled["ORLIX_COMPILER_REMOTE_STORAGE"], "file:/var/cache/remote")
        self.assertEqual(enabled["ORLIX_COMPILER_REMOTE_ONLY"], "1")

    def test_pull_request_cannot_receive_the_publication_url(self) -> None:
        secret = "http://publisher.example/cache|bearer-token=secret"
        for role in ("producer", "consumer"):
            selected = select_storage(
                event="pull_request",
                ref="refs/pull/433/head",
                write_url=secret,
                read_url=secret,
                role=role,
            )
            self.assertEqual(selected["access"], "off")
            self.assertEqual(selected["url"], "")
            self.assertNotIn("secret", selected["reason"])
        branch = select_storage(
            event="push",
            ref="refs/heads/cursor/source-mutation-matrix-v8-26ef",
            write_url=secret,
            read_url="",
            role="producer",
        )
        self.assertEqual(branch["access"], "off")
        self.assertEqual(branch["url"], "")
        main = select_storage(
            event="push",
            ref="refs/heads/main",
            write_url=secret,
            read_url="",
            role="producer",
        )
        self.assertEqual(main["access"], "write")
        self.assertEqual(main["url"], secret)
        consumer = select_storage(
            event="push",
            ref="refs/heads/main",
            write_url=secret,
            read_url="",
            role="consumer",
        )
        self.assertEqual(consumer["access"], "read")
        self.assertIn("|read-only", consumer["url"])
        self.assertNotIn(secret, json.dumps({key: value for key, value in consumer.items() if key != "url"}))

    def test_consumer_flags_disable_bazel_caches_and_keep_remote_ccache(self) -> None:
        flags = bazel_flags(
            bazel="bazel",
            output_base=Path("/tmp/consumer"),
            developer_dir="/xcode",
            xcode_version="17.0",
            ccache_dir=Path("/tmp/empty-ccache"),
            remote_url="file:/tmp/remote",
            remote_only=True,
            label="//bazel/feasibility/kernel:macho",
        )
        text = " ".join(flags)
        self.assertIn("--nouse_action_cache", text)
        self.assertIn("--disk_cache=", text)
        self.assertIn("--remote_cache=", text)
        self.assertIn("--action_env=CCACHE_DISABLE=", text)
        self.assertNotIn("--action_env=CCACHE_DISABLE=1", text)
        self.assertIn("--action_env=ORLIX_COMPILER_REMOTE_ONLY=1", text)
        self.assertIn("--action_env=CCACHE_DIR=/tmp/empty-ccache", text)
        disabled = " ".join(bazel_flags(
            bazel="bazel",
            output_base=Path("/tmp/uapi"),
            developer_dir="/xcode",
            xcode_version="17.0",
            ccache_dir=Path("/tmp/unused"),
            remote_url="file:/tmp/remote",
            remote_only=True,
            label="//bazel/feasibility/kernel:uapi",
            ccache_disable=True,
        ))
        self.assertIn("--action_env=CCACHE_DISABLE=1", disabled)
        self.assertNotIn("ORLIX_COMPILER_REMOTE_STORAGE", disabled)

    def test_kernel_action_keeps_one_kbuild_and_records_from_ccache(self) -> None:
        text = (ROOT / "bazel/feasibility/kernel/kernel_macho.bzl").read_text(encoding="utf-8")
        self.assertEqual(text.count('mnemonic = "OrlixKernelMachOArchive"'), 1)
        self.assertIn('"block-network": "1"', text)
        self.assertIn("compiler_object_recorder.py", text)
        self.assertIn('export CCACHE_REMOTE_STORAGE="$ORLIX_COMPILER_REMOTE_STORAGE"', text)
        self.assertIn('export ORLIX_COMPILER_LAUNCHER=', text)
        workflow = (ROOT / ".github/workflows/bazel-ci.yml").read_text(encoding="utf-8")
        self.assertIn("orlix-ccache-${{ steps.compiler.outputs.digest }}", workflow)
        self.assertNotIn("ORLIX_COMPILER_REMOTE_STORAGE_WRITE", workflow)
        for path in (ROOT / ".github/workflows").glob("*.yml"):
            self.assertNotIn("ORLIX_COMPILER_REMOTE_STORAGE_WRITE", path.read_text(encoding="utf-8"), path.name)
        policy = (ROOT / "make/bazel-migration.mk").read_text(encoding="utf-8")
        self.assertIn('if [ "$$mode" = normal ] && [ "$$context" = main ]; then access=write; fi', policy)
        self.assertIn('if [ "$$mode" = normal ] && [ "$$context" = pr ]; then access=read; fi', policy)
        self.assertIn("__bazel-kernel-compiler-remote-same:", policy)
        self.assertIn("__bazel-kernel-compiler-remote-edit:", policy)

    def test_join_records_skips_separately_from_hits(self) -> None:
        units = [{
            "unit": "/work/arch/orlix/kernel/time.c",
            "kbuild": "issued",
            "wrapper": "hit",
            "compiler_executed": False,
            "remote_storage_hit": 1,
            "transferred_bytes": None,
            "stats_source": "ccache --print-stats",
        }]
        record = join_records(
            units,
            "  ORLIXCC iphonesimulator arch/orlix/kernel/time.c\n",
            ["/work/arch/orlix/kernel/idle.c"],
            "abc",
        )
        by_unit = {str(row["unit"]): row for row in record["units"]}
        self.assertEqual(by_unit["/work/arch/orlix/kernel/time.c"]["wrapper"], "hit")
        self.assertEqual(by_unit["/work/arch/orlix/kernel/idle.c"]["kbuild"], "skipped")
        self.assertIsNone(by_unit["/work/arch/orlix/kernel/idle.c"]["wrapper"])
        self.assertFalse(by_unit["/work/arch/orlix/kernel/idle.c"]["compiler_executed"])
        summary = summarize(record)
        self.assertEqual(summary["remote_object_hits"], 1)
        self.assertEqual(summary["compiler_processes_executed"], 0)
        self.assertIsNone(summary["transferred_bytes"])

    def test_fake_ccache_exit_zero_stays_unobserved(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            ccache = root / "ccache"
            ccache.write_text(
                "#!/bin/sh\n"
                "if [ \"$1\" = --print-stats ]; then printf '%s\\n' 'direct_cache_hit\t0' 'cache_miss\t0'; exit 0; fi\n"
                "exit 0\n",
                encoding="utf-8",
            )
            ccache.chmod(0o755)
            record_path = root / "units.jsonl"
            env = os.environ.copy()
            env.update({
                "ORLIX_CCACHE_BIN": str(ccache),
                "ORLIX_COMPILER_CACHE_RECORD": str(record_path),
                "CCACHE_DIR": str(root / "cache"),
                "PATH": os.environ["PATH"],
            })
            with mock.patch.dict(os.environ, env, clear=False):
                status = record(["clang", "-c", "leaf.c", "-o", "leaf.o"])
            self.assertEqual(status, 0)
            row = json.loads(record_path.read_text(encoding="utf-8"))
            self.assertEqual(row["wrapper"], "unobserved")
            self.assertFalse(row["compiler_executed"])
            self.assertEqual(row["exit_code"], 0)

    def test_real_ccache_separates_remote_hits_from_compiler_execution(self) -> None:
        ccache = "/usr/bin/ccache"
        compiler = "/usr/bin/clang"
        if not Path(ccache).is_file() or not Path(compiler).is_file():
            self.skipTest("ccache and clang are required")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            remote = root / "remote"
            remote.mkdir()
            leaf = root / "leaf.c"
            other = root / "other.c"
            leaf.write_text("int leaf(int n) { return n + 1; }\n", encoding="utf-8")
            other.write_text("int other(int n) { return n + 2; }\n", encoding="utf-8")
            prefix_log = root / "compiler.log"
            prefix = root / "ran.sh"
            prefix.write_text(
                "#!/bin/sh\n"
                "printf '%s\\n' \"$1\" >> \"$CCACHE_PREFIX_LOG\"\n"
                "exec \"$@\"\n",
                encoding="utf-8",
            )
            prefix.chmod(0o755)

            def environment(cache: Path, *, remote_only: bool) -> dict[str, str]:
                env = os.environ.copy()
                env.update({
                    "ORLIX_CCACHE_BIN": ccache,
                    "CCACHE_DIR": str(cache),
                    "CCACHE_CONFIGPATH": "/dev/null",
                    "CCACHE_COMPILERCHECK": "content",
                    "CCACHE_REMOTE_STORAGE": f"file:{remote}",
                    "CCACHE_PREFIX": str(prefix),
                    "CCACHE_PREFIX_LOG": str(prefix_log),
                    "PATH": os.environ["PATH"],
                })
                if remote_only:
                    env["CCACHE_REMOTE_ONLY"] = "1"
                else:
                    env.pop("CCACHE_REMOTE_ONLY", None)
                return env

            def compile_one(env: dict[str, str], source: Path, output: Path) -> dict[str, object]:
                record_path = root / f"{output.name}.jsonl"
                record_path.write_text("", encoding="utf-8")
                env = dict(env)
                env["ORLIX_COMPILER_CACHE_RECORD"] = str(record_path)
                with mock.patch.dict(os.environ, env, clear=True):
                    status = record([compiler, "-c", str(source), "-o", str(output)])
                self.assertEqual(status, 0, output.name)
                return json.loads(record_path.read_text(encoding="utf-8").strip())

            producer = environment(root / "producer", remote_only=False)
            prefix_log.write_text("", encoding="utf-8")
            miss = compile_one(producer, leaf, root / "leaf.o")
            self.assertEqual(miss["wrapper"], "miss")
            self.assertTrue(miss["compiler_executed"])
            self.assertEqual(miss["exit_code"], 0)
            self.assertGreaterEqual(prefix_log.read_text(encoding="utf-8").count("clang"), 1)
            other_miss = compile_one(producer, other, root / "other.o")
            self.assertEqual(other_miss["wrapper"], "miss")
            self.assertTrue(other_miss["compiler_executed"])

            consumer = environment(root / "consumer", remote_only=True)
            (root / "consumer").mkdir()
            before = prefix_log.read_text(encoding="utf-8").count("\n")
            hit = compile_one(consumer, leaf, root / "leaf-remote.o")
            self.assertEqual(hit["wrapper"], "hit")
            self.assertFalse(hit["compiler_executed"])
            self.assertGreaterEqual(int(hit["remote_storage_hit"]), 1)
            self.assertEqual(hit["exit_code"], 0)
            self.assertEqual(prefix_log.read_text(encoding="utf-8").count("\n"), before)
            self.assertEqual((root / "leaf.o").read_bytes(), (root / "leaf-remote.o").read_bytes())
            changed = leaf.read_text(encoding="utf-8").replace("n + 1", "n + 3")
            leaf.write_text(changed, encoding="utf-8")
            before = prefix_log.read_text(encoding="utf-8").count("\n")
            edited = compile_one(consumer, leaf, root / "leaf-edited.o")
            kept = compile_one(consumer, other, root / "other-remote.o")
            self.assertEqual(edited["wrapper"], "miss")
            self.assertTrue(edited["compiler_executed"])
            self.assertGreater(prefix_log.read_text(encoding="utf-8").count("\n"), before)
            self.assertEqual(kept["wrapper"], "hit")
            self.assertFalse(kept["compiler_executed"])
            self.assertGreaterEqual(int(kept["remote_storage_hit"]), 1)

    def test_this_host_does_not_claim_the_kernel_experiment(self) -> None:
        blocker = host_blocker()
        if platform.system() == "Darwin":
            developer = os.environ.get("ORLIX_PINNED_DEVELOPER_DIR", "")
            if developer and Path(developer).is_dir():
                self.assertIsNone(blocker)
            else:
                self.assertIn("ORLIX_PINNED_DEVELOPER_DIR", str(blocker))
        else:
            self.assertIsNotNone(blocker)
            self.assertIn("not macOS", str(blocker))
            self.assertIn("did not execute", str(blocker))
        # A prepared macOS host has no host blocker. That is not experiment A or B.
        record = _blocked(
            "same-commit",
            blocker or "host is prepared; the remote compiler-object experiment was not executed",
        )
        self.assertEqual(record["status"], "not_executed")
        self.assertIs(record["executed"], False)
        self.assertIsNone(record["remote_object_hits"])
        self.assertIsNone(record["compiler_processes_executed"])
        self.assertIsNone(record["output_digest"])


if __name__ == "__main__":
    unittest.main()
