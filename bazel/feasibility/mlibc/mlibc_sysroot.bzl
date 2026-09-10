"""Meson/Ninja OrlixMLibC sysroot from installed Linux UAPI only."""

load("//bazel/providers:kernel_info.bzl", "OrlixInstalledUapiInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")

def _pinned_env(ctx):
    shell = ctx.configuration.default_shell_env
    developer_dir = shell.get("DEVELOPER_DIR")
    if not developer_dir:
        fail("mlibc sysroot requires action_env DEVELOPER_DIR")
    env = {
        "DEVELOPER_DIR": developer_dir,
        "HOME": "/var/empty",
        "PATH": "/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/bin:/bin",
    }
    tmpdir = shell.get("TMPDIR")
    if tmpdir:
        env["TMPDIR"] = tmpdir
    return env

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
    ctx.actions.run_shell(
        mnemonic = "OrlixMLibCSysroot",
        progress_message = "Building OrlixMLibC sysroot from installed UAPI",
        command = r"""
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
runtime_out="$exec_root/$9"
frigg_meson="$exec_root/${10}"
c_hdrs="$exec_root/${11}"
cxx_hdrs="$exec_root/${12}"
smarter="$exec_root/${13}"
bragi="$exec_root/${14}"
compiler_rt_marker="$exec_root/${15}"
digest_in="$exec_root/${16}"
digest_out="$exec_root/${17}"
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
work="$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/orlix-mlibc.XXXXXX")"
trap '/bin/rm -rf "$work"' EXIT
mlibc_src="$(/usr/bin/dirname "$mlibc_meson")"
/bin/mkdir -p "$work/mlibc" "$work/build" "$work/dest"
/bin/cp -R "$mlibc_src/." "$work/mlibc"
/usr/bin/find "$work/mlibc" -type d -exec /bin/chmod u+w {} +
shift 17
while [ "$#" -gt 0 ]; do
    patch="$exec_root/$1"
    /usr/bin/patch --batch --forward -p1 -d "$work/mlibc" -i "$patch"
    shift
done
/bin/mkdir -p "$work/mlibc/subprojects"
copy_wrap() {
    marker="$1"
    dest="$2"
    src="$(/usr/bin/dirname "$marker")"
    /bin/rm -rf "$dest"
    /bin/mkdir -p "$dest"
    /bin/cp -R "$src/." "$dest"
}
copy_wrap "$frigg_meson" "$work/mlibc/subprojects/frigg"
copy_wrap "$c_hdrs" "$work/mlibc/subprojects/freestnd-c-hdrs"
copy_wrap "$cxx_hdrs" "$work/mlibc/subprojects/freestnd-cxx-hdrs"
copy_wrap "$smarter" "$work/mlibc/subprojects/libsmarter"
copy_wrap "$bragi" "$work/mlibc/subprojects/bragi"
/bin/mkdir -p "$work/arch"
/usr/bin/printf '%s\n' '#ifndef MLIBC_ARCH_DEFS_HPP' '#define MLIBC_ARCH_DEFS_HPP' '' '#include <stddef.h>' '' 'namespace mlibc {' '' 'inline constexpr size_t page_size = 16384;' '' '} // namespace mlibc' '' '#endif' > "$work/arch/arch-defs.hpp"
lld="$(/usr/bin/command -v ld.lld)"
test -n "$lld"
builtins="$(/usr/bin/dirname "$compiler_rt_marker")"
/bin/mkdir -p "$work/rt"
for source_name in addtf3.c aarch64/fp_mode.c comparetf2.c clzti2.c divtf3.c extenddftf2.c extendhftf2.c extendsftf2.c extendxftf2.c fixtfdi.c fixtfsi.c fixtfti.c fixunstfdi.c fixunstfsi.c fixunstfti.c floatditf.c floatsitf.c floattitf.c floatunditf.c floatunsitf.c floatuntitf.c muldc3.c mulsc3.c multc3.c multf3.c powitf2.c subtf3.c trunctfbf2.c trunctfdf2.c trunctfhf2.c trunctfsf2.c trunctfxf2.c udivmodti4.c udivti3.c; do
    source="$builtins/$source_name"
    object_name="$(/usr/bin/printf '%s' "$source_name" | /usr/bin/tr / -)"
    object="$work/rt/${object_name%.c}.o"
    test -s "$source"
    "$clang" --target=aarch64-linux-gnu -ffreestanding -fno-builtin -O2 -I"$builtins" -c "$source" -o "$object"
done
"$ar" rcs "$work/libcompiler_rt.a" "$work/rt"/*.o
test -s "$work/libcompiler_rt.a"
/usr/bin/printf '%s\n' '[binaries]' "c = ['$clang', '--target=aarch64-linux-gnu']" "cpp = ['$clangxx', '--target=aarch64-linux-gnu']" "c_ld = 'lld'" "cpp_ld = 'lld'" "ar = '$ar'" "strip = '$strip'" '' '[host_machine]' "system = 'linux'" "cpu_family = 'aarch64'" "cpu = 'aarch64'" "endian = 'little'" '' '[properties]' 'needs_exe_wrapper = true' '' '[built-in options]' "c_args = ['-I$work/arch', '-isystem', '$work/mlibc/subprojects/freestnd-c-hdrs/aarch64/include']" "cpp_args = ['-I$work/arch', '-isystem', '$work/mlibc/subprojects/freestnd-c-hdrs/aarch64/include', '-isystem', '$work/mlibc/subprojects/freestnd-cxx-hdrs/aarch64/include']" "c_link_args = ['-fuse-ld=lld', '$work/libcompiler_rt.a']" "cpp_link_args = ['-fuse-ld=lld', '$work/libcompiler_rt.a']" > "$work/cross.ini"
/usr/bin/printf '%s\n' '[binaries]' "c = ['$clang', '-isysroot', '$sdkroot']" "cpp = ['$clangxx', '-isysroot', '$sdkroot']" > "$work/native.ini"
env -u IPHONEOS_DEPLOYMENT_TARGET -u TVOS_DEPLOYMENT_TARGET -u WATCHOS_DEPLOYMENT_TARGET \
    SDKROOT="$sdkroot" \
    "$meson_bin" setup "$work/build" "$work/mlibc" \
        --wrap-mode=nodownload \
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
        -Dlinux_kernel_headers="$uapi_dir/include"
env -u IPHONEOS_DEPLOYMENT_TARGET SDKROOT="$sdkroot" "$meson_bin" compile -C "$work/build"
env -u IPHONEOS_DEPLOYMENT_TARGET SDKROOT="$sdkroot" DESTDIR="$work/dest" "$meson_bin" install -C "$work/build"
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
/bin/cp "$work/libcompiler_rt.a" "$runtime_out"
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
""",
        arguments = [
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
            ctx.file.compiler_rt.path,
            uapi.uapi_digest.path,
            digest.path,
        ] + [f.path for f in ctx.files.patches],
        inputs = depset(
            direct = [
                ctx.file.mlibc_meson,
                uapi.headers,
                uapi.uapi_digest,
                ctx.file.frigg_meson,
                ctx.file.freestnd_c,
                ctx.file.freestnd_cxx,
                ctx.file.libsmarter,
                ctx.file.bragi,
                ctx.file.compiler_rt,
            ] + ctx.files.patches,
            transitive = [
                ctx.attr.mlibc_source[DefaultInfo].files,
                ctx.attr.frigg_source[DefaultInfo].files,
                ctx.attr.freestnd_c_source[DefaultInfo].files,
                ctx.attr.freestnd_cxx_source[DefaultInfo].files,
                ctx.attr.libsmarter_source[DefaultInfo].files,
                ctx.attr.bragi_source[DefaultInfo].files,
                ctx.attr.compiler_rt_source[DefaultInfo].files,
            ],
        ),
        outputs = [sysroot, headers, libraries, manifest, abi, loader, runtime, digest],
        env = _pinned_env(ctx),
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-sandbox": "1"},
    )
    return [
        DefaultInfo(files = depset([sysroot, headers, libraries, manifest, abi, loader, runtime, digest])),
        OrlixLibcSysrootInfo(
            abi_manifest = abi,
            compiler_runtime = runtime,
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
    },
)
