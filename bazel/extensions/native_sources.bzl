"""Pinned native source repositories for the Bazel Apple feasibility gate."""

def _archive_repository_impl(ctx):
    ctx.download_and_extract(
        url = ctx.attr.url,
        sha256 = ctx.attr.sha256,
        strip_prefix = ctx.attr.strip_prefix,
    )
    export_patterns = [ctx.attr.marker] + list(ctx.attr.extra_exports)
    quoted = ", ".join(['"%s"' % pattern for pattern in export_patterns])
    ctx.file("BUILD.bazel", """
package(default_visibility = ["//visibility:public"])

filegroup(
    name = "source",
    srcs = glob(["**"], exclude = ["BUILD.bazel"]),
)

exports_files(glob([%s]))
""" % quoted)

_archive_repository = repository_rule(
    implementation = _archive_repository_impl,
    attrs = {
        "url": attr.string(mandatory = True),
        "sha256": attr.string(mandatory = True),
        "strip_prefix": attr.string(mandatory = True),
        "marker": attr.string(mandatory = True),
        "extra_exports": attr.string_list(),
    },
)

def _ghostty_kit_impl(ctx):
    pin = json.decode(ctx.read(ctx.attr.pin))
    url = pin.get("url")
    digest = pin.get("sha256")
    if not url or not digest or len(digest) != 64:
        fail("ghostty_kit.json must pin url and a 64-hex sha256")
    ctx.download_and_extract(
        url = url,
        sha256 = digest,
        type = "zip",
    )
    ctx.file("BUILD.bazel", """
load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

exports_files(glob(["GhosttyKit.xcframework/**"]))

cc_library(
    name = "capi_ios_simulator",
    hdrs = ["GhosttyKit.xcframework/ios-arm64_x86_64-simulator/Headers/libghostty/ghostty.h"],
    includes = ["GhosttyKit.xcframework/ios-arm64_x86_64-simulator/Headers/libghostty"],
)

cc_library(
    name = "capi_ios_device",
    hdrs = ["GhosttyKit.xcframework/ios-arm64/Headers/libghostty/ghostty.h"],
    includes = ["GhosttyKit.xcframework/ios-arm64/Headers/libghostty"],
)


filegroup(
    name = "ios_device_lib",
    srcs = ["GhosttyKit.xcframework/ios-arm64/libghostty.a"],
)

filegroup(
    name = "ios_simulator_lib",
    srcs = ["GhosttyKit.xcframework/ios-arm64_x86_64-simulator/libghostty.a"],
)

filegroup(
    name = "macos_lib",
    srcs = ["GhosttyKit.xcframework/macos-arm64_x86_64/libghostty.a"],
)

filegroup(
    name = "ios_device_resources",
    srcs = glob(["GhosttyKit.xcframework/ios-arm64/Headers/**"]),
)

filegroup(
    name = "ios_simulator_resources",
    srcs = glob(["GhosttyKit.xcframework/ios-arm64_x86_64-simulator/Headers/**"]),
)

filegroup(
    name = "macos_resources",
    srcs = glob(["GhosttyKit.xcframework/macos-arm64_x86_64/Headers/**"]),
)

filegroup(
    name = "ios_simulator_ghostty_h",
    srcs = ["GhosttyKit.xcframework/ios-arm64_x86_64-simulator/Headers/libghostty/ghostty.h"],
)

filegroup(
    name = "ios_device_ghostty_h",
    srcs = ["GhosttyKit.xcframework/ios-arm64/Headers/libghostty/ghostty.h"],
)
""")

