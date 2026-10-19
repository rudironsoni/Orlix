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
        "PATH": "/usr/bin:/bin",
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
    ccache_disable = shell.get("CCACHE_DISABLE")
    if ccache_disable:
        env["CCACHE_DISABLE"] = ccache_disable
    if "ORLIX_COMPILER_LAUNCHER" in shell:
        env["ORLIX_COMPILER_LAUNCHER"] = shell["ORLIX_COMPILER_LAUNCHER"]
    if "ORLIX_KERNEL_INCREMENTAL" in shell:
        env["ORLIX_KERNEL_INCREMENTAL"] = shell["ORLIX_KERNEL_INCREMENTAL"]
    return env

def _tcti_isa_prepare_impl(ctx):
    archive = ctx.file.archive
    pin = ctx.file.pin
    members_make = ctx.file.members_make
    members_def = ctx.file.members_def
    serializer = ctx.file.serializer
    tree = ctx.actions.declare_directory(ctx.label.name + "/tree")
    members = ctx.actions.declare_file(ctx.label.name + "/members.txt")
    identity = ctx.actions.declare_directory(ctx.label.name + "/tool-identity")
    if not ctx.files.tool_libs:
        fail("ISA restore requires staged tool libraries")
    if not ctx.files.python_home:
        fail("ISA restore requires the staged Python home")
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
gmake="$exec_root/$7"
awk_bin="$exec_root/$8"
python_bin="$exec_root/$9"
printf_bin="$exec_root/${10}"
members="$exec_root/${11}"
identity="$exec_root/${12}"
tool_bin="${gmake%/*}"
tool_root="${tool_bin%/*}"
tool_lib="$tool_root/lib"
python_home="$tool_root/python-home"
export PATH="$tool_bin"
export DYLD_LIBRARY_PATH="$tool_lib"
export PYTHONHOME="$python_home"
export PYTHONDONTWRITEBYTECODE=1
export PYTHONPATH="$exec_root:$exec_root/OrlixKernel/Sources/ports/orlix/kbuild"
test -s "$members_make"
test -s "$members_def"
test -s "$serializer"
test -x "$gmake"
test -x "$awk_bin"
test -x "$python_bin"
test -x "$printf_bin"
test -d "$tool_lib"
test -d "$python_home"
"$python_bin" -B -c 'from pathlib import Path; import source_state,sys; source_state.resume(Path(sys.argv[1]), [Path(p) for p in sys.argv[2:]])' "$identity" "$gmake" "$awk_bin" "$python_bin" "$printf_bin" "$tool_lib" "$python_home" "$members_make" "$members_def" "$serializer"
"$gmake" --no-print-directory --no-builtin-rules \
  -f "$members_make" ORLIX_TCTI_TARGET_REFRESH_ARTIFACTS_DECLARATION="$members_def" \
  __orlix-tcti-isa-archive-members > "$members"
