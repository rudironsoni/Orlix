"""Analysis tests for UAPI/sysroot provider direction and config flags."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//bazel/providers:kernel_info.bzl", "OrlixInstalledUapiInfo", "OrlixKernelAppleProductInfo", "OrlixLinuxArchiveInfo")
load("//bazel/providers:package_info.bzl", "OrlixPackageTreeInfo")
load("//bazel/providers:rootfs_info.bzl", "OrlixRootfsInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")

def _uapi_providers_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    asserts.true(env, OrlixInstalledUapiInfo in target)
    asserts.true(env, OrlixLinuxArchiveInfo in target)
    asserts.false(env, OrlixKernelAppleProductInfo in target)
    asserts.false(env, OrlixLibcSysrootInfo in target)
    uapi = target[OrlixInstalledUapiInfo]
    asserts.equals(env, "arm64", uapi.arch)
    return analysistest.end(env)

uapi_providers_test = analysistest.make(_uapi_providers_test_impl)

def _sysroot_uapi_only_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    asserts.true(env, OrlixLibcSysrootInfo in target)
    asserts.false(env, OrlixLinuxArchiveInfo in target)
    asserts.false(env, OrlixKernelAppleProductInfo in target)
    sysroot = target[OrlixLibcSysrootInfo]
    asserts.true(env, sysroot.sysroot_digest != None)
    asserts.true(env, sysroot.consumed_uapi_digest != None)
    found = False
    for action in target.actions:
        if action.mnemonic == "OrlixMLibCSysroot":
            found = True
            joined = " ".join([f.path for f in action.inputs.to_list()])
            argv = " ".join(action.argv)
            asserts.false(env, "kbuild-archive.tar" in joined)
            asserts.false(env, "OrlixMLibC/Makefile" in joined)
            asserts.false(env, "OrlixKernel/Makefile" in argv)
            asserts.false(env, "OrlixMLibC/Makefile" in argv)
    asserts.true(env, found)
    return analysistest.end(env)

sysroot_uapi_only_test = analysistest.make(_sysroot_uapi_only_test_impl)

def _config_flags_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    asserts.true(env, target.label.name in ("profile", "component_mode", "apple_destination", "signing_mode"))
    return analysistest.end(env)

config_flags_test = analysistest.make(_config_flags_test_impl)

def _variant_stamp_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    archive = target[OrlixLinuxArchiveInfo]
    asserts.equals(env, ctx.attr.expected_destination, archive.destination)
    asserts.equals(env, ctx.attr.expected_profile, archive.profile)
    asserts.true(env, OrlixInstalledUapiInfo in target)
    return analysistest.end(env)

variant_stamp_test = analysistest.make(
    _variant_stamp_test_impl,
    attrs = {
        "expected_destination": attr.string(),
        "expected_profile": attr.string(),
    },
)

def _no_wrapper_makefile_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    found = False
    for action in target.actions:
        if action.mnemonic == "OrlixLinuxHeadersInstall":
            found = True
            argv = " ".join(action.argv)
            asserts.false(env, "OrlixKernel/" in argv)
            asserts.true(env, "headers_install" in argv)
            asserts.true(env, "orlix_linux_source" in argv)
    asserts.true(env, found)
    return analysistest.end(env)

no_wrapper_makefile_test = analysistest.make(_no_wrapper_makefile_test_impl)

def _macho_no_wrapper_makefile_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    asserts.true(env, OrlixLinuxArchiveInfo in target)
    found = False
    for action in target.actions:
        if action.mnemonic == "OrlixKernelMachOArchive":
            found = True
            argv = " ".join(action.argv)
            asserts.false(env, "OrlixKernel/Makefile" in argv)
            asserts.true(env, "__kernel-archive" in argv)
            asserts.true(env, "kernel-rules.mk" in argv)
    asserts.true(env, found)
    return analysistest.end(env)

macho_no_wrapper_makefile_test = analysistest.make(_macho_no_wrapper_makefile_test_impl)

def _package_tree_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    asserts.true(env, OrlixPackageTreeInfo in target)
    asserts.false(env, OrlixLinuxArchiveInfo in target)
    asserts.false(env, OrlixKernelAppleProductInfo in target)
    found = False
    for action in target.actions:
        if action.mnemonic == "OrlixGuestPackage":
            found = True
            joined = " ".join([f.path for f in action.inputs.to_list()])
            asserts.false(env, "kbuild-archive.tar" in joined)
            asserts.false(env, "OrlixKernel/Makefile" in joined)
            asserts.false(env, "OrlixMLibC/Makefile" in joined)
            asserts.false(env, "OrlixOS/Sources/make" in joined)
            asserts.true(env, "packages/true/configure" in joined)
    asserts.true(env, found)
    return analysistest.end(env)

package_tree_test = analysistest.make(_package_tree_test_impl)

def _coreutils_package_tree_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    asserts.true(env, OrlixPackageTreeInfo in target)
    asserts.false(env, OrlixLinuxArchiveInfo in target)
    asserts.false(env, OrlixKernelAppleProductInfo in target)
    found = False
    for action in target.actions:
        if action.mnemonic == "OrlixGuestPackage":
            found = True
            joined = " ".join([f.path for f in action.inputs.to_list()])
            asserts.false(env, "kbuild-archive.tar" in joined)
            asserts.false(env, "OrlixKernel/Makefile" in joined)
            asserts.false(env, "OrlixMLibC/Makefile" in joined)
            asserts.false(env, "OrlixOS/Sources/make" in joined)
            asserts.false(env, "OrlixCoreUtils/Makefile" in joined)
            asserts.true(env, "configure" in joined)
            asserts.true(env, "coreutils" in joined)
    asserts.true(env, found)
    return analysistest.end(env)

coreutils_package_tree_test = analysistest.make(_coreutils_package_tree_test_impl)

def _bash_package_tree_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    asserts.true(env, OrlixPackageTreeInfo in target)
    asserts.false(env, OrlixLinuxArchiveInfo in target)
    asserts.false(env, OrlixKernelAppleProductInfo in target)
    found = False
    for action in target.actions:
        if action.mnemonic == "OrlixGuestPackage":
            found = True
            joined = " ".join([f.path for f in action.inputs.to_list()])
            asserts.false(env, "kbuild-archive.tar" in joined)
            asserts.false(env, "OrlixKernel/Makefile" in joined)
            asserts.false(env, "OrlixMLibC/Makefile" in joined)
            asserts.false(env, "OrlixOS/Sources/make" in joined)
            asserts.false(env, "OrlixCoreUtils/Makefile" in joined)
            asserts.true(env, "configure" in joined)
            asserts.true(env, "bash" in joined)
    asserts.true(env, found)
    return analysistest.end(env)

bash_package_tree_test = analysistest.make(_bash_package_tree_test_impl)

def _guest_package_tree_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    asserts.true(env, OrlixPackageTreeInfo in target)
    asserts.false(env, OrlixLinuxArchiveInfo in target)
    asserts.false(env, OrlixKernelAppleProductInfo in target)
    found = False
    for action in target.actions:
        if action.mnemonic == "OrlixGuestPackage":
            found = True
            joined = " ".join([f.path for f in action.inputs.to_list()])
            asserts.false(env, "kbuild-archive.tar" in joined)
            asserts.false(env, "OrlixKernel/Makefile" in joined)
            asserts.false(env, "OrlixMLibC/Makefile" in joined)
            asserts.false(env, "OrlixOS/Sources/make" in joined)
            asserts.false(env, "OrlixCoreUtils/Makefile" in joined)
            asserts.true(env, ctx.attr.expected_token in joined)
            if ctx.attr.require_configure:
                asserts.true(env, "configure" in joined)
    asserts.true(env, found)
    return analysistest.end(env)

guest_package_tree_test = analysistest.make(
    _guest_package_tree_test_impl,
    attrs = {
        "expected_token": attr.string(mandatory = True),
        "require_configure": attr.bool(default = True),
    },
)

def _rootfs_info_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    asserts.true(env, OrlixRootfsInfo in target)
    asserts.false(env, OrlixLinuxArchiveInfo in target)
    asserts.false(env, OrlixKernelAppleProductInfo in target)
    found = False
    for action in target.actions:
        if action.mnemonic == "OrlixRootfs":
            found = True
            joined = " ".join([f.path for f in action.inputs.to_list()])
            asserts.false(env, "OrlixOS/Sources/make" in joined)
            asserts.false(env, "OrlixKernel/Makefile" in joined)
            asserts.false(env, "OrlixMLibC/Makefile" in joined)
            asserts.false(env, "kbuild-archive.tar" in joined)
            asserts.true(env, "gen_init_cpio.c" in joined)
            asserts.true(env, "bash" in joined)
            asserts.true(env, "coreutils" in joined)
            asserts.true(env, "grep" in joined)
            asserts.true(env, "findutils" in joined)
            asserts.true(env, "e2fsprogs" in joined)
            asserts.true(env, "jq" in joined)
            asserts.true(env, "curl" in joined)
            asserts.true(env, "zsh" in joined)
            asserts.true(env, "getconf" in joined)
            asserts.true(env, "getent" in joined)
            asserts.true(env, "init" in joined)
    asserts.true(env, found)
    return analysistest.end(env)

rootfs_info_test = analysistest.make(_rootfs_info_test_impl)
