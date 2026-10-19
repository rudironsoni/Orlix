---
type: task
tags:
  - task
  - bazel
  - cache
  - artifacts
updated: 2026-10-19
status: doing
summary: "Reuse compatible Bazel results and signed guest/native component buildsets across Apple builds without sharing mutable worktree state."
task_of:
  - "[Route every Apple build through Bazel](../../story/doing/route-every-apple-build-through-bazel.md)"
depends_on:
  - "[Define the supported Apple Bazel build matrix](../done/define-supported-apple-bazel-build-matrix.md)"
  - "[Implement component and buildset promotion](implement-component-and-buildset-promotion.md)"
  - "[Implement protected Bazel automation](implement-protected-bazel-automation.md)"
blocks:
  - "[Prove all supported Apple builds and feature gates](prove-all-supported-apple-builds-and-feature-gates.md)"
owned_paths:
  - ".bazelrc"
  - ".gitignore"
  - "Brewfile"
  - "bazel/**"
  - "make/bazel-migration.mk"
  - "MODULE.bazel.lock"
  - ".github/workflows/bazel-ci.yml"
  - ".github/workflows/bazel-benchmark.yml"
  - ".github/workflows/bazel-nightly.yml"
  - ".github/workflows/bazel-promote.yml"
  - ".github/workflows/bazel-promoted-consumer-proof.yml"
  - ".github/workflows/bazel-gc.yml"
  - ".github/workflows/ios-build.yml"
  - ".github/workflows/ios-share.yml"
  - ".github/workflows/testflight-beta.yml"
  - "docs/objects/task/doing/implement-shared-bazel-cache-and-buildset-reuse.md"
  - "docs/log.md"
  - "OrlixKernel/Sources/ports/orlix/kbuild/source_state.py"
  - "Orlix/BUILD.bazel"
  - "Orlix/app_modules.bzl"
  - "Orlix/Orlix/**/*.swift"
  - "Orlix/OrlixShared/*.swift"
  - "project.yml"
  - "bazel/product/test_app_modules.py"
read_only_paths:
  - "artifacts.lock.json"
forbidden_paths:
  - "Orlix/Vendor/**"
required_skills:
  - "orlix-bazel"
  - "orlix-implementation-boundaries"
required_role: "orlix-implementer"
required_proof:
  - "foreign producers with complete inputs are disk-cacheable"
  - "cache policy agrees with cache equivalence"
build_intents:
  - "cache foreign producers whose tools and environment are inputs"
verification_intents:
  - "python unittest cache policy and action contracts"
---

# Implement Shared Bazel Cache And Buildset Reuse

Apply the accepted storage split to every supported Apple build. Share only content-addressed or dependency-checked data across parallel worktrees: the bounded Bazel disk cache, repository download cache, and compiler-object cache. Keep each worktree's `ORLIX_BUILD_ROOT`, Bazel output base and server, execution root, Kbuild output, DerivedData, generated Xcode project, and proof outputs isolated.

Use Bazel action-result caching for speed with identities that include every effective input that can change the result. Include destination, profile, compilation mode, or SDK identity only when that dimension changes output. A cache miss, outage, deletion, or corruption must leave the source build correct. Promotion disables action-result reuse, ccache, persistent Kbuild output, and DerivedData, but may use digest-verified source downloads and verified tool installations.

Use GHCR OCI artifacts for private Apple-native components and Linux guest/distribution components, plus one signed compatible buildset. `artifacts.lock.json` selects the buildset and immutable component digests. Normal app-only, Xcode Cloud, TestFlight, and release-candidate builds consume that buildset in promoted mode through OrlixKit. Source mode remains required for component changes, promotion, nightly reconstruction, and toolchain changes. Immutable GitHub Releases hold the public `OrlixKit.xcframework` and official release evidence.

Acceptance requires cache-on and cache-off output equivalence for reproducible outputs, rejection of wrong toolchain, destination, profile, or dependency identities, no release authorization from a cache hit alone, and successful reuse of compatible private artifacts without cross-worktree mutation or duplicate component builds.

The shared promoted-artifact store is local-first and digest-addressed. A warm hit for all locked artifacts performs zero downloads. Locked buildsets and active consumers are pinned against garbage collection. OrlixDistribution artifacts remain guest resources and are not hidden as Apple-native link dependencies.

