"""Pinned Autotools guest packages from OrlixMLibC sysroot and UAPI."""

load("//bazel/providers:kernel_info.bzl", "OrlixInstalledUapiInfo")
load("//bazel/providers:package_info.bzl", "OrlixPackageTreeInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")

def _shell_squote(value):
    return "'" + value.replace("'", "'\\''") + "'"

def _pinned_env(ctx):
    shell = ctx.configuration.default_shell_env
    developer_dir = shell.get("ORLIX_PINNED_DEVELOPER_DIR")
    if not developer_dir:
        fail("autotools package requires action_env ORLIX_PINNED_DEVELOPER_DIR")
    env = {
        "DEVELOPER_DIR": developer_dir,
        "HOME": "/var/empty",
        "LC_ALL": "C",
        "PYTHONDONTWRITEBYTECODE": "1",
        "CCACHE_CONFIGPATH": "/dev/null",
        "CCACHE_COMPILERCHECK": "content",
        "CCACHE_MAXSIZE": "20G",
        "PATH": "/opt/homebrew/opt/bison/bin:/opt/homebrew/opt/coreutils/libexec/gnubin:/opt/homebrew/opt/lld/bin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/bin:/bin",
    }
    tmpdir = shell.get("TMPDIR")
    if tmpdir:
        env["TMPDIR"] = tmpdir
    return env

