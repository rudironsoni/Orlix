"""Pinned Autotools guest packages from OrlixMLibC sysroot and UAPI."""

load(":action_inputs.bzl", "guest_configure_inputs")
load("//bazel:compiler_cache.bzl", "compiler_cache_env")
load("//bazel:artifact_identity.bzl", "declare_artifact_identity")
load("//bazel/providers:kernel_info.bzl", "OrlixInstalledUapiInfo")
load("//bazel/providers:package_info.bzl", "OrlixPackageTreeInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")

def _shell_squote(value):
    return "'" + value.replace("'", "'\\''") + "'"

def _pinned_env(ctx):
    shell = ctx.configuration.default_shell_env
    env = {
        "HOME": "/var/empty",
        "LC_ALL": "C",
        "PYTHONDONTWRITEBYTECODE": "1",
        "CCACHE_CONFIGPATH": "/dev/null",
        "CCACHE_COMPILERCHECK": "content",
        "CCACHE_MAXSIZE": "20G",
        "PATH": "/usr/bin:/bin",
    }
    if "ORLIX_PACKAGE_INCREMENTAL" in shell:
        env["ORLIX_PACKAGE_INCREMENTAL"] = shell["ORLIX_PACKAGE_INCREMENTAL"]
    # CCACHE_DIR is inherited from the client. It is not a fixed action env.
    env.update(compiler_cache_env(shell, False))
    return env

def declare_orlix_package_interface(ctx, package_trees, paths_by_package):
    if not package_trees:
        if paths_by_package:
            fail("package interface paths require package trees")
        return None
    packages = []
    names = {}
    for package in package_trees:
        tree = package[OrlixPackageTreeInfo]
        name = package.label.name
        if name in names:
            fail("duplicate package interface: %s" % name)
        names[name] = True
        packages.append((tree, name))
    for name in paths_by_package:
        if name not in names:
            fail("package interface has no tree: %s" % name)
        for path in paths_by_package[name]:
            if not path or path.startswith("/") or ".." in path.split("/"):
                fail("invalid package interface path: %s" % path)
    extra_input = ctx.actions.declare_directory(ctx.label.name + ".dependency")
    arguments = [extra_input.path]
    inputs = [ctx.file.toolchain_identity]
    for package, name in packages:
        paths = paths_by_package.get(name, [])
        arguments += [package.install_tree.path, str(len(paths))] + paths
        inputs.append(package.install_tree)
    ctx.actions.run_shell(
        mnemonic = "OrlixPackageInterface",
        command = r"""
set -euo pipefail
output="$1"
shift
/bin/mkdir -p "$output"
copy_path() {
  source="$1"
  destination="$2"
  relative="$3"
  input="$source/$relative"
  test -e "$input"
  /bin/mkdir -p "$destination/$(/usr/bin/dirname "$relative")"
  if test -e "$destination/$relative" || test -L "$destination/$relative"; then
    echo "duplicate package interface path: $relative" >&2
    exit 1
  fi
  if test -d "$input"; then
    /bin/cp -R -P "$input" "$destination/$(/usr/bin/dirname "$relative")/"
  else
    /bin/cp -P "$input" "$destination/$relative"
  fi
}
while test "$#" -gt 0; do
  source="$1"
  path_count="$2"
  shift 2
  while test "$path_count" -gt 0; do
    copy_path "$source" "$output" "$1"
    path_count=$((path_count - 1))
    shift
  done
done
""",
        arguments = arguments,
        inputs = inputs,
        outputs = [extra_input],
        env = {
            "HOME": "/var/empty",
            "PATH": "/usr/bin:/bin",
        },
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )
    return extra_input

def _share_inputs(files):
    selected = []
    for item in files:
        if "/share/" in item.path:
            selected.append(item)
    return selected

