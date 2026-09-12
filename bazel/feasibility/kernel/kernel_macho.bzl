"""Mach-O OrlixKernel.a from Kbuild product compile. Does not invoke OrlixKernel/Makefile."""

load("//bazel:artifact_identity.bzl", "declare_artifact_identity")
load("@rules_cc//cc:cc_import.bzl", "cc_import")
load("//bazel/providers:kernel_info.bzl", "OrlixLinuxArchiveInfo")

def _pinned_env(ctx):
    shell = ctx.configuration.default_shell_env
    developer_dir = shell.get("ORLIX_PINNED_DEVELOPER_DIR")
    if not developer_dir:
        fail("kernel Mach-O archive requires action_env ORLIX_PINNED_DEVELOPER_DIR")
    env = {
        "DEVELOPER_DIR": developer_dir,
        "HOME": "/var/empty",
        "PATH": "/opt/homebrew/opt/gnu-sed/libexec/gnubin:/opt/homebrew/opt/coreutils/libexec/gnubin:/opt/homebrew/opt/findutils/libexec/gnubin:/opt/homebrew/opt/lld/bin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/bin:/bin",
        "ORLIX_KERNEL_PORT_PREPARED": "1",
        "ORLIX_KERNEL_KUNIT": "0",
        "ORLIX_OS_LINUX_PAGE_SIZE": "16384",
        "CCACHE_MAXSIZE": "20G",
        "CCACHE_COMPILERCHECK": "content",
        "CCACHE_CONFIGPATH": "/dev/null",
        "PYTHONDONTWRITEBYTECODE": "1",
        "PROFILE": ctx.attr.profile,
        "ORLIX_KERNEL_ARCHIVE_PLATFORMS": ctx.attr.destination,
    }
    tmpdir = shell.get("TMPDIR")
    if tmpdir:
        env["TMPDIR"] = tmpdir
    return env

def _tcti_isa_prepare_impl(ctx):
    archive = ctx.file.archive
    pin = ctx.file.pin
    members_make = ctx.file.members_make
    members_def = ctx.file.members_def
    serializer = ctx.file.serializer
    tree = ctx.actions.declare_directory(ctx.label.name + "/tree")
    ctx.actions.run_shell(
        mnemonic = "OrlixTctiIsaRestore",
        progress_message = "Verifying and extracting pinned OrlixTCTI ISA tables",
        command = r"""
set -euo pipefail
exec_root="$PWD"
archive="$exec_root/$1"
pin="$exec_root/$2"
members_make="$exec_root/$3"
members_def="$exec_root/$4"
serializer="$exec_root/$5"
tree="$exec_root/$6"
temporary="$(/usr/bin/mktemp -d "/tmp/orlix-tcti-isa-prepare.XXXXXX")"
trap '/bin/rm -rf "$temporary"' EXIT
test -s "$members_make"
test -s "$members_def"
test -s "$serializer"
/opt/homebrew/bin/gmake --no-print-directory --no-builtin-rules \
  -f "$members_make" ORLIX_TCTI_TARGET_REFRESH_ARTIFACTS_DECLARATION="$members_def" \
  __orlix-tcti-isa-archive-members > "$temporary/members"
/usr/bin/python3 "$serializer" extract "$archive" "$pin" "$tree" "$temporary/members"
""",
        arguments = [
            archive.path,
            pin.path,
            members_make.path,
            members_def.path,
            serializer.path,
            tree.path,
        ],
        inputs = [archive, pin, members_make, members_def, serializer],
        outputs = [tree],
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-remote-cache": "1"},
    )
    return [DefaultInfo(files = depset([tree]))]

orlix_tcti_isa_prepare = rule(
    implementation = _tcti_isa_prepare_impl,
    attrs = {
        "archive": attr.label(allow_single_file = True, mandatory = True),
        "pin": attr.label(allow_single_file = True, mandatory = True),
        "members_make": attr.label(allow_single_file = True, mandatory = True),
        "members_def": attr.label(allow_single_file = True, mandatory = True),
        "serializer": attr.label(allow_single_file = True, mandatory = True),
    },
)

