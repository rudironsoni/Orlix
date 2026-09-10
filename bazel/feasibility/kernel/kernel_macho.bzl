"""Mach-O OrlixKernel.a from Kbuild product compile. Does not invoke OrlixKernel/Makefile."""

load("@rules_cc//cc:cc_import.bzl", "cc_import")
load("//bazel/providers:kernel_info.bzl", "OrlixLinuxArchiveInfo")

def _pinned_env(ctx):
    shell = ctx.configuration.default_shell_env
    developer_dir = shell.get("DEVELOPER_DIR")
    if not developer_dir:
        fail("kernel Mach-O archive requires action_env DEVELOPER_DIR")
    env = {
        "DEVELOPER_DIR": developer_dir,
        "HOME": "/var/empty",
        "PATH": "/opt/homebrew/opt/gnu-sed/libexec/gnubin:/opt/homebrew/opt/coreutils/libexec/gnubin:/opt/homebrew/opt/lld/bin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/bin:/bin",
        "ORLIX_KERNEL_PORT_PREPARED": "1",
        "ORLIX_KERNEL_KUNIT": "0",
        "ORLIX_OS_LINUX_PAGE_SIZE": "16384",
        "ORLIX_COMPILER_LAUNCHER": "",
        "PROFILE": ctx.attr.profile,
        "ORLIX_KERNEL_ARCHIVE_PLATFORMS": ctx.attr.destination,
    }
    tmpdir = shell.get("TMPDIR")
    if tmpdir:
        env["TMPDIR"] = tmpdir
    prepared_isa = shell.get("ORLIX_TCTI_ISA_PREPARED")
    if prepared_isa:
        env["ORLIX_TCTI_ISA_PREPARED"] = prepared_isa
    return env