def _autotools_package_impl(ctx):
    if ctx.attr.bootstrap and not ctx.attr.in_tree:
        fail("autotools bootstrap requires in_tree=True")
    if ctx.attr.make_only and not ctx.attr.in_tree:
        fail("autotools make_only requires in_tree=True")
    if ctx.attr.make_only and ctx.attr.bootstrap:
        fail("autotools make_only cannot bootstrap")
    sysroot = ctx.attr.sysroot[OrlixLibcSysrootInfo]
    uapi = ctx.attr.uapi[OrlixInstalledUapiInfo]
    interface_packages = list(ctx.attr.interface_packages)
    interface_paths = {}
    for name, paths in ctx.attr.interface_paths.items():
        interface_paths[name] = paths
    extra_input = declare_orlix_package_interface(
        ctx,
        interface_packages,
        interface_paths,
    )
    install_tree = ctx.actions.declare_directory(ctx.label.name + "/install")
    file_manifest = ctx.actions.declare_file(ctx.label.name + "/file-manifest.txt")
    license_manifest = ctx.actions.declare_file(ctx.label.name + "/license-manifest.txt")
    metadata = ctx.actions.declare_file(ctx.label.name + "/package-metadata.txt")
    recipe = ctx.actions.declare_file(ctx.label.name + ".recipe.env")
    script = ctx.actions.declare_file(ctx.label.name + ".sh")
    ctx.actions.write(
        recipe,
        "\n".join([
            "PACKAGE_NAME=%s" % _shell_squote(ctx.attr.package_name),
            "PACKAGE_VERSION=%s" % _shell_squote(ctx.attr.package_version),
            "LINK_MODE=%s" % _shell_squote(ctx.attr.link_mode),
            "IN_TREE=%s" % _shell_squote("1" if ctx.attr.in_tree else "0"),
            "BOOTSTRAP=%s" % _shell_squote("1" if ctx.attr.bootstrap else "0"),
            "MAKE_ONLY=%s" % _shell_squote("1" if ctx.attr.make_only else "0"),
            "FILE_EXPECT=%s" % _shell_squote(ctx.attr.file_expect),
            "REQUIRE_STATIC=%s" % _shell_squote("1" if ctx.attr.require_static else "0"),
            "CONFIGURE_ARGS=%s" % _shell_squote(" ".join(ctx.attr.configure_args)),
            "MAKE_STEPS=%s" % _shell_squote("|".join(ctx.attr.make_steps)),
            "MAKE_VARS=%s" % _shell_squote(";".join(ctx.attr.make_vars)),
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
recipe="$exec_root/${10}"
extra_tree=""
if [ -n "${11:-}" ]; then extra_tree="$exec_root/${11}"; fi
# shellcheck disable=SC1090
. "$recipe"
case "$configure" in
  *OrlixOS/Sources/make*|*OrlixCoreUtils/Makefile*|*OrlixKernel/Makefile*|*OrlixMLibC/Makefile*)
    echo "$PACKAGE_NAME must not invoke wrapper Makefiles: $configure" >&2
    exit 1
    ;;
esac
tool_bin="${ORLIX_PACKAGE_TOOL_BIN:?}"
tool_root="$(/usr/bin/dirname "$tool_bin")"
clang="$tool_bin/clang"
ar_bin="$tool_bin/llvm-ar"
ranlib_bin="$tool_bin/llvm-ranlib"
strip_bin="$tool_bin/llvm-strip"
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
if [ -n "${CCACHE_DISABLE:-}" ] || [ ! -x "$tool_bin/ccache" ]; then
  launcher=""
elif [ -n "${CCACHE_DIR:-}" ]; then
  launcher="$tool_bin/ccache"
else
  launcher=""
fi
export ORLIX_COMPILER_LAUNCHER="$launcher"
export CCACHE_BASEDIR="$work"
export CCACHE_EXTRAFILES="$exec_root/__COMPILER_IDENTITY__"
export CCACHE_STATSLOG="$work/compiler-cache.log"
: > "$CCACHE_STATSLOG"
export MAKE="$tool_bin/gmake"
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
"$tool_bin/printf" '%s\n' '#!/bin/bash' 'set -euo pipefail' \
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
  "tool_bin='$tool_bin'" \
  'link=1' \
  'for arg in "$@"; do case "$arg" in -c|-E|-S) link=0 ;; esac; done' \
  'common=(--target=aarch64-linux-gnu -isystem "$headers" -isystem "$uapi_headers" $extra_inc -D_GNU_SOURCE -fhosted -fno-builtin -ffixed-x18 $code_model -ffile-prefix-map="$work"=. -ffile-prefix-map="$tool_bin"=/orlix/tools)' \
  'if [ "$link" -eq 1 ]; then' \
  '  exec "$cc" "${common[@]}" "$@" -fuse-ld=lld -nostdlib -Wl,--gc-sections $ld_mode $extra_libdir -L"$libraries" "$libraries/crt1.o" "$libraries/crti.o" -Wl,--start-group "$libraries/libc.a" "$libm" "$libpthread" "$libssp_ns" "$libssp" $extra_archives "$runtime" -Wl,--end-group "$libraries/crtn.o"' \
  'fi' \
  'exec ${launcher:+"$launcher"} "$cc" "${common[@]}" "$@"' > "$work/toolchain/aarch64-linux-gnu-gcc"
/bin/chmod +x "$work/toolchain/aarch64-linux-gnu-gcc"
# Staged tools/bin is first and clang is CC. Autotools reads share data from
# that prefix.
export PATH="$tool_bin:$work/toolchain:/usr/bin:/bin"
export DYLD_LIBRARY_PATH="$tool_root/lib:$tool_root/llvm/lib"
share="$tool_root/share"
if [ -d "$share/autoconf" ]; then
  export autom4te_perllibdir="$share/autoconf"
  export AC_MACRODIR="$share/autoconf"
fi
export CC="${launcher:+$launcher }$clang --target=aarch64-linux-gnu -isystem $headers -isystem $uapi_headers/include $extra_inc -D_GNU_SOURCE -fhosted -fno-builtin -ffixed-x18 $code_model -ffile-prefix-map=$work=. -ffile-prefix-map=$tool_bin=/orlix/tools"
export CFLAGS="-O2 -D_FILE_OFFSET_BITS=64 -Wno-unknown-warning-option -Wno-incompatible-function-pointer-types -Wno-implicit-function-declaration -include limits.h -ffile-prefix-map=..=. -ffile-prefix-map=../..=. -ffile-prefix-map=../../..=. $CFLAGS_EXTRA"
if [ "$MAKE_ONLY" = 1 ]; then
  export CFLAGS="$CFLAGS -MD -MP"
fi
export CPPFLAGS="$CPPFLAGS_EXTRA"
export LDFLAGS="-fuse-ld=lld -nostdlib -Wl,--gc-sections $ld_mode $extra_libdir -L$libraries $libraries/crt1.o $libraries/crti.o -Wl,--start-group $libraries/libc.a $libm $libpthread $libssp_ns $libssp $extra_archives $runtime -Wl,--end-group $libraries/crtn.o"
export LIBS=""
export AR="$ar_bin"
export RANLIB="$ranlib_bin"
export STRIP="$strip_bin"
export CC_FOR_BUILD="$clang -isysroot ${ORLIX_PACKAGE_SDK:?} -target arm64-apple-macosx -ffile-prefix-map=${ORLIX_PACKAGE_SDK}=/orlix/macos-sdk"
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
  if [ "$BOOTSTRAP" = 1 ]; then
    export LIBTOOLIZE="$tool_bin/glibtoolize"
    export M4="$tool_bin/m4"
    "$tool_bin/autoreconf" --force --install --verbose
  fi
  if [ "$MAKE_ONLY" != 1 ]; then
    maintainer_mode=""
    if [ "$BOOTSTRAP" != 1 ]; then
      case " $CONFIGURE_ARGS " in
        *" --disable-maintainer-mode "*) ;;
        *) maintainer_mode="--disable-maintainer-mode" ;;
      esac
    fi
    "$conf" $CONFIGURE_ARGS $maintainer_mode
    # jq compiles config.status --config into src/config_opts.inc. That make
    # rule has no prerequisites, so an existing file is not regenerated.
    # Rewrite the output-base prefixes the same way the compiler prefix maps
    # do, before that string is compiled. CC and LDFLAGS stay absolute so the
    # real compile still finds the toolchain.
    if [ -f Makefile ] && /usr/bin/grep -q 'src/config_opts.inc' Makefile; then
      /bin/mkdir -p src
      ./config.status --config | PYTHONPATH="$exec_root" /usr/bin/python3 -B -c 'from bazel.feasibility.packages.jq_config import write_config_opts; import sys; write_config_opts(sys.stdin.read(), sys.argv[1], sys.argv[2], "src/config_opts.inc")' "$work" "$tool_bin"
    fi
  fi
