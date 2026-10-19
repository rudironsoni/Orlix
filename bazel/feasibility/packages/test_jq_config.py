"""Two output bases must compile the same jq configure line."""

from __future__ import annotations

import unittest
from pathlib import Path

from bazel.feasibility.packages.jq_config import jq_config_text


ROOT = Path(__file__).resolve().parents[3]


class JqConfigTests(unittest.TestCase):
    def test_two_output_bases_record_one_jq_config_line(self) -> None:
        def line(root: str) -> str:
            work = f"{root}/orlix-package-state/jq/aarch64-linux-gnu"
            tool_bin = f"{root}/external/+native_sources+orlix_kernel_toolchain/tools/bin"
            config = (
                "--host=aarch64-linux-gnu --build=aarch64-apple-darwin --prefix=/usr "
                f"'CC={tool_bin}/ccache {tool_bin}/clang --target=aarch64-linux-gnu "
                f"-isystem {work}/inputs/headers -isystem {work}/inputs/uapi/include "
                f"-ffile-prefix-map={work}=. -ffile-prefix-map={tool_bin}=/orlix/tools' "
                "'CFLAGS=-O2 -ffile-prefix-map=..=.' "
                f"'LDFLAGS=-fuse-ld=lld -L{work}/inputs/libraries "
                f"{work}/inputs/libraries/crt1.o {work}/inputs/runtime/libcompiler_rt.a' "
                "LIBS= CPPFLAGS=-Imodules/oniguruma/src"
            )
            return jq_config_text(config, work, tool_bin)

        seed = "/Users/runner/work/Orlix/Orlix/Build/Bazel/cache-equivalence.qcNZTm/seed/output-base"
        uncached = "/Users/runner/work/Orlix/Orlix/Build/Bazel/cache-equivalence.qcNZTm/uncached/output-base"
        recorded = line(seed)
        self.assertEqual(recorded, line(uncached))
        self.assertEqual(recorded.count("\n"), 1)
        self.assertTrue(recorded.startswith('#define JQ_CONFIG "'))
        self.assertNotIn("output-base", recorded)
        self.assertNotIn("/Users/", recorded)
        self.assertIn("/orlix/tools/clang", recorded)
        self.assertIn("-isystem ./inputs/headers", recorded)

    def test_package_build_writes_config_opts_before_gmake(self) -> None:
        text = (ROOT / "bazel/feasibility/packages/autotools.bzl").read_text(encoding="utf-8")
        configure = text.index('"$conf" $CONFIGURE_ARGS $maintainer_mode')
        write = text.index("src/config_opts.inc", configure)
        gmake = text.index('gmake_bin="$tool_bin/gmake"', write)
        self.assertLess(configure, write)
        self.assertLess(write, gmake)
        self.assertIn("write_config_opts", text)
        self.assertNotIn("llvm-objcopy", text)
