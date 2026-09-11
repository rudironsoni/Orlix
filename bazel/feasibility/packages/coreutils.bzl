"""Pinned GNU Coreutils Autotools guest package."""

load(":autotools.bzl", "declare_orlix_package_interface")
load(":programs.bzl", "ORLIX_COREUTILS_PROGRAMS")
load("//bazel:artifact_identity.bzl", "declare_artifact_identity")
load("//bazel/providers:kernel_info.bzl", "OrlixInstalledUapiInfo")
load("//bazel/providers:package_info.bzl", "OrlixPackageTreeInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")

_COREUTILS_FEATURE_PATHS = {
    "libselinux": ["usr/include/selinux", "usr/lib/libselinux.a"],
    "libcap": ["usr/include/sys/capability.h", "usr/include/linux/capability.h", "usr/lib/libcap.a"],
    "acl": ["usr/include/acl", "usr/include/sys/acl.h", "usr/lib/libacl.a"],
    "attr": ["usr/include/attr", "usr/lib/libattr.a"],
    "libsepol": ["usr/include/sepol", "usr/lib/libsepol.a"],
    "pcre2": ["usr/include/pcre2.h", "usr/lib/libpcre2-8.a"],
    "musl-fts": ["usr/include/fts.h", "usr/lib/libfts.a"],
}

def _pinned_env(ctx):
    shell = ctx.configuration.default_shell_env
    developer_dir = shell.get("ORLIX_PINNED_DEVELOPER_DIR")
    if not developer_dir:
        fail("coreutils package requires action_env ORLIX_PINNED_DEVELOPER_DIR")
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

