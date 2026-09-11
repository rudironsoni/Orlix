"""Meson/Ninja OrlixMLibC sysroot from installed Linux UAPI only."""

load("//bazel:artifact_identity.bzl", "declare_artifact_identity")
load("//bazel/providers:kernel_info.bzl", "OrlixInstalledUapiInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")

def _pinned_env(ctx):
    shell = ctx.configuration.default_shell_env
    developer_dir = shell.get("ORLIX_PINNED_DEVELOPER_DIR")
    if not developer_dir:
        fail("mlibc sysroot requires action_env ORLIX_PINNED_DEVELOPER_DIR")
    env = {
        "DEVELOPER_DIR": developer_dir,
        "HOME": "/var/empty",
        "PATH": "/opt/homebrew/opt/lld/bin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/bin:/bin",
        "PYTHONDONTWRITEBYTECODE": "1",
        "CCACHE_CONFIGPATH": "/dev/null",
        "CCACHE_COMPILERCHECK": "content",
        "CCACHE_MAXSIZE": "20G",
    }
    tmpdir = shell.get("TMPDIR")
    if tmpdir:
        env["TMPDIR"] = tmpdir
    return env

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
export CCACHE_EXTRAFILES="$exec_root/$3"
export CCACHE_BASEDIR="$exec_root"
shift 3
launcher="${ORLIX_COMPILER_LAUNCHER-/opt/homebrew/bin/ccache}"
case "$launcher" in
  /opt/homebrew/bin/ccache) test -n "${CCACHE_DIR:-}" || { echo "CCACHE_DIR is required; use the repository Make interface" >&2; exit 1; } ;;
  "") ;;
  *) echo "unsupported compiler launcher: $launcher" >&2; exit 1 ;;
esac
clang="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
compiler=("$clang")
if [ -n "$launcher" ]; then compiler=("$launcher" "$clang"); fi
ar="/opt/homebrew/opt/llvm/bin/llvm-ar"
work="$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/orlix-compiler-rt.XXXXXX")"
trap '/bin/rm -rf "$work"' EXIT
for relative in "$@"; do
    source="$exec_root/$relative"
    source_name="${source#"$builtins/"}"
    object_name="${source_name//\//-}"
    "${compiler[@]}" --target=aarch64-linux-gnu -ffreestanding -fno-builtin -ffixed-x18 -O2 "-ffile-prefix-map=$builtins=/orlix/compiler-rt" -I"$builtins" -c "$source" -o "$work/${object_name%.c}.o"
