"""Pinned native source repositories for the Bazel Apple feasibility gate."""

def _archive_repository_impl(ctx):
    ctx.download_and_extract(
        url = ctx.attr.url,
        sha256 = ctx.attr.sha256,
        strip_prefix = ctx.attr.strip_prefix,
    )
    ctx.file("BUILD.bazel", """
package(default_visibility = ["//visibility:public"])

filegroup(
    name = "source",
    srcs = glob(["**"], exclude = ["BUILD.bazel"]),
)

exports_files(["%s"])
""" % ctx.attr.marker)

_archive_repository = repository_rule(
    implementation = _archive_repository_impl,
    attrs = {
        "url": attr.string(mandatory = True),
        "sha256": attr.string(mandatory = True),
        "strip_prefix": attr.string(mandatory = True),
        "marker": attr.string(mandatory = True),
    },
)

def _ghostty_repository_impl(ctx):
    ctx.download_and_extract(
        url = ctx.attr.url,
        sha256 = ctx.attr.sha256,
        strip_prefix = ctx.attr.strip_prefix,
        output = "source",
    )
    patch_digest = ctx.execute([
        "/usr/bin/shasum",
        "-a",
        "256",
        ctx.path(ctx.attr.patch),
    ])
    if patch_digest.return_code:
        fail("Ghostty patch digest failed:\n%s\n%s" % (patch_digest.stdout, patch_digest.stderr))
    if patch_digest.stdout.split(" ")[0] != ctx.attr.patch_sha256:
        fail("Ghostty patch digest does not match patch_sha256")
    patch_result = ctx.execute([
        "/usr/bin/patch",
        "--batch",
        "--forward",
        "-p1",
        "-i",
        ctx.path(ctx.attr.patch),
    ])
    if patch_result.return_code:
        fail("Ghostty patch failed:\n%s\n%s" % (patch_result.stdout, patch_result.stderr))
    copy_result = ctx.execute([
        "/bin/cp",
        "-R",
        ctx.path("source"),
        ctx.path("fetch-source"),
    ])
    if copy_result.return_code:
        fail("Ghostty fetch copy failed:\n%s\n%s" % (copy_result.stdout, copy_result.stderr))
    ctx.download_and_extract(
        url = ctx.attr.zig_url,
        sha256 = ctx.attr.zig_sha256,
        strip_prefix = ctx.attr.zig_strip_prefix,
        output = "zig",
    )

    packages = ctx.path("zig-pkg")
    fetch_local = ctx.path("zig-fetch-local")
    developer_dir = ctx.os.environ.get("DEVELOPER_DIR")
    if not developer_dir:
        fail("Ghostty fetch requires repo_env DEVELOPER_DIR")
    xcode_ver = ctx.execute(
        ["/usr/bin/xcodebuild", "-version"],
        environment = {
            "DEVELOPER_DIR": developer_dir,
            "PATH": "/usr/bin:/bin",
        },
    )
    if xcode_ver.return_code:
        fail("Ghostty fetch xcodebuild -version failed:\n%s\n%s" % (xcode_ver.stdout, xcode_ver.stderr))
    xcode_lines = [line for line in xcode_ver.stdout.strip().split("\n") if line]
    allowed_xcode = {
        "Xcode 26.6": "Build version 17F113",
        "Xcode 27.0": "Build version 27A5252f",
    }
    if (
        len(xcode_lines) < 2
        or xcode_lines[0] not in allowed_xcode
        or xcode_lines[1] != allowed_xcode[xcode_lines[0]]
    ):
        fail("Ghostty fetch requires Xcode 26.6/17F113 or 27.0/27A5252f, got:\n%s" % xcode_ver.stdout)
    result = ctx.execute(
        [
            ctx.path("zig/zig"),
            "build",
            "--fetch=all",
            "--global-cache-dir",
            str(packages),
            "--cache-dir",
            str(fetch_local),
            "-Dapp-runtime=none",
            "-Demit-xcframework=true",
            "-Demit-macos-app=false",
            "-Demit-exe=false",
            "-Demit-docs=false",
            "-Demit-webdata=false",
            "-Demit-helpgen=false",
            "-Demit-terminfo=false",
            "-Demit-termcap=false",
            "-Demit-themes=false",
            "-Di18n=false",
            "-Doptimize=ReleaseFast",
            "-Dstrip",
            "-Dxcframework-target=universal",
        ],
        working_directory = str(ctx.path("fetch-source")),
        environment = {
            "DEVELOPER_DIR": developer_dir,
            "HOME": str(ctx.path("repository-home")),
            "PATH": "/usr/bin:/bin",
            "ZIG_GLOBAL_CACHE_DIR": str(packages),
            "ZIG_LOCAL_CACHE_DIR": str(fetch_local),
        },
        timeout = 900,
    )
    if result.return_code:
        fail("Ghostty dependency fetch failed:\n%s\n%s" % (result.stdout, result.stderr))
    if not ctx.path("zig-pkg/p").exists:
        fail("Ghostty dependency fetch produced no packages")
    ctx.delete("fetch-source")
    ctx.delete("zig-fetch-local")
    ctx.delete("repository-home")
    ctx.file("zig-pkg/orlix-ready", "Ghostty packages fetched by digest\n")

    ctx.file("BUILD.bazel", """
package(default_visibility = ["//visibility:public"])

exports_files([
    "source/build.zig",
    "source/include/ghostty.h",
    "zig/zig",
    "zig-pkg/orlix-ready",
])

filegroup(
    name = "source",
    srcs = glob(["source/**"], exclude = [
        "source/CLAUDE.md",
        "source/zig-pkg/**",
        "source/.zig-cache/**",
    ]),
)

filegroup(
    name = "zig_packages",
    srcs = glob(["zig-pkg/p/**"]),
)
""")

