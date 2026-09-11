"""Rootfs, initramfs, and ext4 from package trees. No wrapper Makefiles."""

load("//bazel:artifact_identity.bzl", "declare_artifact_identity")
load("//bazel/providers:package_info.bzl", "OrlixPackageTreeInfo")
load("//bazel/providers:rootfs_info.bzl", "OrlixRootfsInfo")

def _pinned_env(ctx):
    shell = ctx.configuration.default_shell_env
    env = {
        "HOME": "/var/empty",
        "PATH": "/opt/homebrew/opt/e2fsprogs/sbin:/opt/homebrew/opt/llvm/bin:/opt/homebrew/bin:/usr/bin:/bin",
    }
    tmpdir = shell.get("TMPDIR")
    if tmpdir:
        env["TMPDIR"] = tmpdir
    return env

def _rootfs_impl(ctx):
    pkgs = [p[OrlixPackageTreeInfo] for p in ctx.attr.packages]
    initramfs = ctx.actions.declare_file(ctx.label.name + "/initramfs.cpio.gz")
    base_ext4 = ctx.actions.declare_file(ctx.label.name + "/base.ext4")
    state_ext4 = ctx.actions.declare_file(ctx.label.name + "/state.ext4")
    file_manifest = ctx.actions.declare_file(ctx.label.name + "/file-manifest.txt")
    payload_metadata = ctx.actions.declare_file(ctx.label.name + "/payload-metadata.txt")
    digest = ctx.actions.declare_file(ctx.label.name + "/source-input.sha256")
    trees = ctx.actions.declare_file(ctx.label.name + "/package-trees.txt")
    ctx.actions.write(trees, "\n".join([pkg.install_tree.path for pkg in pkgs]) + "\n")
    pkg_inputs = [pkg.install_tree for pkg in pkgs]
    ctx.actions.run_shell(
        mnemonic = "OrlixRootfs",
        progress_message = "Assembling feasibility rootfs, initramfs, and ext4",
        command = r"""
set -euo pipefail
umask 022
exec_root="$PWD"
trees_file="$exec_root/$1"
gen_init_cpio="$exec_root/$2"
initramfs_out="$exec_root/$3"
base_ext4="$exec_root/$4"
state_ext4="$exec_root/$5"
file_manifest="$exec_root/$6"
payload_metadata="$exec_root/$7"
digest_out="$exec_root/$8"
toolchain_identity="$exec_root/$9"
case "$trees_file" in
  *OrlixOS/Sources/make*|*OrlixKernel/Makefile*)
    echo "rootfs must not invoke wrapper Makefiles" >&2
    exit 1
    ;;
esac
mke2fs="$(/usr/bin/command -v mke2fs)"
debugfs="$(/usr/bin/command -v debugfs)"
test -x "$gen_init_cpio"
test -s "$toolchain_identity"
test -n "$mke2fs"
test -x "$debugfs"
work="$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/orlix-rootfs.XXXXXX")"
trap '/bin/rm -rf "$work"' EXIT
base_tree="$work/base-tree"
state_tree="$work/state-tree"
/bin/mkdir -p "$base_tree/bin" "$base_tree/sbin" "$base_tree/usr/bin" "$base_tree/dev" "$base_tree/proc" "$base_tree/sys" "$base_tree/tmp" "$state_tree/upper" "$state_tree/work"
while IFS= read -r rel; do
  [ -n "$rel" ] || continue
  tree="$exec_root/$rel"
  if [ -x "$tree/rootinit" ]; then /bin/cp "$tree/rootinit" "$work/init"; fi
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
test -x "$base_tree/usr/bin/getfattr"
test -x "$base_tree/usr/bin/setfattr"
test -x "$base_tree/usr/bin/getfacl"
test -x "$base_tree/usr/bin/setfacl"
test -x "$base_tree/usr/bin/getcap"
test -x "$base_tree/usr/bin/setcap"
test -x "$base_tree/usr/bin/getenforce"
test -x "$base_tree/usr/bin/setenforce"
test -x "$base_tree/usr/bin/selinuxenabled"
test -x "$base_tree/usr/bin/policyvers"
test -x "$base_tree/usr/bin/getpolicyload"
/bin/ln -sf bash "$base_tree/bin/sh"
test -x "$work/init"
/usr/bin/printf '%s\n' 'dir /bin 0755 0 0' 'dir /dev 0755 0 0' 'nod /dev/console 0600 0 0 c 5 1' 'file /init '"$work/init"' 0755 0 0' > "$work/initramfs.list"
"$gen_init_cpio" -t 1 "$work/initramfs.list" | /usr/bin/gzip -n > "$initramfs_out"
test -s "$initramfs_out"
/bin/dd if=/dev/zero of="$base_ext4" bs=1048576 count=256 status=none
/bin/dd if=/dev/zero of="$state_ext4" bs=1048576 count=8 status=none
build_ext4() {
  tree="$1"
  image="$2"
  label="$3"
  SOURCE_DATE_EPOCH=1 "$mke2fs" -q -t ext4 -F -m 0 -O ^orphan_file -U clear -L "$label" -E root_owner=0:0,hash_seed=00000000-0000-0000-0000-000000000001 -d "$tree" "$image"
  (cd "$tree" && /usr/bin/find . -print | /usr/bin/sort) > "$work/paths"
  while IFS= read -r path; do
    [ "$path" != . ] || continue
    /usr/bin/printf 'set_inode_field "%s" uid 0\nset_inode_field "%s" gid 0\n' "${path#.}" "${path#.}"
  done < "$work/paths" > "$work/owners"
  SOURCE_DATE_EPOCH=1 "$debugfs" -w -f "$work/owners" "$image" > "$work/owners.out" 2> "$work/owners.err"
  if /usr/bin/grep -Ev '^(debugfs [0-9].*|[[:space:]]*)$' "$work/owners.err"; then exit 1; fi
}
build_ext4 "$base_tree" "$base_ext4" ORLIXROOT
build_ext4 "$state_tree" "$state_ext4" ORLIXSTATE
check_image_entry() {
  entry_stat="$("$debugfs" -R "stat $2" "$1")"
  if ! /usr/bin/printf '%s\n' "$entry_stat" | /usr/bin/grep -Eq "Type: $3[[:space:]]+Mode:[[:space:]]+0[0-7]*[1357][0-7][0-7][[:space:]]"; then
    echo "invalid rootfs image entry $1:$2, expected $3 with owner execute permission" >&2
    exit 1
  fi
  /usr/bin/printf '%s\n' "$entry_stat" | /usr/bin/grep -Eq 'User:[[:space:]]+0[[:space:]]+Group:[[:space:]]+0[[:space:]]' || { echo "non-root image ownership at $1:$2" >&2; exit 1; }
}
for entry in /bin/true /bin/ls /usr/bin/true /usr/bin/ls /bin/bash /usr/bin/bash /bin/grep /bin/find /bin/xargs /bin/mke2fs /bin/mkfs.ext4 /bin/debugfs /bin/e2fsck /usr/bin/getconf /usr/bin/getent /sbin/init /usr/bin/jq /usr/bin/curl /usr/bin/zsh /usr/bin/getfattr /usr/bin/setfattr /usr/bin/getfacl /usr/bin/setfacl /usr/bin/getcap /usr/bin/setcap /usr/bin/getenforce /usr/bin/setenforce /usr/bin/selinuxenabled /usr/bin/policyvers /usr/bin/getpolicyload; do
  check_image_entry "$base_ext4" "$entry" regular
done
for entry in /dev /proc /sys /tmp; do check_image_entry "$base_ext4" "$entry" directory; done
for entry in /upper /work; do check_image_entry "$state_ext4" "$entry" directory; done
check_image_entry "$base_ext4" /bin/sh symlink
/usr/bin/printf '%s\n' "$entry_stat" | /usr/bin/grep -Fx 'Fast link dest: "bash"'
(cd "$base_tree" && /usr/bin/find . -print | /usr/bin/sort) > "$file_manifest"
/usr/bin/printf 'init=/init\ninitramfs=initramfs.cpio.gz\nbase_ext4=base.ext4\nstate_ext4=state.ext4\npackages=coreutils,bash,grep,findutils,e2fsprogs,getconf,getent,init,jq,curl,zsh,attr,acl,libcap,libselinux\nbase_packages=bash coreutils grep findutils e2fsprogs jq curl zsh attr acl libcap libselinux\nshell=/bin/sh\n' > "$payload_metadata"
digest="$( (
  cd "$base_tree"
  /usr/bin/find . -type f -print0 | /usr/bin/sort -z | /usr/bin/xargs -0 /usr/bin/shasum -a 256
  /usr/bin/shasum -a 256 < "$payload_metadata"
  /usr/bin/shasum -a 256 < "$initramfs_out"
  /usr/bin/shasum -a 256 < "$base_ext4"
  /usr/bin/shasum -a 256 < "$state_ext4"
) | /usr/bin/shasum -a 256 | /usr/bin/awk '{print $1}' )"
/usr/bin/printf '%s\n' "$digest" > "$digest_out"
""",
        arguments = [
            trees.path,
            ctx.file.gen_init_cpio.path,
            initramfs.path,
            base_ext4.path,
            state_ext4.path,
            file_manifest.path,
            payload_metadata.path,
            digest.path,
            ctx.file.toolchain_identity.path,
        ],
        inputs = depset(
            direct = pkg_inputs + [
                trees,
                ctx.file.toolchain_identity,
            ],
        ),
        tools = [ctx.executable.gen_init_cpio],
        outputs = [initramfs, base_ext4, state_ext4, file_manifest, payload_metadata, digest],
        env = _pinned_env(ctx),
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-cache": "1", "no-remote-exec": "1", "no-sandbox": "1"},
    )
    artifact_identity = declare_artifact_identity(
        ctx,
        "rootfs",
        ctx.file._artifact_identity_serializer,
        artifacts = {
            "base.ext4": base_ext4,
            "initramfs.cpio.gz": initramfs,
            "state.ext4": state_ext4,
        },
    )
    return [
        DefaultInfo(files = depset([
            initramfs,
            base_ext4,
            state_ext4,
            file_manifest,
            payload_metadata,
            digest,
            artifact_identity.manifest,
            artifact_identity.digest,
        ])),
        OrlixRootfsInfo(
            artifact_identity_digest = artifact_identity.digest,
            artifact_identity_manifest = artifact_identity.manifest,
            base_ext4 = base_ext4,
            file_manifest = file_manifest,
            initramfs = initramfs,
            package_closure = depset(transitive = [pkg.artifact_identity_closure for pkg in pkgs]),
            payload_metadata = payload_metadata,
            source_input_digest = digest,
            state_ext4 = state_ext4,
        ),
    ]

def _rootfs_payload_impl(ctx):
    info = ctx.attr.rootfs[OrlixRootfsInfo]
    root = ctx.actions.declare_directory(ctx.label.name + "/rootfs")
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
    return [DefaultInfo(files = depset([root])), info]


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
        "gen_init_cpio": attr.label(allow_single_file = True, executable = True, cfg = "exec", mandatory = True),
        "toolchain_identity": attr.label(allow_single_file = True, default = "@orlix_kernel_toolchain//:rootfs-identity.json"),
        "_artifact_identity_serializer": attr.label(
            allow_single_file = True,
            default = Label("//bazel:content_digest.py"),
        ),
    },
)