fi
gmake_bin="$tool_bin/gmake"
test -x "$gmake_bin"
make_vars=(AUTOMAKE="$tool_bin/automake" ACLOCAL="$tool_bin/aclocal" AUTOCONF="$tool_bin/autoconf" AUTOHEADER="$tool_bin/autoheader" AUTOM4TE="$tool_bin/autom4te" AUTOPOINT="$tool_bin/autopoint")
remaining="$MAKE_VARS"
while [ -n "$remaining" ]; do
  case "$remaining" in
    *\;*) item="${remaining%%;*}"; remaining="${remaining#*;}" ;;
    *) item="$remaining"; remaining="" ;;
  esac
  item="${item//@EXTRA@/$extra_tree}"
  make_vars+=("$item")
done
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
  case "$target" in
    *.a|*.h) ;;
    *) "$tool_bin/printf" '%s: private .EXTRA_PREREQS := %s\n' "${target##*/}" "$link_inputs" >> "$work/toolchain/link-inputs.mk" ;;
  esac
done
if [ "$MAKE_ONLY" = 1 ]; then
  "$tool_bin/printf" '%s\n' '%: %.c' '	$(LINK.c) $< $(LOADLIBES) $(LDLIBS) -o $@' '-include $(wildcard *.d)' >> "$work/toolchain/link-inputs.mk"
