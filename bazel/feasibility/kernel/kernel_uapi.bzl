"""Bounded Kbuild headers_install for upstream ARCH=arm64 UAPI."""

load("//bazel/providers:kernel_info.bzl", "OrlixInstalledUapiInfo", "OrlixLinuxArchiveInfo")

def _pinned_env(ctx):
    shell = ctx.configuration.default_shell_env
    developer_dir = shell.get("DEVELOPER_DIR")
    if not developer_dir:
        fail("kernel UAPI requires action_env DEVELOPER_DIR")
    env = {
        "DEVELOPER_DIR": developer_dir,
        "HOME": "/var/empty",
        "PATH": "/opt/homebrew/opt/gnu-sed/libexec/gnubin:/opt/homebrew/bin:/usr/bin:/bin",
    }
    tmpdir = shell.get("TMPDIR")
    if tmpdir:
        env["TMPDIR"] = tmpdir
    persist = shell.get("ORLIX_KBUILD_PERSIST")
    if persist:
        env["ORLIX_KBUILD_PERSIST"] = persist
    return env

def _kernel_uapi_impl(ctx):
    headers = ctx.actions.declare_directory(ctx.label.name + "/uapi")
    archive = ctx.actions.declare_file(ctx.label.name + "/kbuild-archive.tar")
    manifest = ctx.actions.declare_file(ctx.label.name + "/manifest.json")
    digest = ctx.actions.declare_file(ctx.label.name + "/uapi.sha256")
    ctx.actions.run_shell(
        mnemonic = "OrlixLinuxHeadersInstall",
        progress_message = "Installing upstream Linux arm64 UAPI headers",
        command = r"""
set -euo pipefail
exec_root="$PWD"
linux_makefile="$exec_root/$1"
headers_out="$exec_root/$2"
archive_out="$exec_root/$3"
manifest_out="$exec_root/$4"
digest_out="$exec_root/$5"
persist_py="$exec_root/$6"
test -n "${DEVELOPER_DIR:-}"
xcode_ver="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcodebuild -version)"
xcode_name="$(printf '%s\n' "$xcode_ver" | /usr/bin/sed -n '1p')"
xcode_build="$(printf '%s\n' "$xcode_ver" | /usr/bin/sed -n '2p')"
case "$xcode_name|$xcode_build" in
  "Xcode 26.6|Build version 17F113"|"Xcode 27.0|Build version 27A5252f") ;;
  *) echo "unsupported Xcode: $xcode_ver" >&2; exit 1 ;;
esac
xcode_build_id="$(printf '%s\n' "$xcode_ver" | /usr/bin/sed -n 's/^Build version //p')"
ident="$(/usr/bin/python3 "$persist_py" identity 6.12.105 14c37ff05f22da2fa7076d10f6a07c7ede330c83 "$xcode_build_id")"
persist_dir="${ORLIX_KBUILD_PERSIST:-}"
if [ -n "$persist_dir" ]; then
  persist_dir="$persist_dir/headers_install"
fi
reused=0
if [ -n "$persist_dir" ] && /usr/bin/python3 "$persist_py" reuse "$persist_dir" "$ident" "$headers_out" "$archive_out"; then
  reused=1
  echo "orlix-kbuild: reuse $persist_dir" >&2
fi
if [ "$reused" -eq 0 ]; then
  gmake="$(/usr/bin/command -v gmake)"
  test -n "$gmake"
  sed --version >/dev/null
  hostcc="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
  test -x "$hostcc"
  sdkroot="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcrun --sdk macosx --show-sdk-path)"
  test -n "$sdkroot"
  case "$linux_makefile" in
    *orlix_linux_source*/Makefile) ;;
    *) echo "kernel UAPI must invoke upstream Linux Makefile, got $linux_makefile" >&2; exit 1 ;;
  esac
  linux_src="$(/usr/bin/dirname "$linux_makefile")"
  work="$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/orlix-linux-uapi.XXXXXX")"
  trap '/bin/rm -rf "$work"' EXIT
  /bin/mkdir -p "$work/linux" "$work/hdr"
  /bin/cp -R "$linux_src/." "$work/linux"
  /usr/bin/find "$work/linux" -type d -exec /bin/chmod u+w {} +
  /bin/mkdir -p "$work/linux/.orlix-uapi-build"
  cd "$work/linux"
  env -u MAKEFLAGS -u MFLAGS -u GNUMAKEFLAGS \
      -u IPHONEOS_DEPLOYMENT_TARGET -u TVOS_DEPLOYMENT_TARGET -u WATCHOS_DEPLOYMENT_TARGET \
      SDKROOT="$sdkroot" \
      KBUILD_BUILD_TIMESTAMP="1970-01-01" \
      KBUILD_BUILD_USER="orlix" \
      KBUILD_BUILD_HOST="bazel" \
      "$gmake" -j1 -C "$work/linux" O="$work/linux/.orlix-uapi-build" ARCH=arm64 LLVM=1 \
          HOSTCC="$hostcc" INSTALL_HDR_PATH="$work/hdr" headers_install
  test -s "$work/hdr/include/linux/unistd.h"
  test -s "$work/hdr/include/asm/unistd.h"
  /bin/mkdir -p "$headers_out"
  /bin/cp -R "$work/hdr/include" "$headers_out/include"
  /usr/bin/python3 "$persist_py" archive "$work/linux/.orlix-uapi-build" "$archive_out"
  if [ -n "$persist_dir" ]; then
    /usr/bin/python3 "$persist_py" store "$persist_dir" "$ident" "$headers_out" "$archive_out"
  fi
fi
/usr/sbin/chown -R "$(/usr/bin/id -u):$(/usr/bin/id -g)" "$headers_out" 2>/dev/null || true
digest="$(/usr/bin/find "$headers_out/include" -type f -print0 | /usr/bin/sort -z | /usr/bin/xargs -0 /usr/bin/shasum -a 256 | /usr/bin/awk '{print $1}' | /usr/bin/sort | /usr/bin/shasum -a 256 | /usr/bin/awk '{print $1}')"
/usr/bin/printf '%s\n' "$digest" > "$digest_out"
/usr/bin/printf '%s\n' '{' \
    '  "arch": "arm64",' \
    '  "component": "OrlixKernel",' \
    '  "kbuild_target": "headers_install",' \
    '  "linux_revision": "6.12.105",' \
    '  "linux_tag_commit": "14c37ff05f22da2fa7076d10f6a07c7ede330c83",' \
    '  "uapi_digest": "'"$digest"'"' \
    '}' > "$manifest_out"
""",
        arguments = [
            ctx.file.linux_makefile.path,
            headers.path,
            archive.path,
            manifest.path,
            digest.path,
            ctx.file.persist_tool.path,
        ],
        inputs = depset(
            direct = [ctx.file.linux_makefile, ctx.file.persist_tool],
            transitive = [ctx.attr.linux_source[DefaultInfo].files],
        ),
        outputs = [headers, archive, manifest, digest],
        env = _pinned_env(ctx),
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-sandbox": "1"},
    )
    return [
        DefaultInfo(files = depset([headers, archive, manifest, digest])),
        OutputGroupInfo(
            headers = depset([headers]),
            archive = depset([archive]),
            manifest = depset([manifest]),
        ),
        OrlixInstalledUapiInfo(
            arch = "arm64",
            headers = headers,
            linux_revision = "6.12.105",
            uapi_digest = digest,
        ),
        OrlixLinuxArchiveInfo(
            archive = archive,
            build_manifest = manifest,
            destination = "host",
            profile = "release",
            source_input_digest = digest,
            symbol_manifest = manifest,
        ),
    ]

