"""Rootfs, initramfs, and ext4 from package trees. No wrapper Makefiles."""

load("//bazel/providers:package_info.bzl", "OrlixPackageTreeInfo")
load("//bazel/providers:rootfs_info.bzl", "OrlixRootfsInfo")
load("//bazel/providers:sysroot_info.bzl", "OrlixLibcSysrootInfo")

def _pinned_env(ctx):
    shell = ctx.configuration.default_shell_env
    developer_dir = shell.get("DEVELOPER_DIR")
    if not developer_dir:
        fail("rootfs requires action_env DEVELOPER_DIR")
    env = {
        "DEVELOPER_DIR": developer_dir,
        "HOME": "/var/empty",
        "PATH": "/opt/homebrew/opt/e2fsprogs/sbin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/bin:/bin",
    }
    tmpdir = shell.get("TMPDIR")
    if tmpdir:
        env["TMPDIR"] = tmpdir
    return env

def _rootfs_impl(ctx):
    pkgs = [p[OrlixPackageTreeInfo] for p in ctx.attr.packages]
    sysroot = ctx.attr.sysroot[OrlixLibcSysrootInfo]
    base_tree = ctx.actions.declare_directory(ctx.label.name + "/base-tree")
    state_tree = ctx.actions.declare_directory(ctx.label.name + "/state-tree")
    initramfs = ctx.actions.declare_file(ctx.label.name + "/initramfs.cpio.gz")
    base_ext4 = ctx.actions.declare_file(ctx.label.name + "/base.ext4")
    state_ext4 = ctx.actions.declare_file(ctx.label.name + "/state.ext4")
    file_manifest = ctx.actions.declare_file(ctx.label.name + "/file-manifest.txt")
    payload_metadata = ctx.actions.declare_file(ctx.label.name + "/payload-metadata.txt")
    digest = ctx.actions.declare_file(ctx.label.name + "/source-input.sha256")
    trees = ctx.actions.declare_file(ctx.label.name + "/package-trees.txt")
    ctx.actions.write(trees, "\n".join([pkg.install_tree.path for pkg in pkgs]) + "\n")
    pkg_inputs = []
    for pkg in pkgs:
        pkg_inputs.extend([pkg.install_tree, pkg.file_manifest, pkg.source_input_digest])
    ctx.actions.run_shell(
        mnemonic = "OrlixRootfs",
        progress_message = "Assembling feasibility rootfs, initramfs, and ext4",
        command = r"""
set -euo pipefail
exec_root="$PWD"
trees_file="$exec_root/$1"
gen_init_cpio_src="$exec_root/$2"
base_tree="$exec_root/$3"
state_tree="$exec_root/$4"
initramfs_out="$exec_root/$5"
base_ext4="$exec_root/$6"
state_ext4="$exec_root/$7"
file_manifest="$exec_root/$8"
payload_metadata="$exec_root/$9"
digest_out="$exec_root/${10}"
case "$gen_init_cpio_src" in
  *usr/gen_init_cpio.c) ;;
  *) echo "rootfs must compile upstream Linux gen_init_cpio.c, got $gen_init_cpio_src" >&2; exit 1 ;;
esac
case "$trees_file" in
  *OrlixOS/Sources/make*|*OrlixKernel/Makefile*)
    echo "rootfs must not invoke wrapper Makefiles" >&2
    exit 1
    ;;
esac
hostcc="$DEVELOPER_DIR/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
sdkroot="$(DEVELOPER_DIR="$DEVELOPER_DIR" /usr/bin/xcrun --sdk macosx --show-sdk-path)"
mke2fs="$(/usr/bin/command -v mke2fs)"
test -x "$hostcc"
test -n "$sdkroot"
test -n "$mke2fs"
work="$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/orlix-rootfs.XXXXXX")"
trap '/bin/rm -rf "$work"' EXIT
"$hostcc" -isysroot "$sdkroot" -O2 -o "$work/gen_init_cpio" "$gen_init_cpio_src"
/bin/mkdir -p "$base_tree/bin" "$base_tree/sbin" "$base_tree/usr/bin" "$base_tree/dev" "$base_tree/proc" "$base_tree/sys" "$base_tree/tmp" "$state_tree/upper" "$state_tree/work"
while IFS= read -r rel; do
  [ -n "$rel" ] || continue
  tree="$exec_root/$rel"
  if [ -d "$tree/usr/bin" ]; then /bin/cp -R "$tree/usr/bin/." "$base_tree/usr/bin/"; fi
  if [ -d "$tree/bin" ]; then /bin/cp -R "$tree/bin/." "$base_tree/bin/"; fi
  if [ -d "$tree/sbin" ]; then /bin/cp -R "$tree/sbin/." "$base_tree/sbin/"; fi
done < "$trees_file"
copy_bin() {
  src="$1"
  dest="$2"
  test -x "$src"
  /bin/cp "$src" "$dest"
  /bin/chmod 0755 "$dest"
}
copy_bin "$base_tree/usr/bin/true" "$base_tree/bin/true"
copy_bin "$base_tree/usr/bin/ls" "$base_tree/bin/ls"
copy_bin "$base_tree/usr/bin/bash" "$base_tree/bin/bash"
copy_bin "$base_tree/usr/bin/grep" "$base_tree/bin/grep"
copy_bin "$base_tree/usr/bin/find" "$base_tree/bin/find"
copy_bin "$base_tree/usr/bin/xargs" "$base_tree/bin/xargs"
copy_bin "$base_tree/usr/bin/mke2fs" "$base_tree/bin/mke2fs"
copy_bin "$base_tree/usr/bin/mkfs.ext4" "$base_tree/bin/mkfs.ext4"
copy_bin "$base_tree/usr/bin/debugfs" "$base_tree/bin/debugfs"
copy_bin "$base_tree/usr/bin/e2fsck" "$base_tree/bin/e2fsck"
test -x "$base_tree/usr/bin/getconf"
test -x "$base_tree/usr/bin/getent"
test -x "$base_tree/sbin/init"
test -x "$base_tree/usr/bin/jq"
test -x "$base_tree/usr/bin/curl"
test -x "$base_tree/usr/bin/zsh"
/bin/ln -sf bash "$base_tree/bin/sh"
/bin/cp "$base_tree/bin/true" "$work/init"
TZ=UTC /usr/bin/touch -t 197001010000 "$work/init"
/usr/bin/printf '%s\n' 'dir /bin 0755 0 0' 'dir /dev 0755 0 0' 'nod /dev/console 0600 0 0 c 5 1' 'file /init '"$work/init"' 0755 0 0' > "$work/initramfs.list"
"$work/gen_init_cpio" "$work/initramfs.list" | /usr/bin/gzip -n > "$initramfs_out"
test -s "$initramfs_out"
/bin/dd if=/dev/zero of="$base_ext4" bs=1048576 count=256 status=none
/bin/dd if=/dev/zero of="$state_ext4" bs=1048576 count=8 status=none
"$mke2fs" -q -t ext4 -F -m 0 -O ^orphan_file -U clear -L ORLIXROOT -E root_owner=0:0 -d "$base_tree" "$base_ext4"
"$mke2fs" -q -t ext4 -F -m 0 -O ^orphan_file -U clear -L ORLIXSTATE -E root_owner=0:0 -d "$state_tree" "$state_ext4"
/usr/bin/find "$base_tree" -print | /usr/bin/sort > "$file_manifest"
/usr/bin/printf 'init=/bin/true\ninitramfs=initramfs.cpio.gz\nbase_ext4=base.ext4\nstate_ext4=state.ext4\npackages=coreutils,bash,grep,findutils,e2fsprogs,getconf,getent,init,jq,curl,zsh\nbase_packages=bash coreutils grep findutils e2fsprogs jq curl zsh\nshell=/bin/sh\n' > "$payload_metadata"
digest="$( (
  cd "$base_tree"
  /usr/bin/find . -type f -print0 | /usr/bin/sort -z | /usr/bin/xargs -0 /usr/bin/shasum -a 256
  /usr/bin/shasum -a 256 < "$payload_metadata"
) | /usr/bin/shasum -a 256 | /usr/bin/awk '{print $1}' )"
/usr/bin/printf '%s\n' "$digest" > "$digest_out"
""",
        arguments = [
            trees.path,
            ctx.file.gen_init_cpio.path,
            base_tree.path,
            state_tree.path,
            initramfs.path,
            base_ext4.path,
            state_ext4.path,
            file_manifest.path,
            payload_metadata.path,
            digest.path,
        ],
        inputs = depset(
            direct = pkg_inputs + [
                trees,
                ctx.file.gen_init_cpio,
                sysroot.consumed_uapi_digest,
            ],
        ),
        outputs = [base_tree, state_tree, initramfs, base_ext4, state_ext4, file_manifest, payload_metadata, digest],
        env = _pinned_env(ctx),
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-sandbox": "1"},
    )
    return [
        DefaultInfo(files = depset([base_tree, state_tree, initramfs, base_ext4, state_ext4, file_manifest, payload_metadata, digest])),
        OrlixRootfsInfo(
            base_ext4 = base_ext4,
            base_tree = base_tree,
            file_manifest = file_manifest,
            initramfs = initramfs,
            package_closure = pkgs[0].source_input_digest,
            payload_metadata = payload_metadata,
            source_input_digest = digest,
            state_ext4 = state_ext4,
            state_tree = state_tree,
        ),
    ]

