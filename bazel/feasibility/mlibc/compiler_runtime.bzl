"""Linux compiler-rt archive. The sysroot depends on this rule and not the builtin sources."""

load("//bazel:artifact_identity.bzl", "declare_artifact_identity")

OrlixCompilerRuntimeInfo = provider(
    doc = "Linux compiler-rt archive the sysroot links and does not rebuild.",
    fields = {
        "archive": "libcompiler_rt.a",
        "identity_digest": "SHA-256 of the compiler-runtime artifact-identity-v2 manifest.",
        "identity_manifest": "Canonical compiler-runtime artifact-identity-v2 manifest.",
    },
)

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

def _pinned_env(_ctx):
    return {
        "HOME": "/var/empty",
        "LC_ALL": "C",
        "PATH": "/usr/bin:/bin",
        "PYTHONDONTWRITEBYTECODE": "1",
    }

def _compiler_runtime_impl(ctx):
    runtime = ctx.actions.declare_file(ctx.label.name + "/libcompiler_rt.a")
    builtins = ctx.file.compiler_rt.dirname
    sources = {f.path: f for f in ctx.files.compiler_rt_source}
    selected = [sources[builtins + "/" + name] for name in _COMPILER_RT_SOURCES]
    headers = [f for f in ctx.files.compiler_rt_source if f.extension in ["h", "inc"]]
    # Staged clang, llvm-ar, mkdir, rm, and sort. The recipe does not call
    # xcrun or Homebrew, so the result can be cached remotely.
    ctx.actions.run_shell(
        mnemonic = "OrlixCompilerRuntime",
        progress_message = "Building Linux compiler runtime",
        command = r"""
set -euo pipefail
exec_root="$PWD"
builtins="$exec_root/$1"
runtime_out="$exec_root/$2"
clang="$exec_root/$3"
ar="$exec_root/$4"
libdir=""
if [ "$5" != none ]; then libdir="$exec_root/$5"; fi
mkdir_bin="$exec_root/$6"
rm_bin="$exec_root/$7"
sort_bin="$exec_root/$8"
if [ -n "$libdir" ]; then export DYLD_LIBRARY_PATH="$libdir"; fi
export PATH="${clang%/*}"
test -x "$clang"
test -x "$ar"
test -x "$mkdir_bin"
test -x "$rm_bin"
test -x "$sort_bin"
case "$clang" in
  */tools/bin/clang) ;;
  *) printf '%s\n' "compiler runtime must execute the staged clang" >&2; exit 1 ;;
esac
case "$ar" in
  */tools/bin/llvm-ar) ;;
  *) printf '%s\n' "compiler runtime must execute the staged llvm-ar" >&2; exit 1 ;;
esac
case "$mkdir_bin" in
  */tools/bin/mkdir) ;;
  *) printf '%s\n' "compiler runtime must execute the staged mkdir" >&2; exit 1 ;;
esac
case "$rm_bin" in
  */tools/bin/rm) ;;
  *) printf '%s\n' "compiler runtime must execute the staged rm" >&2; exit 1 ;;
esac
case "$sort_bin" in
  */tools/bin/sort) ;;
  *) printf '%s\n' "compiler runtime must execute the staged sort" >&2; exit 1 ;;
esac
tool_root="${clang%/*}"
tool_root="${tool_root%/*}"
work="$exec_root/compiler-rt-objects"
"$rm_bin" -rf "$work"
"$mkdir_bin" -p "$work"
shift 8
objects=()
for relative in "$@"; do
    source="$exec_root/$relative"
    source_name="${source#"$builtins/"}"
    object_name="${source_name//\//-}"
    object="$work/${object_name%.c}.o"
    "$clang" --target=aarch64-linux-gnu -ffreestanding -fno-builtin -nostdlibinc -ffixed-x18 -O2 "-ffile-prefix-map=$builtins=/orlix/compiler-rt" "-ffile-prefix-map=$work=/orlix/compiler-rt-obj" "-ffile-prefix-map=$tool_root=/orlix/tools" -I"$builtins" -c "$source" -o "$object"
    objects+=("$object")
done
(
    cd "$work"
    names=()
    for object in "${objects[@]}"; do names+=("${object##*/}"); done
    members=()
    while IFS= read -r object; do members+=("$object"); done < <(printf '%s\n' "${names[@]}" | "$sort_bin")
    export ZERO_AR_DATE=1
    export SOURCE_DATE_EPOCH=1
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
            ctx.file.mkdir.path,
            ctx.file.rm.path,
            ctx.file.sort.path,
        ] + [f.path for f in selected],
        inputs = depset(selected + headers + [
            ctx.file.runtime_toolchain_identity,
            ctx.file.clang,
            ctx.file.llvm_ar,
            ctx.file.mkdir,
            ctx.file.rm,
            ctx.file.sort,
        ] + ctx.files.tool_libs + ctx.files.clang_resources),
        outputs = [runtime],
        env = _pinned_env(ctx),
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )
    identity = declare_artifact_identity(
        ctx,
        "compiler-runtime",
        ctx.file._artifact_identity_serializer,
        artifacts = {"libcompiler_rt.a": runtime},
    )
    return [
        DefaultInfo(files = depset([runtime, identity.manifest, identity.digest])),
        OrlixCompilerRuntimeInfo(
            archive = runtime,
            identity_digest = identity.digest,
            identity_manifest = identity.manifest,
        ),
    ]

orlix_compiler_runtime = rule(
    implementation = _compiler_runtime_impl,
    attrs = {
        "compiler_rt_source": attr.label(mandatory = True),
        "compiler_rt": attr.label(allow_single_file = True, mandatory = True),
        "runtime_toolchain_identity": attr.label(allow_single_file = True, mandatory = True),
        "clang": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/clang")),
        "llvm_ar": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/llvm-ar")),
        "mkdir": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/mkdir")),
        "rm": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/rm")),
        "sort": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/sort")),
        "tool_libs": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:tool_libs")),
        "clang_resources": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:clang_resources")),
        "_artifact_identity_serializer": attr.label(
            allow_single_file = True,
            default = Label("//bazel:content_digest.py"),
        ),
    },
)
