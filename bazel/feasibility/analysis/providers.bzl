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
    asserts.true(env, uapi.artifact_identity_digest != None)
    asserts.true(env, uapi.artifact_identity_manifest != None)
    return analysistest.end(env)

uapi_providers_test = analysistest.make(_uapi_providers_test_impl)

def _sysroot_uapi_only_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    asserts.true(env, OrlixLibcSysrootInfo in target)
    asserts.false(env, OrlixLinuxArchiveInfo in target)
    asserts.false(env, OrlixKernelAppleProductInfo in target)
    sysroot = target[OrlixLibcSysrootInfo]
    asserts.true(env, sysroot.artifact_identity_digest != None)
    asserts.true(env, sysroot.artifact_identity_manifest != None)
    asserts.true(env, sysroot.compiler_runtime_identity_digest != None)
    asserts.true(env, sysroot.compiler_runtime_identity_manifest != None)
    asserts.true(env, sysroot.sysroot_digest != None)
    asserts.true(env, sysroot.consumed_uapi_digest != None)
    found = False
    runtime_found = False
    script = " ".join([action.content for action in target.actions if action.mnemonic == "FileWrite" and any([f.basename == "sysroot.sh" for f in action.outputs.to_list()])])
    for action in target.actions:
        if action.mnemonic == "OrlixCompilerRuntime":
            runtime_found = True
            joined = " ".join([f.path for f in action.inputs.to_list()])
            asserts.true(env, sysroot.compiler_runtime in action.outputs.to_list())
            asserts.true(env, "compiler-runtime-identity.json" in joined)
            asserts.false(env, "mlibc" in joined)
            asserts.false(env, "/uapi/" in joined)
        if action.mnemonic == "OrlixMLibCSysroot":
            found = True
            joined = " ".join([f.path for f in action.inputs.to_list()])
            argv = " ".join(action.argv)
            asserts.false(env, "kbuild-archive.tar" in joined)
            asserts.true(env, sysroot.compiler_runtime in action.inputs.to_list())
            asserts.false(env, "compiler-rt/lib/builtins" in joined)
            asserts.false(env, "compiler-runtime-identity.json" in joined)
            asserts.true(env, "MESON_PACKAGE_CACHE_DIR" in script)
            asserts.false(env, "-isystem" in script)
            asserts.true(env, "--force-fallback-for=freestnd-c-hdrs-aarch64,freestnd-cxx-hdrs-aarch64,frigg,libsmarter" in script)
            asserts.true(env, "-include" in script)
            asserts.true(env, "-ffixed-x18" in script)
            asserts.false(env, "OrlixMLibC/Makefile" in joined)
            asserts.false(env, "OrlixKernel/Makefile" in argv)
            asserts.false(env, "OrlixMLibC/Makefile" in argv)
            asserts.true(env, "OrlixMLibC/Sources/patches" in joined)
            asserts.true(env, "0022-options-ansi-reset-line-buffer-after-flush.patch" in joined)
            asserts.true(env, "0023-options-internal-accept-required-shortopt-dash-arg.patch" in joined)
            asserts.true(env, "0024-options-ansi-fclose-preserve-flush-errno.patch" in joined)
            asserts.true(env, "0025-subprojects-frigg-hex-alt-prefix.patch" in joined)
    asserts.true(env, found)
    asserts.true(env, runtime_found)
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
    archive = target[OrlixLinuxArchiveInfo]
    asserts.equals(env, ctx.attr.expected_profile, archive.profile)
    asserts.equals(env, ctx.attr.expected_destination, archive.destination)
    asserts.true(env, archive.product != None)
    asserts.true(env, archive.artifact_identity_manifest != None)
    asserts.true(env, archive.artifact_identity_digest != None)
    asserts.true(env, archive.boot_resources != None)
    found = False
    identity_found = False
    script = "\n".join([action.content for action in target.actions if action.content != None])
    for action in target.actions:
        if action.mnemonic == "OrlixKernelMachOArchive":
            found = True
            asserts.equals(env, archive.profile, action.env["PROFILE"])
            asserts.equals(env, archive.destination, action.env["ORLIX_KERNEL_ARCHIVE_PLATFORMS"])
            argv = " ".join(action.argv)
            inputs = " ".join([f.path for f in action.inputs.to_list()])
            asserts.false(env, "OrlixKernel/Makefile" in argv)
            asserts.true(env, "source_state.run_locked" in argv)
            asserts.true(env, "__kernel-archive" in script)
            asserts.true(env, "kernel-rules.mk" in script)
            asserts.false(env, "OrlixKernel/Makefile" in script)
            asserts.false(env, "ORLIX_TCTI_ISA_PREPARED" in action.env)
            asserts.false(env, "../../../../OrlixKernel/orlix-tcti-isa" in argv)
            asserts.true(env, "tcti_isa" in inputs)
            asserts.false(env, "prepared-tables.tar.gz" in inputs)
            asserts.false(env, "prepared-tables.sha256" in inputs)
            output_paths = [f.path for f in action.outputs.to_list()]
            asserts.true(env, any([f.path == archive.product.path for f in action.outputs.to_list()]))
            asserts.true(env, any([p.endswith("/arch/orlix/boot/dts/release.dtb") for p in output_paths]))
            asserts.true(env, any([p.endswith("/arch/orlix/boot/dts/development.dtb") for p in output_paths]))
        if action.mnemonic == "OrlixArtifactIdentityV2":
            identity_found = True
            argv = " ".join(action.argv)
            inputs = " ".join([f.path for f in action.inputs.to_list()])
            asserts.true(env, "OrlixKernel.a" in argv)
            asserts.true(env, "arch/orlix/boot/dts/release.dtb" in argv)
            asserts.true(env, "arch/orlix/boot/dts/development.dtb" in argv)
            asserts.true(env, archive.archive.path in inputs)
    asserts.true(env, found)
    asserts.true(env, identity_found)
    return analysistest.end(env)

