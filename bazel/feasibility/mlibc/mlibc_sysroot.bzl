"""Meson/Ninja OrlixMLibC sysroot from installed Linux UAPI only."""

load("//bazel:artifact_identity.bzl", "declare_artifact_identity")
load("//bazel/providers:kernel_info.bzl", "OrlixInstalledUapiInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")

def _pinned_env(_ctx):
    return {
        "HOME": "/var/empty",
        "PATH": "/usr/bin:/bin",
        "PYTHONDONTWRITEBYTECODE": "1",
        "CCACHE_CONFIGPATH": "/dev/null",
        "CCACHE_COMPILERCHECK": "content",
        "CCACHE_MAXSIZE": "20G",
    }

_COMPILER_RT_SOURCES = [
    "addtf3.c",
    "aarch64/fp_mode.c",
    "comparetf2.c",
    "clzti2.c",
    "divtf3.c",
    "extenddftf2.c",
    "extendhftf2.c",
    "extendsftf2.c",
    "extendxftf2.c",
    "fixtfdi.c",
    "fixtfsi.c",
    "fixtfti.c",
    "fixunstfdi.c",
    "fixunstfsi.c",
    "fixunstfti.c",
    "floatditf.c",
    "floatsitf.c",
    "floattitf.c",
    "floatunditf.c",
    "floatunsitf.c",
    "floatuntitf.c",
    "muldc3.c",
    "mulsc3.c",
    "multc3.c",
    "multf3.c",
    "powitf2.c",
    "subtf3.c",
    "trunctfbf2.c",
    "trunctfdf2.c",
    "trunctfhf2.c",
    "trunctfsf2.c",
    "trunctfxf2.c",
    "udivmodti4.c",
    "udivti3.c",
]

def _compiler_runtime(ctx, runtime):
    builtins = ctx.file.compiler_rt.dirname
    sources = {f.path: f for f in ctx.files.compiler_rt_source}
    selected = [sources[builtins + "/" + name] for name in _COMPILER_RT_SOURCES]
    headers = [f for f in ctx.files.compiler_rt_source if f.extension in ["h", "inc"]]
    ctx.actions.run_shell(
        mnemonic = "OrlixCompilerRuntime",
        progress_message = "Building Linux compiler runtime",
        command = r"""
set -euo pipefail
exec_root="$PWD"
builtins="$exec_root/$1"
runtime_out="$exec_root/$2"
tool_bin="$(/usr/bin/dirname "$exec_root/$3")"
ar_name="$(/usr/bin/basename "$exec_root/$4")"
libdir=""
if [ "$5" != none ]; then libdir="$exec_root/$5"; fi
if [ -n "$libdir" ]; then export DYLD_LIBRARY_PATH="$libdir${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"; fi
tool_root="$(/usr/bin/dirname "$tool_bin")"
export PATH="$tool_bin:/usr/bin:/bin"
shift 5
test -x "$tool_bin/clang"
test -x "$tool_bin/$ar_name"
clang=clang
ar="$ar_name"
work="$exec_root/compiler-rt-objects"
/bin/rm -rf "$work"
/bin/mkdir -p "$work"
trap '/bin/rm -rf "$work"' EXIT
for relative in "$@"; do
    source="$exec_root/$relative"
    source_name="${source#"$builtins/"}"
    object_name="${source_name//\//-}"
    "$clang" --target=aarch64-linux-gnu -ffreestanding -fno-builtin -nostdlibinc -ffixed-x18 -O2 "-ffile-prefix-map=$builtins=/orlix/compiler-rt" "-ffile-prefix-map=$work=/orlix/compiler-rt-obj" "-ffile-prefix-map=$tool_root=/orlix/tools" -I"$builtins" -c "$source" -o "$work/${object_name%.c}.o"
done
(
  cd "$work"
  members=()
  for object in *.o; do members+=("$object"); done
  "$ar" rcs "$runtime_out" "${members[@]}"
)
test -s "$runtime_out"
""",
        arguments = [
            builtins,
            runtime.path,
            ctx.file.clang.path,
            ctx.file.llvm_ar.path,
            ctx.files.tool_libs[0].dirname if ctx.files.tool_libs else "none",
        ] + [f.path for f in selected],
        inputs = depset(selected + headers + [
            ctx.file.runtime_toolchain_identity,
            ctx.file.compiler_identity,
            ctx.file.clang,
            ctx.file.llvm_ar,
        ] + ctx.files.tool_libs + ctx.files.clang_resources),
        outputs = [runtime],
        env = _pinned_env(ctx),
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )
    return declare_artifact_identity(
        ctx,
        "compiler-runtime",
        ctx.file._artifact_identity_serializer,
        artifacts = {"libcompiler_rt.a": runtime},
    )