_ghostty_kit_repository = repository_rule(
    implementation = _ghostty_kit_impl,
    attrs = {
        "pin": attr.label(allow_single_file = True, mandatory = True),
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

def _kernel_toolchain_repository_impl(ctx):
    developer = ctx.getenv("DEVELOPER_DIR", "/Applications/Xcode-26.6.0.app/Contents/Developer")
    observer = ctx.path(ctx.attr.observer)
    result = ctx.execute([
        "/usr/bin/python3", "-B", "-c",
        "import sys,json; sys.path.insert(0,sys.argv[1]); import toolchain_pin; print(json.dumps(toolchain_pin.capture_kernel_manifest(sys.argv[2],sys.argv[3])))",
        str(observer.dirname), developer, str(ctx.path("identity.json")),
    ], timeout = 300)
    if result.return_code:
        fail("Kernel toolchain identity failed:\n%s" % result.stderr)
    watched = json.decode(result.stdout)
    ctx.watch(observer)
    for path in watched["files"]:
        ctx.watch(path)
    for path in watched["trees"]:
        ctx.watch_tree(path)
    ctx.file("BUILD.bazel", 'exports_files(["identity.json", "compiler-identity.json", "compiler-runtime-identity.json", "mlibc-identity.json", "guest-compiler-identity.json", "coreutils-identity.json", "bash-identity.json", "autotools-identity.json"], visibility = ["//visibility:public"])\n')

_kernel_toolchain_repository = repository_rule(
    implementation = _kernel_toolchain_repository_impl,
    attrs = {"observer": attr.label(default = "//bazel/config:toolchain_pin.py")},
    local = True,
    configure = True,
)

def _native_sources_impl(_ctx):
    _kernel_toolchain_repository(name = "orlix_kernel_toolchain")
    _ghostty_kit_repository(
        name = "orlix_ghostty_kit",
        pin = "//bazel/extensions:ghostty_kit.json",
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
        extra_exports = ["usr/gen_init_cpio.c"],
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
    # GNU Coreutils 9.11 Autotools tarball (configure + bundled gnulib).
    # Git identity matches OrlixCoreUtils COREUTILS_GIT_COMMIT / v9.11:
    # c01fd163a47468a8296fb369f5233853bb551bb6
    # Product git bootstrap uses https://github.com/coreutils/gnulib.git;
    # gnulib is already applied in this release tarball.
    _archive_repository(
        name = "orlix_coreutils_source",
        url = "https://ftp.gnu.org/gnu/coreutils/coreutils-9.11.tar.xz",
        sha256 = "394024eda0a5955217ceda9cd1201e65dc8fa3aa29c2951135a49521d57c3cc3",
        strip_prefix = "coreutils-9.11",
        marker = "configure",
    )
    # GNU Bash 5.3 Autotools tarball. Pins match OrlixOS BASH_URL / BASH_SHA256.
    _archive_repository(
        name = "orlix_bash_source",
        url = "https://ftp.gnu.org/gnu/bash/bash-5.3.tar.gz",
        sha256 = "0d5cd86965f869a26cf64f4b71be7b96f90a3ba8b3d74e27e8e9d9d5550f31ba",
        strip_prefix = "bash-5.3",
        marker = "configure",
    )
    _archive_repository(
        name = "orlix_grep_source",
        url = "https://ftp.gnu.org/gnu/grep/grep-3.12.tar.xz",
        sha256 = "2649b27c0e90e632eadcd757be06c6e9a4f48d941de51e7c0f83ff76408a07b9",
        strip_prefix = "grep-3.12",
        marker = "configure",
    )
    _archive_repository(
        name = "orlix_findutils_source",
        url = "https://ftp.gnu.org/gnu/findutils/findutils-4.10.0.tar.xz",
        sha256 = "1387e0b67ff247d2abde998f90dfbf70c1491391a59ddfecb8ae698789f0a4f5",
        strip_prefix = "findutils-4.10.0",
        marker = "configure",
    )
    _archive_repository(
        name = "orlix_e2fsprogs_source",
        url = "https://www.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v1.47.1/e2fsprogs-1.47.1.tar.xz",
        sha256 = "5a33dc047fd47284bca4bb10c13cfe7896377ae3d01cb81a05d406025d99e0d1",
        strip_prefix = "e2fsprogs-1.47.1",
        marker = "configure",
    )
    _archive_repository(
        name = "orlix_jq_source",
        url = "https://github.com/jqlang/jq/releases/download/jq-1.7.1/jq-1.7.1.tar.gz",
        sha256 = "478c9ca129fd2e3443fe27314b455e211e0d8c60bc8ff7df703873deeee580c2",
        strip_prefix = "jq-1.7.1",
        marker = "configure",
    )
    _archive_repository(
        name = "orlix_curl_source",
        url = "https://curl.se/download/curl-8.20.0.tar.xz",
        sha256 = "63fe2dc148ba0ceae89922ef838f7e5c946272c2e78b7c59fab4b79d3ce2b896",
        strip_prefix = "curl-8.20.0",
        marker = "configure",
    )
    _archive_repository(
        name = "orlix_ncurses_source",
        url = "https://ftp.gnu.org/gnu/ncurses/ncurses-6.6.tar.gz",
        sha256 = "355b4cbbed880b0381a04c46617b7656e362585d52e9cf84a67e2009b749ff11",
        strip_prefix = "ncurses-6.6",
        marker = "configure",
    )
    # Official zsh 5.9 tarball now lives under pub/old (pub/ returns 404).
    _archive_repository(
        name = "orlix_zsh_source",
        url = "https://www.zsh.org/pub/old/zsh-5.9.tar.xz",
        sha256 = "9b8d1ecedd5b5e81fbf1918e876752a7dd948e05c1a0dba10ab863842d45acd5",
        strip_prefix = "zsh-5.9",
        marker = "configure",
    )

native_sources = module_extension(implementation = _native_sources_impl)