def _rootfs_payload_impl(ctx):
    info = ctx.attr.rootfs[OrlixRootfsInfo]
    root = ctx.actions.declare_directory(ctx.label.name)
    ctx.actions.run_shell(
        mnemonic = "OrlixRootfsPayload",
        progress_message = "Staging OrlixOS rootfs payload images",
        command = r"""
set -euo pipefail
dest="$1"
/bin/mkdir -p "$dest"
/bin/cp "$2" "$dest/initramfs.cpio.gz"
/bin/cp "$3" "$dest/base.ext4"
/bin/cp "$4" "$dest/state.ext4"
""",
        arguments = [
            root.path,
            info.initramfs.path,
            info.base_ext4.path,
            info.state_ext4.path,
        ],
        inputs = [info.initramfs, info.base_ext4, info.state_ext4],
        outputs = [root],
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )
    return [DefaultInfo(files = depset([root]))]


orlix_rootfs_payload = rule(
    implementation = _rootfs_payload_impl,
    attrs = {
        "rootfs": attr.label(mandatory = True, providers = [OrlixRootfsInfo]),
    },
)


orlix_rootfs = rule(
    implementation = _rootfs_impl,
    attrs = {
        "packages": attr.label_list(mandatory = True, providers = [OrlixPackageTreeInfo]),
        "sysroot": attr.label(mandatory = True, providers = [OrlixLibcSysrootInfo]),
        "gen_init_cpio": attr.label(allow_single_file = True, mandatory = True),
    },
)