BuildBuddy Cloud is the shared Bazel AC/CAS at `grpcs://remote.buildbuddy.io`. The compatible cache instance is `orlix/apple/bazel-9.2.0/xcode-17F113/v1`; `v1` is the tracked `ORLIX_BAZEL_CACHE_EPOCH`. The instance excludes commit, branch, pull-request, and simulator-runtime identity. Remote execution stays disabled. Cache-enabled builds use compression, content-defined chunking, minimal downloads by default, the local disk cache, and BuildBuddy BES. That same `build:buildbuddy` configuration records `bazel-remote-grpc.log`, `execution_log.binpb.zst`, and a full timing profile with target labels and primary outputs. The canonical simulator-product operation requests top-level outputs only when it needs the app locally. Invocations that must keep a JSON execution log, including the cache-equivalence gate, clear the compact log for that invocation only. Later Bazel commands, including the cquery after the Apple CI product build, also clear it so they do not replace `execution_log.binpb.zst` before that log is projected to JSON. An empty spawn projection is rejected.

`ORLIX_BUILDBUDDY_CACHE_MODE` accepts only `normal`, `conserve`, or `off`, and defaults to `normal`. In `normal`, main uses `ORLIX_CI_BUILDBUDDY_WRITE_API_KEY`, same-repository pull requests use `ORLIX_CI_BUILDBUDDY_READ_API_KEY` with disabled uploads, and forks stay off. In `conserve`, only main reads and writes. In `off`, all contexts stay off. Promotion, nightly independent reconstruction, TestFlight, and release do not use BuildBuddy action results. Local use is explicit through the untracked `.bazelrc.local`.

The operating ceiling is 80 GB of the 100 GB monthly transfer allowance. From 0 to less than 60 GB, keep `normal`. From 60 to less than 75 GB, keep `normal` and inspect actions with high transfer. From 75 to less than 80 GB, use `conserve`. At 80 GB or more, use `off` until the billing period resets. The 20 GB reserve is not an operating target. Optimize for CI time avoided per GB transferred, not cache-hit percentage alone. Publication artifacts stay in GHCR or release storage, never BuildBuddy.

The cache-equivalence gate uses independent seed, cached, and uncached output
bases. Seed and cached inherit BuildBuddy and do not use a private disk cache.
Only a push of `refs/heads/main` configures the write credential and runs
the publishing seed with `--remote_upload_local_results=true`. A pull
request, including a same-repo pull request, stays on the read key. The
write key is not written to disk for that job. The cached side requires
`cacheHit == true` and
`runner == "remote cache hit"` for UAPI, MLibC, and rootfs. A disk hit does
not pass. The uncached side still disables the action cache, disk cache, and
remote accept. Full component-tree comparison covers paths, modes, symlinks,
and file contents. That comparison includes the UAPI directory and
`kbuild-archive.tar`, the mlibc sysroot directory, and the rootfs images
`initramfs.cpio.gz`, `base.ext4`, and `state.ext4` with their manifest and
digest. Host command files and dependency files in `kbuild-archive.tar` record
the Bazel output base. The archive rewrites that prefix, and its real path, to
`/orlix/output-base`. Empty directories stay inside those images. Digest
marker equality alone cannot satisfy this gate. `compare_trees` still decides
equality.
The gate produces verification evidence without publishing artifacts or
changing the signed buildset lock.

Rootfs output representation must retain empty directories across cache
restoration. Bazel's [tree-artifact limitation](https://github.com/bazelbuild/bazel/issues/15901)
cannot justify removing those directories from the full-tree comparison.
Rootfs uses its existing filesystem images as the cacheable product outputs.
The producing action owns temporary assembly trees and validates their
contents in the images. The provider and app consumers share this image boundary.
`mke2fs` uses `nodiscard` and `lazy_itable_init=0`, so a discard that claims
to zero the image cannot skip the inode tables. A `cache-equivalence`
directory id in the copied tree is rewritten to `cache-equivalence.pinned`
before the image is built. The rewrite adds user write permission, replaces
the same-length id, and restores the original mode, including mode 0555 files. The superblock UUID, hash seed, mount count, and
error times stay pinned with `kbytes_written`. jq's `src/config_opts.inc` is written from `config.status --config` before gmake, with the package work root replaced by `.` and the tool bin replaced by `/orlix/tools`, so `JQ_CONFIG` does not record the output base. The make rule has no prerequisites and does not regenerate that file. `CC` and `LDFLAGS` stay absolute for the real compile.