def _kernel_macho_impl(ctx):
    archive = ctx.actions.declare_file(ctx.label.name + "/OrlixKernel.a")
    symbols = ctx.actions.declare_file(ctx.label.name + "/symbols.txt")
    digest = ctx.actions.declare_file(ctx.label.name + "/archive.sha256")
    manifest = ctx.actions.declare_file(ctx.label.name + "/manifest.json")
    release_dtb = ctx.actions.declare_file(ctx.label.name + "/arch/orlix/boot/dts/release.dtb")
    development_dtb = ctx.actions.declare_file(ctx.label.name + "/arch/orlix/boot/dts/development.dtb")
    product = ctx.actions.declare_directory(ctx.label.name + "/product")
    overlay_files = ctx.files.overlay
    patch_files = ctx.files.patches
    config_files = ctx.files.configs
    engine_files = ctx.files.kbuild_engine
    extra_files = ctx.files.extra_inputs
    isa_tree = ctx.file.isa_tree
    if not isa_tree.is_directory:
        fail("kernel Mach-O requires a prepared ISA tree artifact")
    script = ctx.actions.declare_file(ctx.label.name + ".sh")
    ctx.actions.write(script, r"""
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
isa_tree="$exec_root/${11}"
release_dtb="$exec_root/${12}"
development_dtb="$exec_root/${13}"
product="$exec_root/${14}"
toolchain_identity="$exec_root/${15}"
compiler_identity="$exec_root/${16}"
shift 16
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
export ORLIX_COMPILER_LAUNCHER="${ORLIX_COMPILER_LAUNCHER-/opt/homebrew/bin/ccache}"
ORLIX_KERNEL_INCREMENTAL="${ORLIX_KERNEL_INCREMENTAL:-1}"
if [ -n "$ORLIX_COMPILER_LAUNCHER" ]; then
  test -n "${CCACHE_DIR:-}" || { echo "CCACHE_DIR is required; use the repository Make interface" >&2; exit 1; }
fi
work="$ORLIX_KERNEL_WORK_ROOT"
/bin/mkdir -p "$work"
export PYTHONPATH="$exec_root:$exec_root/OrlixKernel/Sources/ports/orlix/kbuild"
/usr/bin/python3 -c 'from pathlib import Path; import source_state,sys; source_state.resume(Path(sys.argv[1]), [Path(p) for p in sys.argv[2:]])' "$work" "$toolchain_identity" "$0" "${engine_paths[@]}"
port="$work/OrlixKernel/src/linux-6.12.105-port"
profile_config=""
for rel in "${config_paths[@]}"; do
  case "$rel" in
    */"${PROFILE}_defconfig") profile_config="$exec_root/$rel" ;;
  esac
done
test -n "$profile_config" && test -s "$profile_config"
/usr/bin/python3 -c 'from pathlib import Path; import source_state,sys; n=int(sys.argv[6]); result=source_state.prepare(Path(sys.argv[1]), Path(sys.argv[2]), [Path(p) for p in sys.argv[7:7+n]], [Path(p) for p in sys.argv[7+n:]], Path(sys.argv[3]), Path(sys.argv[4]), sys.argv[5], Path(sys.argv[2]).parents[2]); output=Path(sys.argv[2]).parents[2]/"prepared-source.sha256"; output.unlink(missing_ok=True); output.write_text(result["sha256"])' "$linux_src" "$port" "$profile_config" "$isa_tree" "$PROFILE" "$overlay_count" "${overlay_paths[@]}" "${patch_paths[@]}"
isa_dest="$work/OrlixKernel/orlix-tcti-isa"
/usr/bin/python3 -c 'from pathlib import Path; import source_state,sys; source_state.sync(source_state._files(Path(sys.argv[1])), Path(sys.argv[2]), Path(sys.argv[2]).parents[1])' "$isa_tree" "$isa_dest"
hostcc="$(/usr/bin/mktemp "$work/.hostcc.XXXXXX")"
/usr/bin/printf '%s\n' '#!/bin/bash' "exec \"$xcode_clang\" -isysroot \"$sdkroot\" \"\$@\"" > "$hostcc"
/bin/chmod +x "$hostcc"
/bin/mv -f "$hostcc" "$work/hostcc"
hostcc="$work/hostcc"
export ORLIX_KERNEL_PREPARED_SOURCE_SHA256="$(/bin/cat "$work/prepared-source.sha256")"
export CCACHE_BASEDIR="$work"
export CCACHE_EXTRAFILES="$compiler_identity"
export ORLIX_BUILD_ROOT="$work"
export ORLIX_KERNEL_PORT_PREPARED=1
export ORLIX_KERNEL_KUNIT=0
export ORLIX_OS_LINUX_PAGE_SIZE=16384
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
    PROFILE="$PROFILE" \
    ORLIX_KERNEL_CC="$clang" \
    ORLIX_KERNEL_HOSTCC="$hostcc" \
    ORLIX_KERNEL_HOST_SDKROOT="$sdkroot" \
    ORLIX_KERNEL_ARCHIVE_PLATFORMS="$ORLIX_KERNEL_ARCHIVE_PLATFORMS" \
    "$gmake" -f OrlixKernel/Sources/ports/orlix/kbuild/kernel-rules.mk __kernel-archive
/usr/bin/python3 -c 'from pathlib import Path; import source_state,sys; source_state.record(Path(sys.argv[1]))' "$work"
built="$work/OrlixKernel/$PROFILE/$ORLIX_KERNEL_ARCHIVE_PLATFORMS/OrlixKernel.a"
test -s "$built"
/bin/mkdir -p "$(/usr/bin/dirname "$release_dtb")" "$(/usr/bin/dirname "$development_dtb")" "$product/arch/orlix/boot/dts"
for dtb in release development; do
  source_dtb="$work/OrlixKernel/build/$PROFILE/arch/orlix/boot/dts/$dtb.dtb"
  test -s "$source_dtb"
  if [ "$dtb" = release ]; then
    /bin/cp "$source_dtb" "$release_dtb"
  else
    /bin/cp "$source_dtb" "$development_dtb"
  fi
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
/bin/cp "$archive_out" "$product/OrlixKernel.a"
/bin/cp "$release_dtb" "$product/arch/orlix/boot/dts/release.dtb"
/bin/cp "$development_dtb" "$product/arch/orlix/boot/dts/development.dtb"
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
""")
    ctx.actions.run_shell(
        mnemonic = "OrlixKernelMachOArchive",
        progress_message = "Compiling Mach-O OrlixKernel.a from prepared Linux sources",
        command = "PYTHONPATH=.:OrlixKernel/Sources/ports/orlix/kbuild /usr/bin/python3 -B -c 'import source_state,sys; raise SystemExit(source_state.run_locked(sys.argv[1],sys.argv[2:]))' \"$@\"",
        arguments = [
            script.path,
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
            isa_tree.path,
            release_dtb.path,
            development_dtb.path,
            product.path,
            ctx.file.toolchain_identity.path,
            ctx.file.compiler_identity.path,
        ] + [f.path for f in overlay_files] + [f.path for f in patch_files] + [f.path for f in config_files] + [f.path for f in engine_files] + [f.path for f in extra_files],
        inputs = depset(
            direct = [script, ctx.file.linux_makefile, isa_tree, ctx.file.toolchain_identity, ctx.file.compiler_identity] + overlay_files + patch_files + config_files + engine_files + extra_files,
            transitive = [ctx.attr.linux_source[DefaultInfo].files],
        ),
        outputs = [archive, symbols, digest, manifest, release_dtb, development_dtb, product],
        env = _pinned_env(ctx),
        use_default_shell_env = True,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-remote-cache": "1", "no-sandbox": "1"},
    )
    artifact_identity = declare_artifact_identity(
        ctx,
        "kernel",
        ctx.file._artifact_identity_serializer,
        artifacts = {
            "OrlixKernel.a": archive,
            "arch/orlix/boot/dts/development.dtb": development_dtb,
            "arch/orlix/boot/dts/release.dtb": release_dtb,
        },
    )
    return [
        DefaultInfo(files = depset([
            archive,
            symbols,
            digest,
            manifest,
            release_dtb,
            development_dtb,
            product,
            artifact_identity.manifest,
            artifact_identity.digest,
        ])),
        OutputGroupInfo(
            archive = depset([archive]),
            symbols = depset([symbols]),
            manifest = depset([manifest]),
            product = depset([product]),
            boot_resources = depset([release_dtb, development_dtb]),
            artifact_identity = depset([artifact_identity.manifest, artifact_identity.digest]),
            artifact_identity_manifest = depset([artifact_identity.manifest]),
            artifact_identity_digest = depset([artifact_identity.digest]),
        ),
        OrlixLinuxArchiveInfo(
            archive = archive,
            build_manifest = manifest,
            product = product,
            artifact_identity_digest = artifact_identity.digest,
            artifact_identity_manifest = artifact_identity.manifest,
            boot_resources = depset([release_dtb, development_dtb]),
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
        "isa_tree": attr.label(allow_single_file = True, mandatory = True),
        "toolchain_identity": attr.label(allow_single_file = True, mandatory = True),
        "compiler_identity": attr.label(allow_single_file = True, mandatory = True),
        "_artifact_identity_serializer": attr.label(
            allow_single_file = True,
            default = Label("//bazel:content_digest.py"),
        ),
    },
)

def orlix_kernel_macho_library(name, archive, **kwargs):
    cc_import(
        name = name,
        static_library = archive,
        alwayslink = True,
        **kwargs
    )