def _kernel_macho_impl(ctx):
    archive = ctx.actions.declare_file(ctx.label.name + "/OrlixKernel.a")
    symbols = ctx.actions.declare_file(ctx.label.name + "/symbols.txt")
    digest = ctx.actions.declare_file(ctx.label.name + "/archive.sha256")
    manifest = ctx.actions.declare_file(ctx.label.name + "/manifest.json")
    boot_resources = ctx.actions.declare_directory(ctx.label.name + "/arch")
    overlay_files = ctx.files.overlay
    patch_files = ctx.files.patches
    config_files = ctx.files.configs
    engine_files = ctx.files.kbuild_engine
    extra_files = ctx.files.extra_inputs
    ctx.actions.run_shell(
        mnemonic = "OrlixKernelMachOArchive",
        progress_message = "Compiling Mach-O OrlixKernel.a from prepared Linux sources",
        command = r"""
set -euo pipefail
exec_root="$PWD"
linux_makefile="$exec_root/$1"
archive_out="$exec_root/$2"
symbols_out="$exec_root/$3"
digest_out="$exec_root/$4"
manifest_out="$exec_root/$5"
overlay_count="$6"
patch_count="$7"
config_count="$8"
engine_count="$9"
extra_count="${10}"
boot_resources="$exec_root/${11}"
shift 11
overlay_paths=()
i=0
while [ "$i" -lt "$overlay_count" ]; do
  overlay_paths+=("$1")
  shift
  i=$((i + 1))
done
patch_paths=()
i=0
while [ "$i" -lt "$patch_count" ]; do
  patch_paths+=("$1")
  shift
  i=$((i + 1))
done
config_paths=()
i=0
while [ "$i" -lt "$config_count" ]; do
  config_paths+=("$1")
  shift
  i=$((i + 1))
done
engine_paths=()
i=0
while [ "$i" -lt "$engine_count" ]; do
  engine_paths+=("$1")
  shift
  i=$((i + 1))
done
i=0
while [ "$i" -lt "$extra_count" ]; do
  shift
  i=$((i + 1))
done
test -n "${DEVELOPER_DIR:-}"
xcode_ver="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcodebuild -version)"
xcode_name="$(printf '%s\n' "$xcode_ver" | /usr/bin/sed -n '1p')"
xcode_build="$(printf '%s\n' "$xcode_ver" | /usr/bin/sed -n '2p')"
case "$xcode_name|$xcode_build" in
  "Xcode 26.6|Build version 17F113"|"Xcode 27.0|Build version 27A5252f") ;;
  *) echo "unsupported Xcode: $xcode_ver" >&2; exit 1 ;;
esac
case "$linux_makefile" in
  *orlix_linux_source*/Makefile) ;;
  *) echo "kernel Mach-O must consume upstream Linux Makefile, got $linux_makefile" >&2; exit 1 ;;
esac
gmake="$(/usr/bin/command -v gmake)"
test -n "$gmake"
xcode_clang="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
test -x "$xcode_clang"
clang="/opt/homebrew/opt/llvm/bin/clang"
test -x "$clang"
sdkroot="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcrun --sdk macosx --show-sdk-path)"
test -n "$sdkroot"
linux_src="$(/usr/bin/dirname "$linux_makefile")"
work="$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/orlix-kernel-macho.XXXXXX")"
trap '/bin/rm -rf "$work"' EXIT
port="$work/OrlixKernel/src/linux-6.12.105-port"
/bin/mkdir -p "$port" "$work/OrlixKernel/orlix-tcti-isa" "$work/OrlixKernel/$PROFILE"
hostcc="$work/hostcc"
/usr/bin/printf '%s\n' '#!/bin/bash' "exec \"$xcode_clang\" -isysroot \"$sdkroot\" \"\$@\"" > "$hostcc"
/bin/chmod +x "$hostcc"
/bin/cp -R "$linux_src/." "$port"
/usr/bin/find "$port" -type d -exec /bin/chmod u+w {} +
overlay_prefix="OrlixKernel/Sources/ports/orlix/overlay/"
for rel in "${overlay_paths[@]}"; do
  case "$rel" in
    *"$overlay_prefix"*) dest_rel="${rel#*"$overlay_prefix"}" ;;
    *) echo "overlay path missing overlay prefix: $rel" >&2; exit 1 ;;
  esac
  /bin/mkdir -p "$port/$(/usr/bin/dirname "$dest_rel")"
  /bin/cp "$exec_root/$rel" "$port/$dest_rel"
done
uapi_source="$port/arch/arm64/include/uapi/asm"
uapi_target="$port/arch/orlix/include/uapi/asm"
test -d "$uapi_source"
/bin/rm -rf "$uapi_target"
/bin/mkdir -p "$(/usr/bin/dirname "$uapi_target")"
/bin/cp -R "$uapi_source" "$uapi_target"
if [ ! -s "$uapi_target/types.h" ]; then
  /bin/cp "$port/include/uapi/asm-generic/types.h" "$uapi_target/types.h"
fi
profile_config=""
for rel in "${config_paths[@]}"; do
  case "$rel" in
    */"${PROFILE}_defconfig") profile_config="$exec_root/$rel" ;;
  esac
done
test -n "$profile_config" && test -s "$profile_config"
/bin/mkdir -p "$port/arch/orlix/configs"
/usr/bin/awk '!/^CONFIG_PAGE_SIZE_[0-9]+KB=y$/' "$profile_config" > "$port/arch/orlix/configs/defconfig"
printf '%s\n' 'CONFIG_PAGE_SIZE_16KB=y' >> "$port/arch/orlix/configs/defconfig"
for rel in "${patch_paths[@]}"; do
  case "$rel" in
    *.patch|*.diff)
      /usr/bin/patch -d "$port" -p1 < "$exec_root/$rel" >/dev/null
      ;;
  esac
done
printf '%s\n' 'linux_version=6.12.105' "profile=$PROFILE" 'linux_uapi_arch=arm64' 'linux_page_size=16384' > "$port/.orlix-port-profile"
isa_dest="$work/OrlixKernel/orlix-tcti-isa"
/bin/mkdir -p "$isa_dest"
prepared_isa="${ORLIX_TCTI_ISA_PREPARED:-}"
if [ -z "$prepared_isa" ] || [ ! -s "$prepared_isa/source_manifest.def" ]; then
  candidate="$exec_root/../../../../OrlixKernel/orlix-tcti-isa"
  if [ -s "$candidate/source_manifest.def" ]; then
    prepared_isa="$candidate"
  fi
fi
test -s "$prepared_isa/source_manifest.def" || {
  echo "missing prepared ISA tables at ${prepared_isa:-unset}/source_manifest.def" >&2
  echo "run make __tcti-isa-refresh so Build/OrlixKernel/orlix-tcti-isa exists" >&2
  exit 1
}
port_isa="$port/arch/orlix/hosted_exec/orlix_tcti/isa"
/bin/mkdir -p "$port_isa"
for isa_name in \
  manifest \
  source_manifest.def \
  target_asl_availability.def \
  target_feature_applicability.def \
  target_feature_artifact.def \
  target_feature_field_domain_binding.def \
  target_instruction_artifact_generated.h \
  target_register_artifact.def \
  target_runtime_capability_cohort_artifact.def \
  target_system_accessor_reconciliation.def
do
  test -s "$prepared_isa/$isa_name" || { echo "missing prepared ISA artifact: $prepared_isa/$isa_name" >&2; exit 1; }
  /bin/cp "$prepared_isa/$isa_name" "$isa_dest/$isa_name"
  /bin/cp "$prepared_isa/$isa_name" "$port_isa/$isa_name"
done
export ORLIX_BUILD_ROOT="$work"
export ORLIX_KERNEL_PORT_PREPARED=1
export ORLIX_KERNEL_KUNIT=0
export ORLIX_OS_LINUX_PAGE_SIZE=16384
export ORLIX_COMPILER_LAUNCHER=
export PROFILE
export ORLIX_KERNEL_CC="$clang"
export ORLIX_KERNEL_HOSTCC="$hostcc"
export ORLIX_KERNEL_HOST_SDKROOT="$sdkroot"
export ORLIX_KERNEL_ARCHIVE_PLATFORMS
export DEVELOPER_DIR
cd "$exec_root"
env -u MAKEFLAGS -u MFLAGS -u GNUMAKEFLAGS \
    -u IPHONEOS_DEPLOYMENT_TARGET -u TVOS_DEPLOYMENT_TARGET -u WATCHOS_DEPLOYMENT_TARGET \
    SDKROOT="$sdkroot" \
    ORLIX_BUILD_ROOT="$work" \
    ORLIX_KERNEL_PORT_PREPARED=1 \
    ORLIX_KERNEL_KUNIT=0 \
    ORLIX_OS_LINUX_PAGE_SIZE=16384 \
    ORLIX_COMPILER_LAUNCHER= \
    PROFILE="$PROFILE" \
    ORLIX_KERNEL_CC="$clang" \
    ORLIX_KERNEL_HOSTCC="$hostcc" \
    ORLIX_KERNEL_HOST_SDKROOT="$sdkroot" \
    ORLIX_KERNEL_ARCHIVE_PLATFORMS="$ORLIX_KERNEL_ARCHIVE_PLATFORMS" \
    "$gmake" -f OrlixKernel/Sources/ports/orlix/kbuild/kernel-rules.mk __kernel-archive
built="$work/OrlixKernel/$PROFILE/$ORLIX_KERNEL_ARCHIVE_PLATFORMS/OrlixKernel.a"
test -s "$built"
/bin/mkdir -p "$boot_resources/orlix/boot/dts"
for dtb in release development; do
  source_dtb="$work/OrlixKernel/build/$PROFILE/arch/orlix/boot/dts/$dtb.dtb"
  test -s "$source_dtb"
  /bin/cp "$source_dtb" "$boot_resources/orlix/boot/dts/$dtb.dtb"
done
nm_cmd="$(/usr/bin/command -v llvm-nm)"
if [ -z "$nm_cmd" ]; then nm_cmd="$(/usr/bin/command -v nm)"; fi
test -n "$nm_cmd"
"$nm_cmd" -gU "$built" > "$symbols_out"
/usr/bin/grep -E '[[:space:]]T[[:space:]]+_arch_boot_entry' "$symbols_out" >/dev/null || {
  echo "Mach-O OrlixKernel.a missing defined _arch_boot_entry" >&2
  /usr/bin/grep arch_boot "$symbols_out" >&2 || true
  exit 1
}
/bin/cp "$built" "$archive_out"
digest="$(/usr/bin/shasum -a 256 "$archive_out" | /usr/bin/awk '{print $1}')"
/usr/bin/printf '%s\n' "$digest" > "$digest_out"
/usr/bin/printf '%s\n' '{' \
  '  "component": "OrlixKernel",' \
  '  "format": "mach-o-static-archive",' \
  '  "platform": "'"$ORLIX_KERNEL_ARCHIVE_PLATFORMS"'",' \
  '  "profile": "'"$PROFILE"'",' \
  '  "linked_symbol": "_arch_boot_entry",' \
  '  "wrapper_makefile": false,' \
  '  "archive_digest": "'"$digest"'"' \
  '}' > "$manifest_out"
""",
        arguments = [
            ctx.file.linux_makefile.path,
            archive.path,
            symbols.path,
            digest.path,
            manifest.path,
            str(len(overlay_files)),
            str(len(patch_files)),
            str(len(config_files)),
            str(len(engine_files)),
            str(len(extra_files)),
            boot_resources.path,
        ] + [f.path for f in overlay_files] + [f.path for f in patch_files] + [f.path for f in config_files] + [f.path for f in engine_files] + [f.path for f in extra_files],
        inputs = depset(
            direct = [ctx.file.linux_makefile] + overlay_files + patch_files + config_files + engine_files + extra_files,
            transitive = [ctx.attr.linux_source[DefaultInfo].files],
        ),
        outputs = [archive, symbols, digest, manifest, boot_resources],
        env = _pinned_env(ctx),
        use_default_shell_env = True,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-sandbox": "1"},
    )
    return [
        DefaultInfo(files = depset([archive, symbols, digest, manifest, boot_resources])),
        OutputGroupInfo(
            archive = depset([archive]),
            symbols = depset([symbols]),
            manifest = depset([manifest]),
            boot_resources = depset([boot_resources]),
        ),
        OrlixLinuxArchiveInfo(
            archive = archive,
            build_manifest = manifest,
            destination = ctx.attr.destination,
            profile = ctx.attr.profile,
            source_input_digest = digest,
            symbol_manifest = symbols,
        ),
    ]

orlix_kernel_macho_archive = rule(
    implementation = _kernel_macho_impl,
    attrs = {
        "profile": attr.string(mandatory = True, values = ["release", "development"]),
        "destination": attr.string(mandatory = True, values = ["iphoneos", "iphonesimulator"]),
        "linux_source": attr.label(mandatory = True),
        "linux_makefile": attr.label(allow_single_file = True, mandatory = True),
        "overlay": attr.label(mandatory = True, allow_files = True),
        "patches": attr.label(mandatory = True, allow_files = True),
        "configs": attr.label(mandatory = True, allow_files = True),
        "kbuild_engine": attr.label(mandatory = True, allow_files = True),
        "extra_inputs": attr.label_list(allow_files = True),
    },
)

def orlix_kernel_macho_library(name, archive, **kwargs):
    cc_import(
        name = name,
        static_library = archive,
        alwayslink = True,
        **kwargs
    )