GitHub Actions caches the slow setup that is outside Bazel action results. `prepare-runtime` asks `xcrun simctl list runtimes` and `xcrun simctl runtime list` before it installs anything. A runtime either command already lists is not installed again. iOS 26.5 on the runner image is that skip. iOS 15.5 is not installed when simctl does not list it. A restored download archive is not an installed runtime and is not copied into a protected directory. A permission error while reading a listed bundle is not a missing runtime and does not run `xcodes runtimes install`. After one iOS 15.5 install, main saves one sudo tar as `orlix-ios15-simruntime-bootable-v1` under `~/Library/Caches/Orlix/ios15-simruntime-bootable`. Later jobs restore that tar and extract it before `prepare-runtime`. `prepare-runtime` returns skip when the restored bundle is bootable, so `xcodes runtimes install` does not run. A tree that fails the `private/var/mobile` check is not uploaded. iOS 26.5 stays the runner image runtime and is not copied into the directory simctl scans. A job that restored the bootable archive does not restore or save the iOS 15.5 DMG. Every workflow that selects Xcode skips the download when that Xcode is already selected or `.xcode-version` is already on the runner. Every workflow that runs Homebrew skips `brew bundle` when each declared formula is already installed at its pinned version. The check reads the Homebrew prefix and does not run `brew info` once per formula. A permission error while reading that prefix is not an unsatisfied Brewfile. A missing compiler or build tool still runs `brew bundle`. The pinned LLVM clang is `/opt/homebrew/opt/llvm/bin/clang`. The macOS 26 runner's unlinked `llvm@20` is not that keg. Ordinary Bazel CI does not restore or save Homebrew source bottles, because that cache only keeps formulae built from source and does not skip bottle installs. The xcodes assertion stays `2.1.0`. `~/Library/Caches/Orlix/ccache` is cached with the pinned compiler identity on the product workflow. That directory is a local compiler-object cache, not a BuildBuddy remote action-cache hit. Evidence reads ccache's direct hits, preprocessed hits, misses, called-for-link count, and cache size from the executed `CCACHE_DIR`. A directory at that path is not a hit. Those ccache counters stay labeled apart from Bazel disk-cache hits and BuildBuddy remote-cache hits. Guest packages inherit `CCACHE_DIR` from the client, so that directory is not a fixed action environment. The package statistics log is not an action input. The CI summary reports the Coreutils action's direct hits, preprocessed hits, and misses. A zeroed shared-directory counter is not that result. One bounded proof deletes the object from a real compilation unit, keeps `CCACHE_DIR`, rebuilds, and requires a ccache hit with the same object bytes. `object_differs` is true only when the bytes differ. A same-runner hit after that delete is not a fresh-runner pass. The Bazel output base stays out of `actions/cache`. The kernel archive executes staged `tools/bin` binaries and their declared dylibs, so that action is remote-cacheable and keeps `no-remote-exec`. ISA restore executes staged gmake, awk, and Python, declares the libraries and Python home they open, and hashes those files in `resume()`, so that action is remote-cacheable and keeps `no-remote-exec`.

`OrlixCompilerRuntime` is its own rule. The mlibc sysroot depends on that archive and does not compile the builtin sources. The recipe executes the staged clang, llvm-ar, mkdir, rm, and sort, and it does not call Homebrew or `xcrun`, so the action stays remotely cacheable. `ORLIX_BAZEL_AUTHORITY` stays `?= 0`.

The Orlix application compiles as separate Swift modules that follow the source dependency graph: shared models, SSH and remote transport, files and SFTP, statistics and telemetry, settings, terminal presentation, app composition, and a one-file app entry. Cross-module declarations use `package` access under the shared package name `Orlix`. Access control and concurrency checking stay in place. `OrlixFiles`, `OrlixModels`, `OrlixSSH`, `OrlixSettings`, and `OrlixStats` do not depend on `OrlixTerminal`, so a terminal presentation edit does not recompile those features. The pre-cutover Xcode target remains one module and passes `-package-name Orlix`. `ORLIX_BAZEL_AUTHORITY` stays `?= 0`. GhosttyKit stays the pinned XCFramework.