fi
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
      "$gmake_bin" "${make_vars[@]}" $step LIBS="$LIBS" || { echo "orlix-autotools: make failed $?" >&2; /bin/ls -la . .libs 2>/dev/null || true; exit 1; }
      ;;
    *)
      echo "orlix-autotools: $gmake_bin $step" >&2
      # shellcheck disable=SC2086
      "$gmake_bin" "${make_vars[@]}" $step || { echo "orlix-autotools: make failed $?" >&2; /bin/ls -la . .libs 2>/dev/null || true; exit 1; }
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
  base="$("$tool_bin/basename" "$src_rel")"
  if [ -e ".libs/$base" ] && /usr/bin/file ".libs/$base" | /usr/bin/grep -F -q 'ELF 64-bit LSB'; then
    src_rel=".libs/$base"
  fi
  test -e "$src_rel" || { echo "missing built $PACKAGE_NAME file: $src_rel" >&2; /bin/ls -la . ".libs" 2>/dev/null || true; exit 1; }
  /bin/mkdir -p "$work/dest/$(/usr/bin/dirname "$dest_rel")"
  mode=0644
  if [ -x "$src_rel" ] || /usr/bin/file "$src_rel" | /usr/bin/grep -F -q 'ELF 64-bit'; then mode=0755; fi
  "$tool_bin/install" -m "$mode" "$src_rel" "$work/dest/$dest_rel"
  echo "orlix-autotools: installed $src_rel -> $dest_rel $(/usr/bin/file "$work/dest/$dest_rel")" >&2
done
"$tool_bin/find" "$work/dest" -type f -perm -111 -print | while IFS= read -r bin; do
  info="$(/usr/bin/file "$bin")"
  "$tool_bin/printf" '%s\n' "$info" | /usr/bin/grep -F -q 'ELF 64-bit LSB' || continue
  "$strip_bin" "$bin"
  "$tool_bin/printf" '%s\n' "$info" | /usr/bin/grep -F -q 'ARM aarch64' || { "$tool_bin/printf" '%s\n' "$info" >&2; exit 1; }
  if [ -n "$FILE_EXPECT" ]; then
    "$tool_bin/printf" '%s\n' "$info" | /usr/bin/grep -F -q "$FILE_EXPECT" || { "$tool_bin/printf" '%s\n' "$info" >&2; exit 1; }
  fi
  if [ "$REQUIRE_STATIC" = 1 ]; then
    "$tool_bin/printf" '%s\n' "$info" | /usr/bin/grep -E -q 'statically linked|static-pie linked' || { "$tool_bin/printf" '%s\n' "$info" >&2; exit 1; }
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
(cd "$install_out" && "$tool_bin/find" . -type f -print | "$tool_bin/sort") > "$file_manifest"
"$tool_bin/printf" '%s\n' 'license=GPL-compatible' > "$license_manifest"
"$tool_bin/printf" 'name=%s\nversion=%s\nengine=configure-make-destdir\n' "$PACKAGE_NAME" "$PACKAGE_VERSION" > "$metadata"
if [ -n "$launcher" ]; then
  printf 'ccache_directory=%s\n' "$CCACHE_DIR"
  "$launcher" --print-log-stats --format=json