def _mlibc_sysroot_impl(ctx):
    uapi = ctx.attr.uapi[OrlixInstalledUapiInfo]
    sysroot = ctx.actions.declare_directory(ctx.label.name + "/sysroot")
    headers = ctx.actions.declare_directory(ctx.label.name + "/headers")
    libraries = ctx.actions.declare_directory(ctx.label.name + "/libraries")
    manifest = ctx.actions.declare_file(ctx.label.name + "/manifest.json")
    abi = ctx.actions.declare_file(ctx.label.name + "/abi.txt")
    loader = ctx.actions.declare_file(ctx.label.name + "/ld.so")
    runtime = ctx.actions.declare_file(ctx.label.name + "/libcompiler_rt.a")
    digest = ctx.actions.declare_file(ctx.label.name + "/sysroot.sha256")
    product = ctx.actions.declare_directory(ctx.label.name + "/product")
    runtime_identity = _compiler_runtime(ctx, runtime)
    script = ctx.actions.declare_file(ctx.label.name + ".sh")
    ctx.actions.write(script, r"""
set -euo pipefail
exec_root="$PWD"
mlibc_meson="$exec_root/$1"
uapi_dir="$exec_root/$2"
sysroot_out="$exec_root/$3"
headers_out="$exec_root/$4"
libraries_out="$exec_root/$5"
manifest_out="$exec_root/$6"
abi_out="$exec_root/$7"
loader_out="$exec_root/$8"
runtime_in="$exec_root/$9"
frigg_meson="$exec_root/${10}"
c_hdrs="$exec_root/${11}"
cxx_hdrs="$exec_root/${12}"
smarter="$exec_root/${13}"
bragi="$exec_root/${14}"
digest_out="$exec_root/${15}"
tool_bin="${ORLIX_MLIBC_TOOL_BIN:?}"
tool_root="$(/usr/bin/dirname "$tool_bin")"
sdkroot="${ORLIX_MLIBC_SDK:?}"
export PATH="$tool_bin:/usr/bin:/bin"
guest_lib="$tool_root/llvm/lib"
if [ -n "${ORLIX_MLIBC_TOOL_LIB:-}" ]; then export DYLD_LIBRARY_PATH="$guest_lib:$ORLIX_MLIBC_TOOL_LIB"; else export DYLD_LIBRARY_PATH="$guest_lib"; fi
meson_bin="$tool_bin/meson"
meson_py="$tool_bin/meson-python"
test -x "$meson_bin"; test -x "$meson_py"; test -x "$tool_bin/ninja"
test -x "$tool_bin/clang"; test -x "$tool_bin/clang++"; test -x "$tool_bin/llvm-ar"
test -x "$tool_bin/llvm-strip"; test -x "$tool_bin/ld.lld"; test -x "$tool_bin/llvm-nm"
guest_clang="$tool_root/llvm/bin/clang"
guest_clangxx="$tool_root/llvm/bin/clang++"
test -x "$guest_clang"; test -x "$guest_clangxx"; test -d "$guest_lib/clang"
guest_version="$("$guest_clang" --version 2>&1)"
printf '%s\n' "$guest_version" >&2
case "$guest_version" in
  *"Apple clang"*) printf '%s\n' "guest compiler is Apple clang" >&2; exit 1 ;;
esac
clang=clang
clangxx=clang++
ar=llvm-ar
strip=llvm-strip
lld="$tool_bin/ld.lld"
test -x "$lld"
test -d "$sdkroot"
test -s "$sdkroot/SDKSettings.json"
test -d "$uapi_dir/include"
test -s "$uapi_dir/include/linux/unistd.h"
test -s "$uapi_dir/include/asm/unistd.h"
work="$ORLIX_MLIBC_WORK_ROOT"
export PYTHONPATH="$exec_root"
shift 15
/usr/bin/python3 -B -c 'from pathlib import Path; import sys; from bazel.feasibility.mlibc import build_state; build_state.prepare(Path(sys.argv[1]), Path(sys.argv[2]).parent, [Path(p) for p in sys.argv[10:]], dict(zip(("frigg", "freestnd-c-hdrs", "freestnd-cxx-hdrs", "libsmarter", "bragi"), (Path(p).parent for p in sys.argv[5:10]))), Path(sys.argv[3]), Path(sys.argv[4]))' "$work" "$mlibc_meson" "$uapi_dir" "$runtime_in" "$frigg_meson" "$c_hdrs" "$cxx_hdrs" "$smarter" "$bragi" "$@"
uapi_dir="$work/inputs/uapi"
runtime_archives=("$work"/inputs/runtime/libcompiler_rt-*.a)
runtime_in="${runtime_archives[0]}"
export MESON_PACKAGE_CACHE_DIR="$work/inputs/subprojects"
/bin/mkdir -p "$work/arch"
/usr/bin/printf '%s\n' '#ifndef MLIBC_ARCH_DEFS_HPP' '#define MLIBC_ARCH_DEFS_HPP' '' '#include <stddef.h>' '' 'namespace mlibc {' '' 'inline constexpr size_t page_size = 16384;' '' '} // namespace mlibc' '' '#endif' > "$work/arch/arch-defs.hpp.new"
if ! /usr/bin/cmp -s "$work/arch/arch-defs.hpp.new" "$work/arch/arch-defs.hpp"; then /bin/mv "$work/arch/arch-defs.hpp.new" "$work/arch/arch-defs.hpp"; fi
/usr/bin/printf '%s\n' '[binaries]' "c = ['$guest_clang', '--target=aarch64-linux-gnu', '-ffile-prefix-map=$tool_root=/orlix/tools']" "cpp = ['$guest_clangxx', '--target=aarch64-linux-gnu', '-ffile-prefix-map=$tool_root=/orlix/tools']" "c_ld = '$lld'" "cpp_ld = '$lld'" "ar = '$ar'" "strip = '$strip'" '' '[host_machine]' "system = 'linux'" "cpu_family = 'aarch64'" "cpu = 'aarch64'" "endian = 'little'" '' '[properties]' 'needs_exe_wrapper = true' '' '[built-in options]' "c_args = ['-ffile-prefix-map=$work=/orlix', '-ffile-prefix-map=$tool_root=/orlix/tools', '-ffile-prefix-map=../mlibc=/orlix/mlibc', '-ffixed-x18', '-ffunction-sections', '-fdata-sections']" "cpp_args = ['-ffile-prefix-map=$work=/orlix', '-ffile-prefix-map=$tool_root=/orlix/tools', '-ffile-prefix-map=../mlibc=/orlix/mlibc', '-include', '$work/arch/arch-defs.hpp', '-ffixed-x18', '-ffunction-sections', '-fdata-sections']" "c_link_args = ['-fuse-ld=$lld', '$runtime_in']" "cpp_link_args = ['-fuse-ld=$lld', '$runtime_in']" > "$work/cross.ini"
/usr/bin/printf '%s\n' '[binaries]' "c = ['$clang', '-isysroot', '$sdkroot', '-ffile-prefix-map=$sdkroot=/orlix/macos-sdk', '-ffile-prefix-map=$tool_root=/orlix/tools']" "cpp = ['$clangxx', '-isysroot', '$sdkroot', '-ffile-prefix-map=$sdkroot=/orlix/macos-sdk', '-ffile-prefix-map=$tool_root=/orlix/tools']" > "$work/native.ini"
plan="$(/usr/bin/python3 -B -c 'from bazel.feasibility.mlibc.build_state import meson_setup_plan; import pathlib,sys; print(meson_setup_plan(pathlib.Path(sys.argv[1]).read_text(), pathlib.Path(sys.argv[2]).is_file()))' "$work/inputs/configure-cache" "$work/build/build.ninja")"
if [ "$plan" != skip ]; then
  configure_args=()
  if [ -n "$plan" ]; then
    read -r -a configure_args <<< "$plan"
  fi
  meson_setup=("$meson_py" "$meson_bin" setup)
  if [ "${#configure_args[@]}" -gt 0 ]; then
    meson_setup+=("${configure_args[@]}")
  fi
  meson_setup+=(
    "$work/build"
    "$work/mlibc"
    --wrap-mode=nodownload
    --force-fallback-for=freestnd-c-hdrs-aarch64,freestnd-cxx-hdrs-aarch64,frigg,libsmarter
    --cross-file "$work/cross.ini"
    --native-file "$work/native.ini"
    --prefix /usr
    -Ddefault_library=both
    -Dheaders_only=false
    -Dno_headers=false
    -Dbuild_tests=false
    -Dlibgcc_dependency=false
    -Dlinux_option=enabled
    -Dposix_option=enabled
    -Dglibc_option=enabled
    -Dbsd_option=enabled
    -Dlinux_kernel_headers="$uapi_dir/include"
    "-Dc_link_args=['-fuse-ld=$lld', '$runtime_in']"
    "-Dcpp_link_args=['-fuse-ld=$lld', '$runtime_in']"
  )
  PYTHONHOME="${ORLIX_MLIBC_MESON_HOME:?}" PYTHONPATH="${ORLIX_MLIBC_MESON_LIB:?}" \
    /usr/bin/env -u IPHONEOS_DEPLOYMENT_TARGET -u TVOS_DEPLOYMENT_TARGET -u WATCHOS_DEPLOYMENT_TARGET -u SDKROOT -u DEVELOPER_DIR \
      "${meson_setup[@]}"
fi
previous_end="$(/usr/bin/python3 -B -c 'from bazel.feasibility.mlibc.build_state import ninja_log_end; import pathlib,sys; print(ninja_log_end(pathlib.Path(sys.argv[1])))' "$work/build/.ninja_log")"
PYTHONHOME="${ORLIX_MLIBC_MESON_HOME:?}" PYTHONPATH="${ORLIX_MLIBC_MESON_LIB:?}" \
  /usr/bin/env -u IPHONEOS_DEPLOYMENT_TARGET -u SDKROOT -u DEVELOPER_DIR "$meson_py" "$meson_bin" compile -C "$work/build"
/usr/bin/python3 -B -c 'from bazel.feasibility.mlibc.build_state import report_compiler_edges; import pathlib,sys; report_compiler_edges(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), int(sys.argv[3]))' "$work/build/.ninja_log" "$work/build/build.ninja" "$previous_end"
/usr/bin/python3 -B -c 'from pathlib import Path; from bazel.build_state import _remove; import sys; _remove(Path(sys.argv[1]))' "$work/dest"
PYTHONHOME="${ORLIX_MLIBC_MESON_HOME:?}" PYTHONPATH="${ORLIX_MLIBC_MESON_LIB:?}" \
  /usr/bin/env -u IPHONEOS_DEPLOYMENT_TARGET -u SDKROOT -u DEVELOPER_DIR DESTDIR="$work/dest" "$meson_py" "$meson_bin" install -C "$work/build"
test -d "$work/dest/usr/include"
test -d "$work/dest/usr/lib"
test -s "$work/dest/usr/lib/libc.a"
/bin/mkdir -p "$sysroot_out" "$headers_out" "$libraries_out"
/bin/cp -R "$work/dest/." "$sysroot_out/"
/bin/cp -R "$work/dest/usr/include/." "$headers_out/"
/bin/cp -R "$work/dest/usr/lib/." "$libraries_out/"
/usr/bin/find "$libraries_out" "$sysroot_out/usr/lib" -type f \( -name '*.a' -o -name '*.so' -o -name '*.o' -o -name 'ld.so' \) -print0 | /usr/bin/xargs -0 -n 1 "$strip" -g
if [ -s "$libraries_out/ld.so" ]; then
    /bin/cp "$libraries_out/ld.so" "$loader_out"
else
    /usr/bin/printf '' > "$loader_out"
fi
"$tool_bin/llvm-nm" -j "$work/dest/usr/lib/libc.a" | /usr/bin/awk 'NF' | /usr/bin/sort -u > "$abi_out"
followed_uapi="$work/inputs/followed-header.sha256"
test -s "$followed_uapi"
uapi_digest="$(/usr/bin/tr -d '[:space:]' < "$followed_uapi")"
test "${#uapi_digest}" -eq 64
sysroot_digest="$(/usr/bin/find "$headers_out" "$libraries_out" -type f -print0 | /usr/bin/sort -z | /usr/bin/xargs -0 /usr/bin/shasum -a 256 | /usr/bin/awk '{print $1}' | /usr/bin/sort | /usr/bin/shasum -a 256 | /usr/bin/awk '{print $1}')"
test "${#sysroot_digest}" -eq 64
/usr/bin/printf '%s\n' "$sysroot_digest" > "$digest_out"
/usr/bin/printf '%s\n' '{' \
    '  "component": "OrlixMLibC",' \
    '  "consumed_uapi_digest": "'"$uapi_digest"'",' \
    '  "sysroot_digest": "'"$sysroot_digest"'",' \
    '  "linux_input": "OrlixInstalledUapiInfo",' \
    '  "target_triple": "aarch64-linux-gnu"' \
    '}' > "$manifest_out"
""")
    ctx.actions.run_shell(
        mnemonic = "OrlixMLibCSysroot",
        progress_message = "Building OrlixMLibC from declared tools",
        command = "PYTHONPATH=. /usr/bin/python3 -B -c 'from bazel.feasibility.mlibc import build_state; import sys; raise SystemExit(build_state.run(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5], sys.argv[6], sys.argv[7:]))' \"$@\"",
        arguments = [
            script.path,
            ctx.file.toolchain_identity.path,
            ctx.file.compiler_identity.path,
            "mlibc-work",
            ctx.file.clang.dirname,
            ctx.file.sdk_settings.dirname,
            ctx.file.mlibc_meson.path,
            uapi.headers.path,
            sysroot.path,
            headers.path,
            libraries.path,
            manifest.path,
            abi.path,
            loader.path,
            runtime.path,
            ctx.file.frigg_meson.path,
            ctx.file.freestnd_c.path,
            ctx.file.freestnd_cxx.path,
            ctx.file.libsmarter.path,
            ctx.file.bragi.path,
            digest.path,
        ] + [f.path for f in ctx.files.patches],
        inputs = depset(
            direct = [
                script,
                ctx.file.toolchain_identity,
                ctx.file.compiler_identity,
                ctx.file.build_state,
                ctx.file.shared_state,
                ctx.file.mlibc_meson,
                uapi.headers,
                ctx.file.frigg_meson,
                ctx.file.freestnd_c,
                ctx.file.freestnd_cxx,
                ctx.file.libsmarter,
                ctx.file.bragi,
                ctx.file.header_digest,
                runtime,
                ctx.file.clang,
                ctx.file.clangxx,
                ctx.file.guest_clang,
                ctx.file.guest_clangxx,
                ctx.file.llvm_ar,
                ctx.file.llvm_strip,
                ctx.file.ld_lld,
                ctx.file.meson,
                ctx.file.ninja,
                ctx.file.meson_python,
                ctx.file.llvm_nm,
                ctx.file.sdk_settings,
            ] + ctx.files.patches + ctx.files.mlibc_support + ctx.files.tool_libs + ctx.files.clang_resources + ctx.files.guest_clang_support + ctx.files.macos_sdk,
            transitive = [
                ctx.attr.mlibc_source[DefaultInfo].files,
                ctx.attr.frigg_source[DefaultInfo].files,
                ctx.attr.freestnd_c_source[DefaultInfo].files,
                ctx.attr.freestnd_cxx_source[DefaultInfo].files,
                ctx.attr.libsmarter_source[DefaultInfo].files,
                ctx.attr.bragi_source[DefaultInfo].files,
            ],
        ),
        outputs = [sysroot, headers, libraries, manifest, abi, loader, digest],
        env = _pinned_env(ctx),
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-sandbox": "1"},
    )
    ctx.actions.run_shell(
        mnemonic = "OrlixPromotedMlibcProduct",
        progress_message = "Assembling the canonical OrlixMLibC promotion product",
        command = r"""
set -euo pipefail
exec_root="$PWD"
out="$exec_root/$1"
sysroot="$exec_root/$2"
runtime="$exec_root/$3"
abi="$exec_root/$4"
manifest="$exec_root/$5"
digest="$exec_root/$6"
test -d "$sysroot/usr/include"
test -d "$sysroot/usr/lib"
test -s "$runtime"
test -s "$abi"
test -s "$manifest"
test -s "$digest"
/bin/mkdir -p "$out"
/bin/cp -R "$sysroot/." "$out/"
/bin/cp "$runtime" "$out/libcompiler_rt.a"
/bin/cp "$abi" "$out/abi.txt"
/bin/cp "$manifest" "$out/manifest.json"
/bin/cp "$digest" "$out/sysroot.sha256"
""",
        arguments = [product.path, sysroot.path, runtime.path, abi.path, manifest.path, digest.path],
        inputs = [sysroot, runtime, abi, manifest, digest],
        outputs = [product],
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-sandbox": "1"},
    )
    artifact_identity = declare_artifact_identity(
        ctx,
        "sysroot",
        ctx.file._artifact_identity_serializer,
        root = product,
    )
    return [
        DefaultInfo(files = depset([
            sysroot,
            headers,
            libraries,
            manifest,
            abi,
            loader,
            runtime,
            digest,
            product,
            artifact_identity.manifest,
            artifact_identity.digest,
            runtime_identity.manifest,
            runtime_identity.digest,
        ])),
        OrlixLibcSysrootInfo(
            abi_manifest = abi,
            artifact_identity_digest = artifact_identity.digest,
            artifact_identity_manifest = artifact_identity.manifest,
            compiler_runtime = runtime,
            compiler_runtime_identity_digest = runtime_identity.digest,
            compiler_runtime_identity_manifest = runtime_identity.manifest,
            consumed_uapi_digest = uapi.uapi_digest,
            dynamic_loader = loader,
            headers = headers,
            libraries = libraries,
            sysroot_digest = digest,
            target_triple = "aarch64-linux-gnu",
        ),
    ]