_ghostty_repository = repository_rule(
    implementation = _ghostty_repository_impl,
    attrs = {
        "url": attr.string(mandatory = True),
        "sha256": attr.string(mandatory = True),
        "strip_prefix": attr.string(mandatory = True),
        "patch": attr.label(mandatory = True),
        "patch_sha256": attr.string(mandatory = True),
        "zig_url": attr.string(mandatory = True),
        "zig_sha256": attr.string(mandatory = True),
        "zig_strip_prefix": attr.string(mandatory = True),
    },
)

def _cmake_repository_impl(ctx):
    ctx.download_and_extract(
        url = ctx.attr.url,
        sha256 = ctx.attr.sha256,
        strip_prefix = ctx.attr.strip_prefix,
    )
    ctx.file("BUILD.bazel", """
package(default_visibility = ["//visibility:public"])

exports_files(["CMake.app/Contents/bin/cmake"])

filegroup(
    name = "runtime",
    srcs = glob(["CMake.app/**"]),
)
""")

def _compiler_rt_repository_impl(ctx):
    result = ctx.execute([
        "/usr/bin/git",
        "clone",
        "--depth",
        "1",
        "--branch",
        ctx.attr.ref,
        "--filter=blob:none",
        "--sparse",
        ctx.attr.url,
        ".",
    ])
    if result.return_code:
        fail("compiler-rt clone failed:\n%s\n%s" % (result.stdout, result.stderr))
    sparse = ctx.execute([
        "/usr/bin/git",
        "sparse-checkout",
        "set",
        "compiler-rt/lib/builtins",
    ])
    if sparse.return_code:
        fail("compiler-rt sparse-checkout failed:\n%s\n%s" % (sparse.stdout, sparse.stderr))
    commit = ctx.execute(["/usr/bin/git", "rev-parse", "HEAD"])
    if commit.return_code or commit.stdout.strip() != ctx.attr.commit:
        fail("compiler-rt commit mismatch: expected %s got %s" % (ctx.attr.commit, commit.stdout))
    ctx.file("BUILD.bazel", """
package(default_visibility = ["//visibility:public"])
filegroup(
    name = "source",
    srcs = glob(["compiler-rt/lib/builtins/**"], exclude = ["BUILD.bazel"]),
)
exports_files(["compiler-rt/lib/builtins/addtf3.c"])
""")

_compiler_rt_repository = repository_rule(
    implementation = _compiler_rt_repository_impl,
    attrs = {
        "url": attr.string(mandatory = True),
        "ref": attr.string(mandatory = True),
        "commit": attr.string(mandatory = True),
    },
)

_cmake_repository = repository_rule(
    implementation = _cmake_repository_impl,
    attrs = {
        "url": attr.string(mandatory = True),
        "sha256": attr.string(mandatory = True),
        "strip_prefix": attr.string(mandatory = True),
    },
)