fi
""".replace("__COMPILER_IDENTITY__", ctx.file.compiler_identity.path))
    ctx.actions.run_shell(
        mnemonic = "OrlixGuestPackage",
        progress_message = "Building %s with incremental upstream Make state" % ctx.attr.package_name,
        command = "PYTHONPATH=. /usr/bin/python3 -B -c 'from bazel.feasibility.packages import build_state; import sys; raise SystemExit(build_state.run(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[9:], configuration=sys.argv[6], in_tree=sys.argv[7] == \"1\", make_only=sys.argv[8] == \"1\", extra_tree=sys.argv[-1] or None, tool_bin=sys.argv[5]))' \"$@\"",
        arguments = [
            ctx.attr.package_name,
            script.path,
            ctx.file.toolchain_identity.path,
            ctx.file.compiler_identity.path,
            ctx.file.gmake.path,
            recipe.path,
            "1" if ctx.attr.in_tree else "0",
            "1" if ctx.attr.make_only else "0",
            ctx.file.configure.path,
            sysroot.headers.path,
            uapi.headers.path,
            sysroot.libraries.path,
            sysroot.compiler_runtime.path,
            install_tree.path,
            file_manifest.path,
            license_manifest.path,
            metadata.path,
            recipe.path,
            extra_input.path if extra_input else "",
        ],
        inputs = depset(
            direct = guest_configure_inputs(ctx.attr.package_name, [
                script,
                ctx.file.toolchain_identity,
                ctx.file.compiler_identity,
                ctx.file.build_state,
                ctx.file.jq_config,
                ctx.file.shared_state,
                ctx.file.configure,
                recipe,
                sysroot.headers,
                uapi.headers,
                sysroot.libraries,
                sysroot.compiler_runtime,
                ctx.file.gmake,
            ] + ctx.files.sources + ctx.files.host_tools + ctx.files.tool_libs + ctx.files.clang_resources + ctx.files.guest_clang_support + ctx.files.macos_sdk + _share_inputs(ctx.files.host_tools) + ([extra_input] if extra_input else [])),
        ),
        outputs = [install_tree, file_manifest, license_manifest, metadata],
        env = _pinned_env(ctx),
        # Inherited CCACHE_DIR is the shared directory and is not an action key.
        use_default_shell_env = True,
        # Staged tools/bin provides clang and the autotools binaries. Their
        # dylibs, clang resources, macOS SDK, and share trees are inputs.
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-sandbox": "1"},
    )
    artifact_identity = declare_artifact_identity(
        ctx,
        "install",
        ctx.file._artifact_identity_serializer,
        root = install_tree,
    )
    return [
        DefaultInfo(files = depset([
            install_tree,
            file_manifest,
            license_manifest,
            metadata,
            artifact_identity.manifest,
            artifact_identity.digest,
        ])),
        OrlixPackageTreeInfo(
            artifact_identity_closure = depset(
                direct = [artifact_identity.digest],
                transitive = [package[OrlixPackageTreeInfo].artifact_identity_closure for package in ctx.attr.interface_packages],
            ),
            artifact_identity_digest = artifact_identity.digest,
            artifact_identity_manifest = artifact_identity.manifest,
            dependency_digests = sysroot.consumed_uapi_digest,
            file_manifest = file_manifest,
            install_tree = install_tree,
            license_manifest = license_manifest,
            package_metadata = metadata,
            source_input_digest = artifact_identity.digest,
        ),
    ]

orlix_autotools_package = rule(
    implementation = _autotools_package_impl,
    attrs = {
        "build_state": attr.label(allow_single_file = True, default = ":build_state.py"),
        "jq_config": attr.label(allow_single_file = True, default = ":jq_config.py"),
        "shared_state": attr.label(allow_single_file = True, default = "//bazel:build_state.py"),
        "toolchain_identity": attr.label(allow_single_file = True, default = "@orlix_kernel_toolchain//:autotools-identity.json"),
        "compiler_identity": attr.label(allow_single_file = True, default = "@orlix_kernel_toolchain//:guest-compiler-identity.json"),
        "gmake": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/gmake")),
        "host_tools": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:mlibc_support")),
        "tool_libs": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:tool_libs")),
        "clang_resources": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:clang_resources")),
        "guest_clang_support": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:guest_clang_support")),
        "macos_sdk": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:macos_sdk")),
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
        "bootstrap": attr.bool(default = False),
        "make_only": attr.bool(default = False),
        "make_vars": attr.string_list(),
        "file_expect": attr.string(default = "ARM aarch64"),
        "require_static": attr.bool(default = False),
        "cppflags": attr.string(default = ""),
        "cflags": attr.string(default = ""),
        "interface_packages": attr.label_list(providers = [OrlixPackageTreeInfo]),
        "interface_paths": attr.string_list_dict(),
        "extra_archives": attr.string_list(),
        "extra_prerequisites": attr.string_list(),
        "sysroot_archives": attr.string_list(),
        "verify_files": attr.string_list(),
        "_artifact_identity_serializer": attr.label(
            allow_single_file = True,
            default = Label("//bazel:content_digest.py"),
        ),
    },
)
