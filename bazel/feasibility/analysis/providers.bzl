"""Analysis tests for UAPI/sysroot provider direction and config flags."""

load("@bazel_skylib//lib:unittest.bzl", "analysistest", "asserts")
load("//bazel/providers:kernel_info.bzl", "OrlixInstalledUapiInfo", "OrlixKernelAppleProductInfo", "OrlixLinuxArchiveInfo")
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