orlix_mlibc_sysroot = rule(
    implementation = _mlibc_sysroot_impl,
    attrs = {
        "toolchain_identity": attr.label(allow_single_file = True, mandatory = True),
        "compiler_identity": attr.label(allow_single_file = True, mandatory = True),
        "build_state": attr.label(allow_single_file = True, default = ":build_state.py"),
        "header_digest": attr.label(allow_single_file = True, default = ":header_digest.py"),
        "shared_state": attr.label(allow_single_file = True, default = "//bazel:build_state.py"),
        "runtime_toolchain_identity": attr.label(allow_single_file = True, mandatory = True),
        "uapi": attr.label(mandatory = True, providers = [OrlixInstalledUapiInfo]),
        "mlibc_source": attr.label(mandatory = True),
        "mlibc_meson": attr.label(allow_single_file = True, mandatory = True),
        "patches": attr.label(mandatory = True),
        "frigg_source": attr.label(mandatory = True),
        "frigg_meson": attr.label(allow_single_file = True, mandatory = True),
        "freestnd_c_source": attr.label(mandatory = True),
        "freestnd_c": attr.label(allow_single_file = True, mandatory = True),
        "freestnd_cxx_source": attr.label(mandatory = True),
        "freestnd_cxx": attr.label(allow_single_file = True, mandatory = True),
        "libsmarter_source": attr.label(mandatory = True),
        "libsmarter": attr.label(allow_single_file = True, mandatory = True),
        "bragi_source": attr.label(mandatory = True),
        "bragi": attr.label(allow_single_file = True, mandatory = True),
        "compiler_rt_source": attr.label(mandatory = True),
        "compiler_rt": attr.label(allow_single_file = True, mandatory = True),
        "clang": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/clang")),
        "clangxx": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/clang++")),
        "guest_clang": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/llvm/bin/clang")),
        "guest_clangxx": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/llvm/bin/clang++")),
        "llvm_ar": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/llvm-ar")),
        "llvm_strip": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/llvm-strip")),
        "ld_lld": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/ld.lld")),
        "meson": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/meson")),
        "ninja": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/ninja")),
        "meson_python": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/meson-python")),
        "llvm_nm": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/llvm-nm")),
        "sdk_settings": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/sdk/SDKSettings.json")),
        "macos_sdk": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:macos_sdk")),
        "tool_libs": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:tool_libs")),
        "clang_resources": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:clang_resources")),
        "guest_clang_support": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:guest_clang_support")),
        "mlibc_support": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:mlibc_support")),
        "_artifact_identity_serializer": attr.label(
            allow_single_file = True,
            default = Label("//bazel:content_digest.py"),
        ),
    },
)