done
"$ar" rcs "$runtime_out" "$work"/*.o
test -s "$runtime_out"
""",
        arguments = [builtins, runtime.path, ctx.file.compiler_identity.path] + [f.path for f in selected],
        inputs = depset(selected + headers + [ctx.file.runtime_toolchain_identity, ctx.file.compiler_identity]),
        outputs = [runtime],
        env = _pinned_env(ctx),
        use_default_shell_env = True,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-remote-cache": "1"},
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
digest_in="$exec_root/${15}"
digest_out="$exec_root/${16}"
test -n "${DEVELOPER_DIR:-}"
xcode_ver="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcodebuild -version)"
xcode_name="$(printf '%s\n' "$xcode_ver" | /usr/bin/sed -n '1p')"
xcode_build="$(printf '%s\n' "$xcode_ver" | /usr/bin/sed -n '2p')"
case "$xcode_name|$xcode_build" in
  "Xcode 26.6|Build version 17F113"|"Xcode 27.0|Build version 27A5252f") ;;
  *) echo "unsupported Xcode: $xcode_ver" >&2; exit 1 ;;
esac
meson_bin="$(/usr/bin/command -v meson)"
ninja_bin="$(/usr/bin/command -v ninja)"
test -n "$meson_bin"; test -n "$ninja_bin"
clang="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
clangxx="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang++"
ar="$(/usr/bin/command -v llvm-ar)"
strip="$(/usr/bin/command -v llvm-strip)"
if [ -z "$ar" ]; then ar="/opt/homebrew/opt/llvm/bin/llvm-ar"; fi
if [ -z "$strip" ]; then strip="/opt/homebrew/opt/llvm/bin/llvm-strip"; fi
test -x "$ar"; test -x "$strip"
sdkroot="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcrun --sdk macosx --show-sdk-path)"
test -x "$clang"; test -d "$sdkroot"
test -d "$uapi_dir/include"
test -s "$uapi_dir/include/linux/unistd.h"
test -s "$uapi_dir/include/asm/unistd.h"
work="$ORLIX_MLIBC_WORK_ROOT"
export PYTHONPATH="$exec_root"
shift 16
/usr/bin/python3 -B -c 'from pathlib import Path; import sys; from bazel.feasibility.mlibc import build_state; build_state.prepare(Path(sys.argv[1]), Path(sys.argv[2]).parent, [Path(p) for p in sys.argv[10:]], dict(zip(("frigg", "freestnd-c-hdrs", "freestnd-cxx-hdrs", "libsmarter", "bragi"), (Path(p).parent for p in sys.argv[5:10]))), Path(sys.argv[3]), Path(sys.argv[4]))' "$work" "$mlibc_meson" "$uapi_dir" "$runtime_in" "$frigg_meson" "$c_hdrs" "$cxx_hdrs" "$smarter" "$bragi" "$@"
uapi_dir="$work/inputs/uapi"
runtime_archives=("$work"/inputs/runtime/libcompiler_rt-*.a)
runtime_in="${runtime_archives[0]}"
export MESON_PACKAGE_CACHE_DIR="$work/inputs/subprojects"
launcher="${ORLIX_COMPILER_LAUNCHER-/opt/homebrew/bin/ccache}"
case "$launcher" in
  /opt/homebrew/bin/ccache) test -n "${CCACHE_DIR:-}" || { echo "CCACHE_DIR is required; use the repository Make interface" >&2; exit 1; } ;;
  "") ;;
  *) echo "unsupported compiler launcher: $launcher" >&2; exit 1 ;;
esac
export CCACHE_BASEDIR="$work"
export CCACHE_EXTRAFILES="$exec_root/__COMPILER_IDENTITY__"
export CCACHE_STATSLOG="$work/compiler-cache.log"
: > "$CCACHE_STATSLOG"
compiler_prefix=""
if [ -n "$launcher" ]; then compiler_prefix="'$launcher', "; fi
/bin/mkdir -p "$work/arch"
/usr/bin/printf '%s\n' '#ifndef MLIBC_ARCH_DEFS_HPP' '#define MLIBC_ARCH_DEFS_HPP' '' '#include <stddef.h>' '' 'namespace mlibc {' '' 'inline constexpr size_t page_size = 16384;' '' '} // namespace mlibc' '' '#endif' > "$work/arch/arch-defs.hpp.new"
if ! /usr/bin/cmp -s "$work/arch/arch-defs.hpp.new" "$work/arch/arch-defs.hpp"; then /bin/mv "$work/arch/arch-defs.hpp.new" "$work/arch/arch-defs.hpp"; fi
lld="$(/usr/bin/command -v ld.lld)"
test -n "$lld"
/usr/bin/printf '%s\n' '[binaries]' "c = [${compiler_prefix}'$clang', '--target=aarch64-linux-gnu']" "cpp = [${compiler_prefix}'$clangxx', '--target=aarch64-linux-gnu']" "c_ld = 'lld'" "cpp_ld = 'lld'" "ar = '$ar'" "strip = '$strip'" '' '[host_machine]' "system = 'linux'" "cpu_family = 'aarch64'" "cpu = 'aarch64'" "endian = 'little'" '' '[properties]' 'needs_exe_wrapper = true' '' '[built-in options]' "c_args = ['-ffile-prefix-map=$work=/orlix', '-ffile-prefix-map=../mlibc=/orlix/mlibc', '-ffixed-x18', '-ffunction-sections', '-fdata-sections']" "cpp_args = ['-ffile-prefix-map=$work=/orlix', '-ffile-prefix-map=../mlibc=/orlix/mlibc', '-include', '$work/arch/arch-defs.hpp', '-ffixed-x18', '-ffunction-sections', '-fdata-sections']" "c_link_args = ['-fuse-ld=lld', '$runtime_in']" "cpp_link_args = ['-fuse-ld=lld', '$runtime_in']" > "$work/cross.ini"
/usr/bin/printf '%s\n' '[binaries]' "c = ['$clang', '-isysroot', '$sdkroot']" "cpp = ['$clangxx', '-isysroot', '$sdkroot']" > "$work/native.ini"
configure_args=(--reconfigure)
if [ "$(/bin/cat "$work/inputs/configure-cache")" = clear ]; then configure_args+=(--clearcache); fi
/usr/bin/env -u IPHONEOS_DEPLOYMENT_TARGET -u TVOS_DEPLOYMENT_TARGET -u WATCHOS_DEPLOYMENT_TARGET \
    SDKROOT="$sdkroot" \
    "$meson_bin" setup "${configure_args[@]}" "$work/build" "$work/mlibc" \
        --wrap-mode=nodownload \
        --force-fallback-for=freestnd-c-hdrs-aarch64,freestnd-cxx-hdrs-aarch64,frigg,libsmarter \
        --cross-file "$work/cross.ini" \
        --native-file "$work/native.ini" \
        --prefix /usr \
        -Ddefault_library=both \
        -Dheaders_only=false \
        -Dno_headers=false \
        -Dbuild_tests=false \
        -Dlibgcc_dependency=false \
        -Dlinux_option=enabled \
        -Dposix_option=enabled \
        -Dglibc_option=enabled \
        -Dbsd_option=enabled \
        -Dlinux_kernel_headers="$uapi_dir/include" \
        "-Dc_link_args=['-fuse-ld=lld', '$runtime_in']" \
        "-Dcpp_link_args=['-fuse-ld=lld', '$runtime_in']"
/usr/bin/env -u IPHONEOS_DEPLOYMENT_TARGET SDKROOT="$sdkroot" "$meson_bin" compile -C "$work/build"
if [ -n "$launcher" ]; then "$launcher" --print-log-stats --format=json; fi
/usr/bin/python3 -B -c 'from pathlib import Path; from bazel.build_state import _remove; import sys; _remove(Path(sys.argv[1]))' "$work/dest"
/usr/bin/env -u IPHONEOS_DEPLOYMENT_TARGET SDKROOT="$sdkroot" DESTDIR="$work/dest" "$meson_bin" install -C "$work/build"
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
/usr/bin/nm "$work/dest/usr/lib/libc.a" | /usr/bin/awk '{print $3}' | /usr/bin/sort -u > "$abi_out"
test -s "$digest_in"
uapi_digest="$(/usr/bin/tr -d '[:space:]' < "$digest_in")"
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
""".replace("__COMPILER_IDENTITY__", ctx.file.compiler_identity.path))
    ctx.actions.run_shell(
        mnemonic = "OrlixMLibCSysroot",
        progress_message = "Building OrlixMLibC with incremental Ninja state",
        command = "PYTHONPATH=. /usr/bin/python3 -B -c 'from bazel.feasibility.mlibc import build_state; import sys; raise SystemExit(build_state.run(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]))' \"$@\"",
        arguments = [
            script.path,
            ctx.file.toolchain_identity.path,
            ctx.file.compiler_identity.path,
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
            uapi.uapi_digest.path,
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
                uapi.uapi_digest,
                ctx.file.frigg_meson,
                ctx.file.freestnd_c,
                ctx.file.freestnd_cxx,
                ctx.file.libsmarter,
                ctx.file.bragi,
                runtime,
            ] + ctx.files.patches,
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
        use_default_shell_env = True,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-remote-cache": "1", "no-sandbox": "1"},
    )
    artifact_identity = declare_artifact_identity(
        ctx,
        "sysroot",
        ctx.file._artifact_identity_serializer,
        root = sysroot,
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
        "_artifact_identity_serializer": attr.label(
            allow_single_file = True,
            default = Label("//bazel:content_digest.py"),
        ),
    },
)