def _autotools_package_impl(ctx):
    sysroot = ctx.attr.sysroot[OrlixLibcSysrootInfo]
    uapi = ctx.attr.uapi[OrlixInstalledUapiInfo]
    extra = None
    if ctx.attr.extra_sysroot:
        extra = ctx.attr.extra_sysroot[OrlixPackageTreeInfo]
    extra_input = None
    if extra:
        extra_input = ctx.actions.declare_directory(ctx.label.name + ".dependency")
        ctx.actions.run_shell(
            mnemonic = "OrlixPackageInterface",
            command = r"""
set -euo pipefail
source="$1"
output="$2"
shift 2
/bin/mkdir -p "$output/usr/include"
/bin/cp -R "$source/usr/include/." "$output/usr/include/"
for archive in "$@"; do
  test -s "$source/$archive"
  /bin/mkdir -p "$output/$(/usr/bin/dirname "$archive")"
  /bin/cp "$source/$archive" "$output/$archive"
done
""",
            arguments = [extra.install_tree.path, extra_input.path] + ctx.attr.extra_archives + ctx.attr.extra_prerequisites,
            inputs = [extra.install_tree, ctx.file.toolchain_identity],
            outputs = [extra_input],
            execution_requirements = {"block-network": "1", "no-remote-cache": "1", "no-remote-exec": "1"},
        )
    install_tree = ctx.actions.declare_directory(ctx.label.name + "/install")
    file_manifest = ctx.actions.declare_file(ctx.label.name + "/file-manifest.txt")
    license_manifest = ctx.actions.declare_file(ctx.label.name + "/license-manifest.txt")
    metadata = ctx.actions.declare_file(ctx.label.name + "/package-metadata.txt")
    digest = ctx.actions.declare_file(ctx.label.name + "/source-input.sha256")
    recipe = ctx.actions.declare_file(ctx.label.name + ".recipe.env")
    script = ctx.actions.declare_file(ctx.label.name + ".sh")
    ctx.actions.write(
        recipe,
        "\n".join([
            "PACKAGE_NAME=%s" % _shell_squote(ctx.attr.package_name),
            "PACKAGE_VERSION=%s" % _shell_squote(ctx.attr.package_version),
            "LINK_MODE=%s" % _shell_squote(ctx.attr.link_mode),
            "IN_TREE=%s" % _shell_squote("1" if ctx.attr.in_tree else "0"),
            "FILE_EXPECT=%s" % _shell_squote(ctx.attr.file_expect),
            "REQUIRE_STATIC=%s" % _shell_squote("1" if ctx.attr.require_static else "0"),
            "CONFIGURE_ARGS=%s" % _shell_squote(" ".join(ctx.attr.configure_args)),
            "MAKE_STEPS=%s" % _shell_squote("|".join(ctx.attr.make_steps)),
            "INSTALL_MAP=%s" % _shell_squote(",".join(ctx.attr.install_map)),
            "CACHE_VARS=%s" % _shell_squote(";".join(ctx.attr.cache_vars)),
            "CPPFLAGS_EXTRA=%s" % _shell_squote(ctx.attr.cppflags),
            "CFLAGS_EXTRA=%s" % _shell_squote(ctx.attr.cflags),
            "EXTRA_ARCHIVES=%s" % _shell_squote(",".join(ctx.attr.extra_archives)),
            "EXTRA_PREREQUISITES=%s" % _shell_squote(",".join(ctx.attr.extra_prerequisites)),
            "SYSROOT_ARCHIVES=%s" % _shell_squote(",".join(ctx.attr.sysroot_archives)),
            "VERIFY_FILES=%s" % _shell_squote(",".join(ctx.attr.verify_files)),
            "",
        ]),
    )
    ctx.actions.write(script, r"""
set -euo pipefail
exec_root="$PWD"
configure="$exec_root/$1"
headers="$exec_root/$2"
uapi_headers="$exec_root/$3"
libraries="$exec_root/$4"
runtime="$exec_root/$5"
install_out="$exec_root/$6"
file_manifest="$exec_root/$7"
license_manifest="$exec_root/$8"
metadata="$exec_root/$9"
digest_out="$exec_root/${10}"
recipe="$exec_root/${11}"
extra_tree=""
if [ -n "${12:-}" ]; then extra_tree="$exec_root/${12}"; fi
# shellcheck disable=SC1090
. "$recipe"
case "$configure" in
  *OrlixOS/Sources/make*|*OrlixCoreUtils/Makefile*|*OrlixKernel/Makefile*|*OrlixMLibC/Makefile*)
    echo "$PACKAGE_NAME must not invoke wrapper Makefiles: $configure" >&2
    exit 1
    ;;
esac
clang="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
ar_bin="$(/usr/bin/command -v llvm-ar)"
ranlib_bin="$(/usr/bin/command -v llvm-ranlib)"
strip_bin="$(/usr/bin/command -v llvm-strip)"
test -x "$clang"
test -x "$ar_bin"
test -x "$ranlib_bin"
test -x "$strip_bin"
test -s "$libraries/libc.a"
test -s "$libraries/crt1.o"
test -s "$libraries/crti.o"
test -s "$libraries/crtn.o"
test -s "$runtime"
work="$ORLIX_PACKAGE_WORK_ROOT"
headers="$work/inputs/headers"
uapi_headers="$work/inputs/uapi"
libraries="$work/inputs/libraries"
runtime="$work/inputs/runtime/libcompiler_rt.a"
if [ -n "$extra_tree" ]; then extra_tree="$work/inputs/extra"; fi
/bin/mkdir -p "$work/build" "$work/dest" "$work/toolchain"
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
export MAKE=/opt/homebrew/bin/gmake
libm="$libraries/libc.a"
libpthread="$libraries/libc.a"
libssp=""
libssp_ns=""
[ -s "$libraries/libm.a" ] && libm="$libraries/libm.a"
[ -s "$libraries/libpthread.a" ] && libpthread="$libraries/libpthread.a"
[ -s "$libraries/libssp.a" ] && libssp="$libraries/libssp.a"
[ -s "$libraries/libssp_nonshared.a" ] && libssp_ns="$libraries/libssp_nonshared.a"
extra_inc=""
extra_libdir=""
extra_archives=""
if [ -n "$extra_tree" ]; then
  extra_inc="-I$extra_tree/usr/include"
  extra_libdir="-L$extra_tree/usr/lib"
  if [ -n "$EXTRA_ARCHIVES" ]; then
    IFS=,
    for rel in $EXTRA_ARCHIVES; do
      extra_archives="$extra_archives $extra_tree/$rel"
    done
    unset IFS
  fi
fi
code_model="-fPIE"
if [ "$LINK_MODE" = pie ]; then
  ld_mode="-static-pie -Wl,-z,max-page-size=0x4000"
elif [ "$LINK_MODE" = static_nopie ]; then
  code_model="-fno-pie"
  ld_mode="-static -no-pie -Wl,--no-dynamic-linker -Wl,--image-base=0x0000000100000000"
elif [ "$LINK_MODE" = static_wrapper ]; then
  code_model="-fPIC"
  ld_mode="-static -no-pie -Wl,--image-base=0x0000000100000000"
else
  code_model="-fno-pie"
  ld_mode="-static -Wl,--image-base=0x0000000100000000"
fi
CPPFLAGS_EXTRA="${CPPFLAGS_EXTRA//@SRC@/src}"
/usr/bin/printf '%s\n' '#!/bin/bash' 'set -euo pipefail' \
  "cc='$clang'" \
  "launcher='$launcher'" \
  "headers='$headers'" \
  "uapi_headers='$uapi_headers/include'" \
  "libraries='$libraries'" \
  "runtime='$runtime'" \
  "libm='$libm'" \
  "libpthread='$libpthread'" \
  "libssp='$libssp'" \
  "libssp_ns='$libssp_ns'" \
  "extra_inc='$extra_inc'" \
  "extra_libdir='$extra_libdir'" \
  "extra_archives='$extra_archives'" \
  "code_model='$code_model'" \
  "ld_mode='$ld_mode'" \
  "work='$work'" \
  'link=1' \
  'for arg in "$@"; do case "$arg" in -c|-E|-S) link=0 ;; esac; done' \
  'common=(--target=aarch64-linux-gnu -isystem "$headers" -isystem "$uapi_headers" $extra_inc -D_GNU_SOURCE -fhosted -fno-builtin -ffixed-x18 $code_model -ffile-prefix-map="$work"=.)' \
  'if [ "$link" -eq 1 ]; then' \
  '  exec "$cc" "${common[@]}" "$@" -fuse-ld=lld -nostdlib -Wl,--gc-sections $ld_mode $extra_libdir -L"$libraries" "$libraries/crt1.o" "$libraries/crti.o" -Wl,--start-group "$libraries/libc.a" "$libm" "$libpthread" "$libssp_ns" "$libssp" $extra_archives "$runtime" -Wl,--end-group "$libraries/crtn.o"' \
  'fi' \
  'exec ${launcher:+"$launcher"} "$cc" "${common[@]}" "$@"' > "$work/toolchain/aarch64-linux-gnu-gcc"
/bin/chmod +x "$work/toolchain/aarch64-linux-gnu-gcc"
export PATH="$work/toolchain:$PATH"
export CC=aarch64-linux-gnu-gcc
export CFLAGS="-O2 -D_FILE_OFFSET_BITS=64 -Wno-unknown-warning-option -Wno-incompatible-function-pointer-types -Wno-implicit-function-declaration -include limits.h -ffile-prefix-map=..=. -ffile-prefix-map=../..=. -ffile-prefix-map=../../..=. $CFLAGS_EXTRA"
export CPPFLAGS="$CPPFLAGS_EXTRA"
export LDFLAGS=""
export LIBS=""
export AR="$ar_bin"
export RANLIB="$ranlib_bin"
export STRIP="$strip_bin"
export CC_FOR_BUILD="/usr/bin/xcrun --sdk macosx cc -target arm64-apple-macosx"
export BUILD_CC="$CC_FOR_BUILD"
remaining="$CACHE_VARS"
while [ -n "$remaining" ]; do
  case "$remaining" in
    *\;*) item="${remaining%%;*}"; remaining="${remaining#*;}" ;;
    *) item="$remaining"; remaining="" ;;
  esac
  export "$item"
done
if [ "$IN_TREE" = 1 ]; then
  cd "$work/build"
  conf="./configure"
else
  cd "$work/build"
  conf="../src/configure"
fi
if [ "$(/bin/cat "$work/inputs/configure-required")" = 1 ]; then
  "$conf" $CONFIGURE_ARGS
fi
gmake_bin="$(/usr/bin/command -v gmake)"
test -x "$gmake_bin"
make_tools=(AUTOMAKE=/opt/homebrew/bin/automake ACLOCAL=/opt/homebrew/bin/aclocal AUTOCONF=/opt/homebrew/bin/autoconf AUTOHEADER=/opt/homebrew/bin/autoheader AUTOM4TE=/opt/homebrew/bin/autom4te AUTOPOINT=/opt/homebrew/bin/autopoint)
link_inputs="$libraries/libc.a $libm $libpthread $libssp_ns $libssp $runtime $extra_archives $libraries/crt1.o $libraries/crti.o $libraries/crtn.o"
if [ -n "$EXTRA_PREREQUISITES" ]; then
  IFS=,
  for archive in $EXTRA_PREREQUISITES; do
    test -s "$extra_tree/$archive"
    link_inputs="$link_inputs $extra_tree/$archive"
  done
  unset IFS
fi
if [ -n "$SYSROOT_ARCHIVES" ]; then
  IFS=,
  for archive in $SYSROOT_ARCHIVES; do
    test -s "$libraries/$archive"
    link_inputs="$link_inputs $libraries/$archive"
  done
  unset IFS
fi
: > "$work/toolchain/link-inputs.mk"
remaining="$INSTALL_MAP"
while [ -n "$remaining" ]; do
  case "$remaining" in
    *,*) mapping="${remaining%%,*}"; remaining="${remaining#*,}" ;;
    *) mapping="$remaining"; remaining="" ;;
  esac
  target="${mapping%%:*}"
  /usr/bin/printf '%s: %s\n' "${target##*/}" "$link_inputs" >> "$work/toolchain/link-inputs.mk"
done
export MAKEFILES="$work/toolchain/link-inputs.mk"
remaining="$MAKE_STEPS"
while [ -n "$remaining" ]; do
  case "$remaining" in
    *\|*) step="${remaining%%|*}"; remaining="${remaining#*|}" ;;
    *) step="$remaining"; remaining="" ;;
  esac
  step="${step//@DEST@/$work/dest}"
  case "$step" in
    *@USELIBS@*)
      step="${step//@USELIBS@/}"
      echo "orlix-autotools: $gmake_bin $step LIBS=<sysroot>" >&2
      # shellcheck disable=SC2086
      "$gmake_bin" "${make_tools[@]}" $step LIBS="$LIBS" || { echo "orlix-autotools: make failed $?" >&2; /bin/ls -la . .libs 2>/dev/null || true; exit 1; }
      ;;
    *)
      echo "orlix-autotools: $gmake_bin $step" >&2
      # shellcheck disable=SC2086
      "$gmake_bin" "${make_tools[@]}" $step || { echo "orlix-autotools: make failed $?" >&2; /bin/ls -la . .libs 2>/dev/null || true; exit 1; }
      ;;
  esac
done
remaining="$INSTALL_MAP"
while [ -n "$remaining" ]; do
  case "$remaining" in
    *,*) mapping="${remaining%%,*}"; remaining="${remaining#*,}" ;;
    *) mapping="$remaining"; remaining="" ;;
  esac
  src_rel="${mapping%%:*}"
  dest_rel="${mapping#*:}"
  base="$(/usr/bin/basename "$src_rel")"
  if [ -e ".libs/$base" ] && /usr/bin/file ".libs/$base" | /usr/bin/grep -F -q 'ELF 64-bit LSB'; then
    src_rel=".libs/$base"
  fi
  test -e "$src_rel" || { echo "missing built $PACKAGE_NAME file: $src_rel" >&2; /bin/ls -la . ".libs" 2>/dev/null || true; exit 1; }
  /bin/mkdir -p "$work/dest/$(/usr/bin/dirname "$dest_rel")"
  /usr/bin/install -m 0755 "$src_rel" "$work/dest/$dest_rel"
  echo "orlix-autotools: installed $src_rel -> $dest_rel $(/usr/bin/file "$work/dest/$dest_rel")" >&2
done
/usr/bin/find "$work/dest" -type f -perm -111 -print | while IFS= read -r bin; do
  info="$(/usr/bin/file "$bin")"
  /usr/bin/printf '%s\n' "$info" | /usr/bin/grep -F -q 'ELF 64-bit LSB' || continue
  "$strip_bin" "$bin"
  /usr/bin/printf '%s\n' "$info" | /usr/bin/grep -F -q 'ARM aarch64' || { /usr/bin/printf '%s\n' "$info" >&2; exit 1; }
  if [ -n "$FILE_EXPECT" ]; then
    /usr/bin/printf '%s\n' "$info" | /usr/bin/grep -F -q "$FILE_EXPECT" || { /usr/bin/printf '%s\n' "$info" >&2; exit 1; }
  fi
  if [ "$REQUIRE_STATIC" = 1 ]; then
    /usr/bin/printf '%s\n' "$info" | /usr/bin/grep -E -q 'statically linked|static-pie linked' || { /usr/bin/printf '%s\n' "$info" >&2; exit 1; }
  fi
done
remaining="$VERIFY_FILES"
while [ -n "$remaining" ]; do
  case "$remaining" in
    *,*) rel="${remaining%%,*}"; remaining="${remaining#*,}" ;;
    *) rel="$remaining"; remaining="" ;;
  esac
  test -e "$work/dest/$rel" || { echo "missing $PACKAGE_NAME output: $rel" >&2; exit 1; }
done
/bin/mkdir -p "$install_out"
/bin/cp -R "$work/dest/." "$install_out/"
(cd "$install_out" && /usr/bin/find . -type f -print | /usr/bin/sort) > "$file_manifest"
/usr/bin/printf '%s\n' 'license=GPL-compatible' > "$license_manifest"
/usr/bin/printf 'name=%s\nversion=%s\nengine=configure-make-destdir\n' "$PACKAGE_NAME" "$PACKAGE_VERSION" > "$metadata"
digest="$( ( cd "$install_out" && /usr/bin/find . -type f -print0 | /usr/bin/sort -z | /usr/bin/xargs -0 /usr/bin/shasum -a 256 ) | /usr/bin/shasum -a 256 | /usr/bin/awk '{print $1}' )"
/usr/bin/printf '%s\n' "$digest" > "$digest_out"
if [ -n "$launcher" ]; then "$launcher" --print-log-stats --format=json; fi
""".replace("__COMPILER_IDENTITY__", ctx.file.compiler_identity.path))
    ctx.actions.run_shell(
        mnemonic = "OrlixGuestPackage",
        progress_message = "Building %s with incremental upstream Make state" % ctx.attr.package_name,
        command = "PYTHONPATH=. /usr/bin/python3 -B -c 'from bazel.feasibility.packages import build_state; import sys; raise SystemExit(build_state.run(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[7:], configuration=sys.argv[5], in_tree=sys.argv[6] == \"1\", extra_tree=sys.argv[-1] or None))' \"$@\"",
        arguments = [
            ctx.attr.package_name,
            script.path,
            ctx.file.toolchain_identity.path,
            ctx.file.compiler_identity.path,
            recipe.path,
            "1" if ctx.attr.in_tree else "0",
            ctx.file.configure.path,
            sysroot.headers.path,
            uapi.headers.path,
            sysroot.libraries.path,
            sysroot.compiler_runtime.path,
            install_tree.path,
            file_manifest.path,
            license_manifest.path,
            metadata.path,
            digest.path,
            recipe.path,
            extra_input.path if extra_input else "",
        ],
        inputs = depset(
            direct = [
                script,
                ctx.file.toolchain_identity,
                ctx.file.compiler_identity,
                ctx.file.build_state,
                ctx.file.shared_state,
                ctx.file.configure,
                recipe,
                sysroot.headers,
                uapi.headers,
                sysroot.libraries,
                sysroot.compiler_runtime,
            ] + ctx.files.sources + ([extra_input] if extra_input else []),
        ),
        outputs = [install_tree, file_manifest, license_manifest, metadata, digest],
        env = _pinned_env(ctx),
        use_default_shell_env = True,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-remote-cache": "1", "no-sandbox": "1"},
    )
    return [
        DefaultInfo(files = depset([install_tree, file_manifest, license_manifest, metadata, digest])),
        OrlixPackageTreeInfo(
            dependency_digests = sysroot.consumed_uapi_digest,
            file_manifest = file_manifest,
            install_tree = install_tree,
            license_manifest = license_manifest,
            package_metadata = metadata,
            source_input_digest = digest,
        ),
    ]