def _coreutils_package_impl(ctx):
    sysroot = ctx.attr.sysroot[OrlixLibcSysrootInfo]
    uapi = ctx.attr.uapi[OrlixInstalledUapiInfo]
    if len(ctx.attr.interface_packages) != len(_COREUTILS_FEATURE_PATHS):
        fail("coreutils package requires seven feature packages")
    feature_input = declare_orlix_package_interface(
        ctx,
        ctx.attr.interface_packages,
        _COREUTILS_FEATURE_PATHS,
    )
    install_tree = ctx.actions.declare_directory(ctx.label.name + "/install")
    file_manifest = ctx.actions.declare_file(ctx.label.name + "/file-manifest.txt")
    license_manifest = ctx.actions.declare_file(ctx.label.name + "/license-manifest.txt")
    metadata = ctx.actions.declare_file(ctx.label.name + "/package-metadata.txt")
    digest = ctx.actions.declare_file(ctx.label.name + "/source-input.sha256")
    programs = ctx.actions.declare_file(ctx.label.name + "/programs.txt")
    ctx.actions.write(programs, "\n".join(ctx.attr.programs) + "\n")
    script = ctx.actions.declare_file(ctx.label.name + ".sh")
    ctx.actions.write(script, r"""
set -euo pipefail
exec_root="$PWD"
configure="$exec_root/$1"
headers="$exec_root/$2"
uapi_headers="$exec_root/$3"
libraries="$exec_root/$4"
runtime="$exec_root/$5"
feature_tree="$exec_root/${15}"
test -d "$feature_tree"
install_out="$exec_root/$6"
file_manifest="$exec_root/$7"
license_manifest="$exec_root/$8"
metadata="$exec_root/$9"
digest_out="$exec_root/${10}"
programs_file="$exec_root/${11}"
package_name="${12}"
package_version="${13}"
git_commit="${14}"
case "$configure" in
  *OrlixOS/Sources/make*|*OrlixCoreUtils/Makefile*|*OrlixKernel/Makefile*|*OrlixMLibC/Makefile*)
    echo "coreutils must not invoke wrapper Makefiles: $configure" >&2
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
feature_tree="$work/inputs/extra"
feature_libs=(
  "$feature_tree/usr/lib/libselinux.a"
  "$feature_tree/usr/lib/libcap.a"
  "$feature_tree/usr/lib/libacl.a"
  "$feature_tree/usr/lib/libattr.a"
  "$feature_tree/usr/lib/libsepol.a"
  "$feature_tree/usr/lib/libpcre2-8.a"
  "$feature_tree/usr/lib/libfts.a"
)
for archive in "${feature_libs[@]}"; do test -s "$archive"; done
/bin/mkdir -p "$work/build" "$work/dest/usr/bin"
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
gmake=/opt/homebrew/bin/gmake
export MAKE="$gmake"
cd "$work/build"
libm="$libraries/libc.a"
libpthread="$libraries/libc.a"
[ -s "$libraries/libm.a" ] && libm="$libraries/libm.a"
[ -s "$libraries/libpthread.a" ] && libpthread="$libraries/libpthread.a"
export CC="${launcher:+$launcher }$clang --target=aarch64-linux-gnu -isystem $headers -isystem $uapi_headers/include -D_GNU_SOURCE -fhosted -fno-builtin -ffixed-x18 -fPIE -ffile-prefix-map=$work=."
export CPPFLAGS="-I$feature_tree/usr/include"
export CFLAGS="-O2 -D_FILE_OFFSET_BITS=64 -Wno-unknown-warning-option -Wno-incompatible-function-pointer-types -include limits.h -ffile-prefix-map=$work=. -ffile-prefix-map=..=."
export LDFLAGS="--target=aarch64-linux-gnu -L$feature_tree/usr/lib -fuse-ld=lld -nostdlib -static-pie -Wl,--gc-sections -Wl,-z,max-page-size=0x4000 $libraries/crt1.o $libraries/crti.o -Wl,--start-group"
export LIBS="${feature_libs[*]} $libraries/libc.a $libm $libpthread $runtime -Wl,--end-group $libraries/crtn.o"
export AR="$ar_bin"
export RANLIB="$ranlib_bin"
export BUILD_CC="/usr/bin/xcrun --sdk macosx cc -target arm64-apple-macosx"
export gl_cv_header_working_fcntl_h=yes
export gl_cv_func_getopt_gnu=yes
export gl_cv_func_getopt_long_gnu=yes
export gl_cv_func_strtod_works=yes
if [ "$(/bin/cat "$work/inputs/configure-required")" = 1 ]; then
  "$work/src/configure" --host=aarch64-linux-gnu --build=aarch64-apple-darwin --prefix=/usr --disable-nls --with-selinux --enable-libcap --disable-gcc-warnings
fi
config_header="$work/build/lib/config.h"
test -s "$config_header"
for macro in USE_XATTR USE_ACL USE_SELINUX_SELINUX_H HAVE_CAP; do
  /usr/bin/grep -Eq "^#define $macro 1$" "$config_header"
done
"$gmake" -j1 all PROGRAMS= LIBRARIES= MANS= INFO_DEPS=
: > "$work/build/orlix-link-inputs.mk"
link_inputs="${feature_libs[*]} $libraries/libc.a $libm $libpthread $runtime $libraries/crt1.o $libraries/crti.o $libraries/crtn.o"
program_targets=()
while IFS= read -r program; do
  [ -n "$program" ] || continue
  source_program="$program"
  if [ "$program" = install ]; then source_program=ginstall; fi
  program_targets+=("src/$source_program")
  /usr/bin/printf 'src/%s: private .EXTRA_PREREQS := %s\n' "$source_program" "$link_inputs" >> "$work/build/orlix-link-inputs.mk"
done < "$programs_file"
"$gmake" -f GNUmakefile -f orlix-link-inputs.mk -j1 "${program_targets[@]}" MANS= INFO_DEPS=
nm_bin="$(/usr/bin/command -v nm)"
test -x "$nm_bin"
assert_symbol() {
  "$nm_bin" "$1" | /usr/bin/grep -E "[[:space:]][TtWw][[:space:]]$2$" >/dev/null
}
assert_symbol "src/chcon" is_selinux_enabled
assert_symbol "src/runcon" setexeccon
assert_symbol "src/cp" acl_set_file
assert_symbol "src/ls" cap_get_file
while IFS= read -r program; do
  [ -n "$program" ] || continue
  source_program="$program"
  if [ "$program" = "install" ]; then source_program=ginstall; fi
  if [ "$program" = "[" ]; then source_program='['; fi
  /usr/bin/install -m 0755 "src/$source_program" "$work/dest/usr/bin/$program"
  "$strip_bin" "$work/dest/usr/bin/$program"
  /usr/bin/file "$work/dest/usr/bin/$program" | /usr/bin/grep -F -q 'ELF 64-bit LSB pie executable, ARM aarch64' || {
    /usr/bin/file "$work/dest/usr/bin/$program" >&2
    exit 1
  }
done < "$programs_file"
for program in chcon runcon; do test -x "$work/dest/usr/bin/$program"; done
if [ -n "$launcher" ]; then "$launcher" --print-log-stats --format=json; fi
/bin/mkdir -p "$install_out"
/bin/cp -R "$work/dest/." "$install_out/"
(cd "$install_out" && /usr/bin/find . -type f -print | /usr/bin/sort) > "$file_manifest"
/usr/bin/printf '%s\n' 'license=GPL-3.0-or-later' > "$license_manifest"
/usr/bin/printf 'name=%s\nversion=%s\nengine=configure-make-destdir\ngit_commit=%s\ngnulib_git_url=https://github.com/coreutils/gnulib.git\n' "$package_name" "$package_version" "$git_commit" > "$metadata"
digest="$( ( cd "$install_out" && /usr/bin/find . -type f -print0 | /usr/bin/sort -z | /usr/bin/xargs -0 /usr/bin/shasum -a 256 ) | /usr/bin/shasum -a 256 | /usr/bin/awk '{print $1}' )"
/usr/bin/printf '%s\n' "$digest" > "$digest_out"
""".replace("__COMPILER_IDENTITY__", ctx.file.compiler_identity.path))
    ctx.actions.run_shell(
        mnemonic = "OrlixGuestPackage",
        progress_message = "Building GNU Coreutils with incremental Make state",
        command = "PYTHONPATH=. /usr/bin/python3 -B -c 'from bazel.feasibility.packages import build_state; import sys; arguments=sys.argv[5:]; raise SystemExit(build_state.run(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], arguments, extra_tree=arguments[-1]))' \"$@\"",
        arguments = [
            ctx.attr.package_name,
            script.path,
            ctx.file.toolchain_identity.path,
            ctx.file.compiler_identity.path,
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
            programs.path,
            ctx.attr.package_name,
            ctx.attr.package_version,
            ctx.attr.git_commit,
            feature_input.path,
        ],
        inputs = depset(
            direct = [
                script,
                ctx.file.toolchain_identity,
                ctx.file.compiler_identity,
                ctx.file.build_state,
                ctx.file.shared_state,
                ctx.file.configure,
                programs,
                sysroot.headers,
                uapi.headers,
                sysroot.libraries,
                sysroot.compiler_runtime,
                feature_input,
            ] + ctx.files.sources,
        ),
        outputs = [install_tree, file_manifest, license_manifest, metadata, digest],
        env = _pinned_env(ctx),
        use_default_shell_env = True,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-remote-cache": "1", "no-sandbox": "1"},
    )
    artifact_identity = declare_artifact_identity(
        ctx,
        "install",
        ctx.file._artifact_identity_serializer,
        tool_identity = ctx.file.compiler_identity,
        root = install_tree,
    )
    return [
        DefaultInfo(files = depset([
            install_tree,
            file_manifest,
            license_manifest,
            metadata,
            digest,
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
            source_input_digest = digest,
        ),
    ]

orlix_coreutils_package = rule(
    implementation = _coreutils_package_impl,
    attrs = {
        "toolchain_identity": attr.label(allow_single_file = True, default = "@orlix_kernel_toolchain//:coreutils-identity.json"),
        "compiler_identity": attr.label(allow_single_file = True, default = "@orlix_kernel_toolchain//:guest-compiler-identity.json"),
        "build_state": attr.label(allow_single_file = True, default = ":build_state.py"),
        "shared_state": attr.label(allow_single_file = True, default = "//bazel:build_state.py"),
        "sysroot": attr.label(mandatory = True, providers = [OrlixLibcSysrootInfo]),
        "uapi": attr.label(mandatory = True, providers = [OrlixInstalledUapiInfo]),
        "configure": attr.label(allow_single_file = True, mandatory = True),
        "sources": attr.label(allow_files = True, mandatory = True),
        "interface_packages": attr.label_list(mandatory = True, providers = [OrlixPackageTreeInfo]),
        "programs": attr.string_list(mandatory = True),
        "package_name": attr.string(default = "coreutils"),
        "package_version": attr.string(default = "9.11"),
        "git_commit": attr.string(default = "c01fd163a47468a8296fb369f5233853bb551bb6"),
        "_artifact_identity_serializer": attr.label(
            allow_single_file = True,
            default = Label("//bazel:content_digest.py"),
        ),
    },
)

# Re-export so BUILD files can load one symbol set.
COREUTILS_PROGRAMS = ORLIX_COREUTILS_PROGRAMS