"$python_bin" -B "$serializer" extract "$archive" "$pin" "$tree" "$members"
""",
        arguments = [
            archive.path,
            pin.path,
            members_make.path,
            members_def.path,
            serializer.path,
            tree.path,
            ctx.file.gmake.path,
            ctx.file.awk.path,
            ctx.file.python_bin.path,
            ctx.file.printf_bin.path,
            members.path,
            identity.path,
        ],
        inputs = [
            archive,
            pin,
            members_make,
            members_def,
            serializer,
            ctx.file.gmake,
            ctx.file.awk,
            ctx.file.python_bin,
            ctx.file.printf_bin,
            ctx.file.source_state,
            ctx.file.build_state,
        ] + ctx.files.tool_libs + ctx.files.python_home,
        outputs = [tree, members, identity],
        use_default_shell_env = False,
        # Staged gmake, awk, and Python, and the libraries and Python home
        # they open, are action inputs. resume() hashes those files.
        # The action still runs on the Mac.
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
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
        "gmake": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/gmake")),
        "awk": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/awk")),
        "python_bin": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/meson-python")),
        "printf_bin": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/printf")),
        "tool_libs": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:tool_libs")),
        "python_home": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:python_home")),
        "source_state": attr.label(allow_single_file = True, default = Label("//OrlixKernel/Sources:ports/orlix/kbuild/source_state.py")),
        "build_state": attr.label(allow_single_file = True, default = Label("//bazel:build_state.py")),
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
    isa_tree = ctx.file.isa_tree
    if not isa_tree.is_directory:
        fail("kernel Mach-O requires a prepared ISA tree artifact")
    sdk_marker = [f for f in ctx.files.macos_sdk if f.basename == "SDKSettings.json"]
    if len(sdk_marker) != 1:
        fail("kernel Mach-O archive requires the staged macOS SDK")
    if not ctx.files.tool_libs:
        fail("kernel Mach-O archive requires staged tool dylibs")
    if not ctx.files.guest_clang_support:
        fail("kernel Mach-O archive requires staged guest clang libraries")
    sdkroot = sdk_marker[0].dirname
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
isa_tree="$exec_root/${10}"
release_dtb="$exec_root/${11}"
development_dtb="$exec_root/${12}"
product="$exec_root/${13}"
toolchain_identity="$exec_root/${14}"
compiler_identity="$exec_root/${15}"
shift 15
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
gmake_staged="$exec_root/$1"
sed_staged="$exec_root/$2"
llvm_ar_staged="$exec_root/$3"
lld_staged="$exec_root/$4"
llvm_nm_staged="$exec_root/$5"
guest_clang_staged="$exec_root/$6"
ccache_staged="$exec_root/$7"
readlink_staged="$exec_root/$8"
find_staged="$exec_root/$9"
shift 9
host_clang="$exec_root/$1"
sdkroot="$exec_root/$2"
shift 2
tool_bin="$(/usr/bin/dirname "$gmake_staged")"
tool_root="$(/usr/bin/dirname "$tool_bin")"
tool_lib="$tool_root/lib"
guest_lib="$(/usr/bin/dirname "$(/usr/bin/dirname "$guest_clang_staged")")/lib"
export PATH="$tool_bin:/usr/bin:/bin"
export DYLD_LIBRARY_PATH="$tool_lib:$guest_lib"
for staged in "$gmake_staged" "$sed_staged" "$llvm_ar_staged" "$lld_staged" "$llvm_nm_staged" "$guest_clang_staged" "$ccache_staged" "$readlink_staged" "$find_staged" "$host_clang"; do
  test -s "$staged"
  test -x "$staged"
done
test -d "$tool_lib"
test -d "$guest_lib"
test -s "$sdkroot/SDKSettings.json"
test -s "$sdkroot/SDKSettings.plist"
bison_share="$tool_root/share/bison"
test -s "$bison_share/m4sugar/m4sugar.m4"
case "$linux_makefile" in
  *orlix_linux_source*/Makefile) ;;
  *) echo "kernel Mach-O must consume upstream Linux Makefile, got $linux_makefile" >&2; exit 1 ;;
esac
gmake="$gmake_staged"
clang="$guest_clang_staged"
linux_src="$(/usr/bin/dirname "$linux_makefile")"
ORLIX_KERNEL_INCREMENTAL="${ORLIX_KERNEL_INCREMENTAL:-1}"
if [ -n "${CCACHE_DISABLE:-}" ]; then
  export ORLIX_COMPILER_LAUNCHER=
elif [ -z "${CCACHE_DIR:-}" ]; then
  export CCACHE_DIR="${HOME}/Library/Caches/Orlix/ccache"
  /bin/mkdir -p "$CCACHE_DIR"
  export ORLIX_COMPILER_LAUNCHER="$ccache_staged"
else
  export ORLIX_COMPILER_LAUNCHER="$ccache_staged"
fi
export LINUX_MAKE="$gmake_staged"
export LINUX_SED="$sed_staged"
export LINUX_LLVM_BIN="$tool_bin"
export ORLIX_KERNEL_AR="$llvm_ar_staged"
export ORLIX_KERNEL_NM="$llvm_nm_staged"
work="$ORLIX_KERNEL_WORK_ROOT"
/bin/mkdir -p "$work"
export PYTHONPATH="$exec_root:$exec_root/OrlixKernel/Sources/ports/orlix/kbuild"
provenance="$exec_root/make/tcti-proof-registry-provenance.mk"
/usr/bin/python3 -c 'from pathlib import Path; import source_state,sys; source_state.resume(Path(sys.argv[1]), [Path(p) for p in sys.argv[2:]])' "$work" "$toolchain_identity" "$0" "${engine_paths[@]}" "$gmake_staged" "$sed_staged" "$llvm_ar_staged" "$lld_staged" "$llvm_nm_staged" "$guest_clang_staged" "$ccache_staged" "$readlink_staged" "$find_staged" "$host_clang" "$provenance" "$tool_lib" "$guest_lib" "$sdkroot" "$bison_share"
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
/usr/bin/printf '%s\n' '#!/bin/bash' "exec \"$host_clang\" -isysroot \"$sdkroot\" \"\$@\"" > "$hostcc"
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
test -s make/tcti-proof-registry-provenance.mk
kbuild_log="$work/kbuild-make.log"
kbuild_err="$work/kbuild-compiler.log"
: > "$kbuild_log"
: > "$kbuild_err"
set +e
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
    "$gmake" -f OrlixKernel/Sources/ports/orlix/kbuild/kernel-rules.mk __kernel-archive \
    >"$kbuild_log" 2>"$kbuild_err"
kbuild_status=$?
set -e
if [ "$kbuild_status" -ne 0 ]; then
  /bin/cat "$kbuild_log" >&2
  /bin/cat "$kbuild_err" >&2
  exit "$kbuild_status"
fi
/usr/bin/grep -v -E '^[[:space:]]*ORLIXCC[[:space:]]' "$kbuild_err" >&2 || true
/bin/cat "$kbuild_log"
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
nm_cmd="$llvm_nm_staged"
test -x "$nm_cmd"
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
            isa_tree.path,
            release_dtb.path,
            development_dtb.path,
            product.path,
            ctx.file.toolchain_identity.path,
            ctx.file.compiler_identity.path,
        ] + [f.path for f in overlay_files] + [f.path for f in patch_files] + [f.path for f in config_files] + [f.path for f in engine_files] + [
            ctx.file.gmake.path,
            ctx.file.gnu_sed.path,
            ctx.file.llvm_ar.path,
            ctx.file.ld_lld.path,
            ctx.file.llvm_nm.path,
            ctx.file.guest_clang.path,
            ctx.file.ccache.path,
            ctx.file.gnu_readlink.path,
            ctx.file.gnu_find.path,
            ctx.file.host_clang.path,
            sdkroot,
        ],
        inputs = depset(
            direct = [
                script,
                ctx.file.linux_makefile,
                isa_tree,
                ctx.file.toolchain_identity,
                ctx.file.compiler_identity,
                ctx.file.provenance_makefile,
                ctx.file.gmake,
                ctx.file.gnu_sed,
                ctx.file.llvm_ar,
                ctx.file.ld_lld,
                ctx.file.llvm_nm,
                ctx.file.guest_clang,
                ctx.file.ccache,
                ctx.file.gnu_readlink,
                ctx.file.gnu_find,
                ctx.file.host_clang,
            ] + overlay_files + patch_files + config_files + engine_files + ctx.files.tool_libs + ctx.files.guest_clang_support + ctx.files.clang_resources + ctx.files.macos_sdk + ctx.files.bison_share + [ctx.file.sdk_settings_plist],
            transitive = [ctx.attr.linux_source[DefaultInfo].files],
        ),
        outputs = [archive, symbols, digest, manifest, release_dtb, development_dtb, product],
        env = _pinned_env(ctx),
        use_default_shell_env = False,
        # Staged tools, their dylibs, the macOS SDK slice, and the provenance
        # makefile are action inputs. Output-base Kbuild state is not an output.
        # The action still runs on the Mac, outside remote execution.
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-sandbox": "1"},
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
        "isa_tree": attr.label(allow_single_file = True, mandatory = True),
        "toolchain_identity": attr.label(allow_single_file = True, mandatory = True),
        "compiler_identity": attr.label(allow_single_file = True, mandatory = True),
        "gmake": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/gmake")),
        "gnu_sed": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/sed")),
        "llvm_ar": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/llvm-ar")),
        "ld_lld": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/ld.lld")),
        "llvm_nm": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/llvm-nm")),
        "guest_clang": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/llvm/bin/clang")),
        "ccache": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/ccache")),
        "gnu_readlink": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/readlink")),
        "gnu_find": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/find")),
        "host_clang": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/clang")),
        "tool_libs": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:tool_libs")),
        "guest_clang_support": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:guest_clang_support")),
        "clang_resources": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:clang_resources")),
        "macos_sdk": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:macos_sdk")),
        "sdk_settings_plist": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/sdk/SDKSettings.plist")),
        "bison_share": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:bison_share")),
        "provenance_makefile": attr.label(
            allow_single_file = True,
            default = Label("//:make/tcti-proof-registry-provenance.mk"),
        ),
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