orlix_autotools_package = rule(
    implementation = _autotools_package_impl,
    attrs = {
        "build_state": attr.label(allow_single_file = True, default = ":build_state.py"),
        "shared_state": attr.label(allow_single_file = True, default = "//bazel:build_state.py"),
        "toolchain_identity": attr.label(allow_single_file = True, default = "@orlix_kernel_toolchain//:autotools-identity.json"),
        "compiler_identity": attr.label(allow_single_file = True, default = "@orlix_kernel_toolchain//:guest-compiler-identity.json"),
        "sysroot": attr.label(mandatory = True, providers = [OrlixLibcSysrootInfo]),
        "uapi": attr.label(mandatory = True, providers = [OrlixInstalledUapiInfo]),
        "configure": attr.label(allow_single_file = True, mandatory = True),
        "sources": attr.label(allow_files = True, mandatory = True),
        "package_name": attr.string(mandatory = True),
        "package_version": attr.string(mandatory = True),
        "configure_args": attr.string_list(mandatory = True),
        "make_steps": attr.string_list(mandatory = True),
        "install_map": attr.string_list(),
        "cache_vars": attr.string_list(),
        "link_mode": attr.string(default = "pie"),
        "in_tree": attr.bool(default = False),
        "file_expect": attr.string(default = "ARM aarch64"),
        "require_static": attr.bool(default = False),
        "cppflags": attr.string(default = ""),
        "cflags": attr.string(default = ""),
        "extra_sysroot": attr.label(providers = [OrlixPackageTreeInfo]),
        "extra_archives": attr.string_list(),
        "extra_prerequisites": attr.string_list(),
        "sysroot_archives": attr.string_list(),
        "verify_files": attr.string_list(),
    },
)