def _native_sources_impl(_ctx):
    _ghostty_repository(
        name = "orlix_ghostty_source",
        url = "https://github.com/wiedymi/ghostty/archive/02af5158c76036291183e746d436eb8f15356662.tar.gz",
        sha256 = "5f42b2a27d15c4387a40b81a7da60f3b1f0328e238225417db80a1f9aaa4b3aa",
        strip_prefix = "ghostty-02af5158c76036291183e746d436eb8f15356662",
        patch = "//third_party/patches/ghostty:orlix_apple.patch",
        patch_sha256 = "3c1225ea01bde23f1ec8585fd85ca3b2e15bae51501cf71fd3d5c4ca4b56a1b5",
        zig_url = "https://ziglang.org/download/0.16.0/zig-aarch64-macos-0.16.0.tar.xz",
        zig_sha256 = "b23d70deaa879b5c2d486ed3316f7eaa53e84acf6fc9cc747de152450d401489",
        zig_strip_prefix = "zig-aarch64-macos-0.16.0",
    )
    _archive_repository(
        name = "orlix_openssl_source",
        url = "https://www.openssl.org/source/openssl-3.2.0.tar.gz",
        sha256 = "14c826f07c7e433706fb5c69fa9e25dab95684844b4c962a2cf1bf183eb4690e",
        strip_prefix = "openssl-3.2.0",
        marker = "Configure",
    )
    _archive_repository(
        name = "orlix_libssh2_source",
        url = "https://www.libssh2.org/download/libssh2-1.11.1.tar.gz",
        sha256 = "d9ec76cbe34db98eec3539fe2c899d26b0c837cb3eb466a56b0f109cabf658f7",
        strip_prefix = "libssh2-1.11.1",
        marker = "CMakeLists.txt",
    )
    _archive_repository(
        name = "orlix_linux_source",
        url = "https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.12.105.tar.xz",
        sha256 = "eb36801e119529b13513c3459dc20e2a32f7053629f3aabb63ea501a4d88f63d",
        strip_prefix = "linux-6.12.105",
        marker = "Makefile",
    )
    _archive_repository(
        name = "orlix_mlibc_source",
        url = "https://github.com/managarm/mlibc/archive/7c2a178142625cc9852e59a1a090468c61a62d3b.tar.gz",
        sha256 = "22535f15a789b463bf19abb45b6d4aded20cf5f0b207eb43922b2f95e4507603",
        strip_prefix = "mlibc-7c2a178142625cc9852e59a1a090468c61a62d3b",
        marker = "meson.build",
    )
    _archive_repository(
        name = "orlix_frigg_source",
        url = "https://github.com/managarm/frigg/archive/b0dbea66bc19f7c5546f0039a3be842feb02678c.tar.gz",
        sha256 = "10cbee1dab6e7b0a1ca8d6d59d9eeffab1ed3cf6b0031551fd63a6861cc07a41",
        strip_prefix = "frigg-b0dbea66bc19f7c5546f0039a3be842feb02678c",
        marker = "meson.build",
    )
    _archive_repository(
        name = "orlix_freestnd_c_source",
        url = "https://github.com/osdev0/freestnd-c-hdrs/archive/d33711241b46ecb8f2ad33927fcefdcb3ac0162e.tar.gz",
        sha256 = "d3f0c1e0720dec9da97175eebdc51ac4b9bdd776b5687b2a1ccd97f6e861498e",
        strip_prefix = "freestanding-c-hdrs-gnu-d33711241b46ecb8f2ad33927fcefdcb3ac0162e",
        marker = "COPYING",
    )
    _archive_repository(
        name = "orlix_freestnd_cxx_source",
        url = "https://github.com/osdev0/freestnd-cxx-hdrs/archive/a6b351e0ab3e74e5789b01fa1447e4cd62373da7.tar.gz",
        sha256 = "dc4a44daef5d50a6f9aea0cb7bd2164c208f159d273c51f3ff8334b5bd65ef95",
        strip_prefix = "freestanding-cxx-hdrs-gnu-a6b351e0ab3e74e5789b01fa1447e4cd62373da7",
        marker = "COPYING",
    )
    _archive_repository(
        name = "orlix_libsmarter_source",
        url = "https://github.com/managarm/libsmarter/archive/f7d061bc37d485418344452c7ceb28d5df3ba85d.tar.gz",
        sha256 = "1426e24f3c3c2a08983fad71d461faa415f0ccc1ca432dd80bebe1dac9cc7bce",
        strip_prefix = "libsmarter-f7d061bc37d485418344452c7ceb28d5df3ba85d",
        marker = "meson.build",
    )
    _compiler_rt_repository(
        name = "orlix_compiler_rt_source",
        url = "https://github.com/llvm/llvm-project.git",
        ref = "llvmorg-22.1.6",
        commit = "fc4aad7b5db3fff421df9a9637605b9ca5667881",
    )
    _archive_repository(
        name = "orlix_bragi_source",
        url = "https://github.com/managarm/bragi/archive/523b86efac124b0d749eed201df0c7ea9f87ee17.tar.gz",
        sha256 = "f1eec00de9234f2040737a6c69638948c1b02bbf044882eff1e35be29d20ff72",
        strip_prefix = "bragi-523b86efac124b0d749eed201df0c7ea9f87ee17",
        marker = "Cargo.toml",
    )
    _cmake_repository(
        name = "orlix_cmake_tool",
        url = "https://github.com/Kitware/CMake/releases/download/v4.0.3/cmake-4.0.3-macos-universal.tar.gz",
        sha256 = "4e85de4daf1c3e82d7dc6b8ba5683972944b466343aeb9c327a742437bb3ce9a",
        strip_prefix = "cmake-4.0.3-macos-universal",
    )

native_sources = module_extension(implementation = _native_sources_impl)