macho_no_wrapper_makefile_test = analysistest.make(
    _macho_no_wrapper_makefile_test_impl,
    attrs = {
        "expected_profile": attr.string(default = "release"),
        "expected_destination": attr.string(default = "iphonesimulator"),
    },
)

macho_device_development_test = analysistest.make(
    _macho_no_wrapper_makefile_test_impl,
    attrs = {
        "expected_profile": attr.string(default = "development"),
        "expected_destination": attr.string(default = "iphoneos"),
    },
    config_settings = {
        str(Label("//bazel/config:profile")): "development",
        "//command_line_option:platforms": str(Label("@build_bazel_apple_support//platforms:ios_arm64")),
    },
)

def _package_tree_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    asserts.true(env, OrlixPackageTreeInfo in target)
    asserts.false(env, OrlixLinuxArchiveInfo in target)
    asserts.false(env, OrlixKernelAppleProductInfo in target)
    package = target[OrlixPackageTreeInfo]
    asserts.true(env, package.artifact_identity_closure != None)
    asserts.true(env, package.artifact_identity_digest != None)
    asserts.true(env, package.artifact_identity_manifest != None)
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
    package = target[OrlixPackageTreeInfo]
    asserts.true(env, package.artifact_identity_closure != None)
    asserts.true(env, package.artifact_identity_digest != None)
    asserts.true(env, package.artifact_identity_manifest != None)
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
    package = target[OrlixPackageTreeInfo]
    asserts.true(env, package.artifact_identity_closure != None)
    asserts.true(env, package.artifact_identity_digest != None)
    asserts.true(env, package.artifact_identity_manifest != None)
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
            asserts.false(env, "uapi.sha256" in joined)
            asserts.true(env, "bash-identity.json" in joined)
    asserts.true(env, found)
    return analysistest.end(env)

bash_package_tree_test = analysistest.make(_bash_package_tree_test_impl)