orlix_kernel_uapi = rule(
    implementation = _kernel_uapi_impl,
    attrs = {
        "linux_source": attr.label(mandatory = True),
        "linux_makefile": attr.label(allow_single_file = True, mandatory = True),
        "persist_tool": attr.label(allow_single_file = True, default = Label("//bazel/feasibility/kernel:kbuild_persist.py")),
    },
)

_ALLOWED_DESTINATIONS = {
    "iphoneos": True,
    "iphonesimulator": True,
}

_ALLOWED_PROFILES = {
    "development": True,
    "release": True,
}

def _kernel_uapi_variant_impl(ctx):
    if ctx.attr.destination not in _ALLOWED_DESTINATIONS:
        fail("unsupported Apple destination %s" % ctx.attr.destination)
    if ctx.attr.profile not in _ALLOWED_PROFILES:
        fail("unsupported Orlix profile %s" % ctx.attr.profile)
    uapi = ctx.attr.uapi[OrlixInstalledUapiInfo]
    archive = ctx.attr.uapi[OrlixLinuxArchiveInfo]
    stamped = ctx.actions.declare_file(ctx.label.name + "/manifest.json")
    ctx.actions.write(
        stamped,
        "{\n  \"arch\": \"arm64\",\n  \"component\": \"OrlixKernel\",\n  \"destination\": \"%s\",\n  \"kbuild_target\": \"headers_install\",\n  \"linux_revision\": \"6.12.105\",\n  \"profile\": \"%s\"\n}\n" % (
            ctx.attr.destination,
            ctx.attr.profile,
        ),
    )
    return [
        DefaultInfo(files = depset([stamped], transitive = [ctx.attr.uapi[DefaultInfo].files])),
        OrlixInstalledUapiInfo(
            arch = uapi.arch,
            headers = uapi.headers,
            linux_revision = uapi.linux_revision,
            uapi_digest = uapi.uapi_digest,
        ),
        OrlixLinuxArchiveInfo(
            archive = archive.archive,
            build_manifest = stamped,
            destination = ctx.attr.destination,
            profile = ctx.attr.profile,
            source_input_digest = archive.source_input_digest,
            symbol_manifest = stamped,
        ),
    ]

orlix_kernel_uapi_variant = rule(
    implementation = _kernel_uapi_variant_impl,
    attrs = {
        "uapi": attr.label(mandatory = True, providers = [OrlixInstalledUapiInfo, OrlixLinuxArchiveInfo]),
        "destination": attr.string(mandatory = True),
        "profile": attr.string(mandatory = True),
    },
)