def _guest_package_tree_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    asserts.true(env, OrlixPackageTreeInfo in target)
    asserts.false(env, OrlixLinuxArchiveInfo in target)
    asserts.false(env, OrlixKernelAppleProductInfo in target)
    package = target[OrlixPackageTreeInfo]
    asserts.true(env, package.artifact_identity_closure != None)
    asserts.true(env, package.artifact_identity_digest != None)
    asserts.true(env, package.artifact_identity_manifest != None)
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
                asserts.false(env, "uapi.sha256" in joined)
                asserts.false(env, "source-input.sha256" in joined)
                identity = "autotools-bootstrap-identity.json" if ctx.attr.expected_token == "musl-fts" else "autotools-identity.json"
                asserts.true(env, identity in joined)
                if ctx.attr.expected_token == "zsh":
                    asserts.true(env, "zsh.dependency" in joined)
                    asserts.false(env, "ncurses/install" in joined)
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
    rootfs = target[OrlixRootfsInfo]
    asserts.true(env, rootfs.artifact_identity_digest != None)
    asserts.true(env, rootfs.artifact_identity_manifest != None)
    asserts.true(env, rootfs.package_closure != None)
    found = False
    for action in target.actions:
        if action.mnemonic == "OrlixRootfs":
            found = True
            asserts.false(env, any([output.is_directory for output in action.outputs.to_list()]))
            joined = " ".join([f.path for f in action.inputs.to_list()])
            asserts.false(env, "OrlixOS/Sources/make" in joined)
            asserts.false(env, "OrlixKernel/Makefile" in joined)
            asserts.false(env, "OrlixMLibC/Makefile" in joined)
            asserts.false(env, "kbuild-archive.tar" in joined)
            asserts.false(env, "gen_init_cpio.c" in joined)
            asserts.true(env, "gen_init_cpio" in joined)
            asserts.false(env, "file-manifest.txt" in joined)
            asserts.false(env, "source-input.sha256" in joined)
            asserts.false(env, "uapi.sha256" in joined)
            asserts.true(env, "bash" in joined)
            asserts.true(env, "coreutils" in joined)
            asserts.true(env, "grep" in joined)
            asserts.true(env, "findutils" in joined)
            asserts.true(env, "e2fsprogs" in joined)
            asserts.true(env, "jq" in joined)
            asserts.true(env, "curl" in joined)
            asserts.true(env, "zsh" in joined)
            asserts.true(env, "attr" in joined)
            asserts.true(env, "acl" in joined)
            asserts.true(env, "libcap" in joined)
            asserts.true(env, "libselinux" in joined)
            asserts.true(env, "getconf" in joined)
            asserts.true(env, "getent" in joined)
            asserts.true(env, "init" in joined)
            command = " ".join(action.argv)
            for tool in [
                "getfattr",
                "setfattr",
                "getfacl",
                "setfacl",
                "getcap",
                "setcap",
                "getenforce",
                "setenforce",
                "selinuxenabled",
                "policyvers",
                "getpolicyload",
            ]:
                asserts.true(env, tool in command)
    asserts.true(env, found)
    return analysistest.end(env)

rootfs_info_test = analysistest.make(_rootfs_info_test_impl)

def _kernel_composition_test_impl(ctx):
    env = analysistest.begin(ctx)
    target = analysistest.target_under_test(env)
    asserts.true(env, OrlixKernelAppleProductInfo in target)
    found = False
    for action in target.actions:
        if action.mnemonic == "OrlixKernelComposition":
            found = True
            joined = " ".join([f.path for f in action.inputs.to_list()])
            argv = " ".join(action.argv)
            asserts.true(env, "OrlixHostAdapter" in joined)
            asserts.true(env, "OrlixKernel" in joined)
            asserts.false(env, "OrlixKernel/Makefile" in argv)
            asserts.false(env, "OrlixMLibC/Makefile" in joined)
    asserts.true(env, found)
    return analysistest.end(env)

kernel_composition_test = analysistest.make(_kernel_composition_test_impl)
