"""Rootfs, initramfs, and ext4 from package trees. No wrapper Makefiles."""

load("//bazel:artifact_identity.bzl", "declare_artifact_identity")
load("//bazel/providers:package_info.bzl", "OrlixPackageTreeInfo")
load("//bazel/providers:rootfs_info.bzl", "OrlixRootfsInfo")

def _pinned_env(_ctx):
    return {
        "HOME": "/var/empty",
        "PATH": "/usr/bin:/bin",
    }

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
mke2fs="$exec_root/${10}"
debugfs="$exec_root/${11}"
export MKE2FS_CONFIG="$exec_root/${12}"
policy="$exec_root/${14}"
libdir=""
if [ "${13}" != none ]; then libdir="$exec_root/${13}"; fi
if [ -n "$libdir" ]; then export DYLD_LIBRARY_PATH="$libdir"; fi
test -x "$gen_init_cpio"
test -s "$toolchain_identity"
test -x "$mke2fs"
test -x "$debugfs"
test -s "$MKE2FS_CONFIG"
test -s "$policy"
work="$exec_root/rootfs-work"
/bin/rm -rf "$work"
/bin/mkdir -p "$work"
trap '/bin/rm -rf "$work"' EXIT
base_tree="$work/base-tree"
state_tree="$work/state-tree"
for name in base_dirs state_dirs copy_bins requires symlinks metadata image_files image_dirs image_links; do
  : > "$work/$name"
done
while IFS= read -r line || [ -n "$line" ]; do
  case "$line" in
    ''|\#*) continue ;;
  esac
  key="${line%% *}"
  rest="${line#"$key"}"
  rest="${rest# }"
  case "$key" in
    base_dir) printf '%s\n' "$rest" >> "$work/base_dirs" ;;
    state_dir) printf '%s\n' "$rest" >> "$work/state_dirs" ;;
    copy_bin) printf '%s\n' "$rest" >> "$work/copy_bins" ;;
    require) printf '%s\n' "$rest" >> "$work/requires" ;;
    symlink) printf '%s\n' "$rest" >> "$work/symlinks" ;;
    metadata) printf '%s\n' "$rest" >> "$work/metadata" ;;
    image_file) printf '%s\n' "$rest" >> "$work/image_files" ;;
    image_dir) printf '%s\n' "$rest" >> "$work/image_dirs" ;;
    image_link) printf '%s\n' "$rest" >> "$work/image_links" ;;
    *) echo "unknown rootfs policy: $line" >&2; exit 1 ;;
  esac
done < "$policy"
while IFS= read -r dir; do
  [ -n "$dir" ] || continue
  /bin/mkdir -p "$base_tree/$dir"
done < "$work/base_dirs"
while IFS= read -r dir; do
  [ -n "$dir" ] || continue
  /bin/mkdir -p "$state_tree/$dir"
done < "$work/state_dirs"
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
while IFS= read -r spec; do
  [ -n "$spec" ] || continue
  copy_bin "$base_tree/${spec%% *}" "$base_tree/${spec#* }"
done < "$work/copy_bins"
while IFS= read -r spec; do
  [ -n "$spec" ] || continue
  /bin/ln -sf "${spec#* }" "$base_tree/${spec%% *}"
done < "$work/symlinks"
while IFS= read -r path; do
  [ -n "$path" ] || continue
  test -x "$base_tree/$path"
done < "$work/requires"
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
  # Package builds record the cache-equivalence directory name. The id is six
  # characters, so replacing it does not move ELF offsets. nodiscard keeps the
  # zeroed image, and lazy_itable_init=0 writes every inode-table block.
  /usr/bin/python3 -B - "$tree" << 'PY'
import os
import re
import sys
from pathlib import Path
root = Path(sys.argv[1])
text_pattern = re.compile(r"cache-equivalence\.[A-Za-z0-9]{6}")
byte_pattern = re.compile(rb"cache-equivalence\.[A-Za-z0-9]{6}")
replacement = "cache-equivalence.pinned"
for path in sorted(root.rglob("*"), key=lambda item: item.as_posix()):
    if path.is_symlink():
        target = os.readlink(path)
        updated = text_pattern.sub(replacement, target)
        if updated != target:
            path.unlink()
            path.symlink_to(updated)
        continue
    if not path.is_file():
        continue
    data = path.read_bytes()
    updated = byte_pattern.sub(replacement.encode(), data)
    if updated == data:
        continue
    mode = path.stat().st_mode & 0o777
    path.chmod(mode | 0o200)
    try:
        path.write_bytes(updated)
    finally:
        path.chmod(mode)
PY
  SOURCE_DATE_EPOCH=1 E2FSPROGS_FAKE_TIME=1 "$mke2fs" -q -t ext4 -F -m 0 -O ^orphan_file -U clear -L "$label" -E root_owner=0:0,hash_seed=00000000-0000-0000-0000-000000000001,no_copy_xattrs,nodiscard,lazy_itable_init=0 -d "$tree" "$image"
  (cd "$tree" && /usr/bin/find . -print | /usr/bin/sort) > "$work/paths"
  {
    /usr/bin/printf '%s\n' \
      'set_inode_field "/lost+found" uid 0' \
      'set_inode_field "/lost+found" gid 0' \
      'set_inode_field "/lost+found" atime 1' \
      'set_inode_field "/lost+found" ctime 1' \
      'set_inode_field "/lost+found" mtime 1' \
      'set_inode_field "/lost+found" crtime 1' \
      'set_inode_field "/lost+found" atime_extra 0' \
      'set_inode_field "/lost+found" ctime_extra 0' \
      'set_inode_field "/lost+found" mtime_extra 0' \
      'set_inode_field "/lost+found" crtime_extra 0' \
      'set_inode_field <8> atime 1' \
      'set_inode_field <8> ctime 1' \
      'set_inode_field <8> mtime 1' \
      'set_inode_field <8> crtime 1' \
      'set_inode_field <8> atime_extra 0' \
      'set_inode_field <8> ctime_extra 0' \
      'set_inode_field <8> mtime_extra 0' \
      'set_inode_field <8> crtime_extra 0'
    : > "$work/gen-order"
    while IFS= read -r path; do
      [ -n "$path" ] || continue
      if [ "$path" = . ]; then
        rel=/
      else
        rel="${path#.}"
      fi
      /usr/bin/printf 'set_inode_field "%s" uid 0\nset_inode_field "%s" gid 0\nset_inode_field "%s" atime 1\nset_inode_field "%s" ctime 1\nset_inode_field "%s" mtime 1\nset_inode_field "%s" crtime 1\nset_inode_field "%s" atime_extra 0\nset_inode_field "%s" ctime_extra 0\nset_inode_field "%s" mtime_extra 0\nset_inode_field "%s" crtime_extra 0\n' \
        "$rel" "$rel" "$rel" "$rel" "$rel" "$rel" "$rel" "$rel" "$rel" "$rel"
      if [ "$rel" = / ]; then
        depth=0
      else
        depth=$(/usr/bin/printf '%s\n' "$rel" | /usr/bin/awk -F/ '{print NF-1}')
      fi
      /usr/bin/printf '%s\t%s\n' "$depth" "$rel" >> "$work/gen-order"
    done < "$work/paths"
    /usr/bin/printf '1\t/lost+found\n' >> "$work/gen-order"
    /usr/bin/sort -t "$(/usr/bin/printf '\t')" -k1,1nr -k2,2 "$work/gen-order" > "$work/gen-sorted"
    while IFS="$(/usr/bin/printf '\t')" read -r _depth rel; do
      [ -n "$rel" ] || continue
      /usr/bin/printf 'set_inode_field "%s" generation 0\n' "$rel"
      src="$tree$rel"
      if [ "$rel" = /lost+found ] || { [ ! -L "$src" ] && [ -d "$src" ]; }; then
        if [ "$rel" = / ]; then
          temp=/.orlix-csum
        else
          temp="$rel/.orlix-csum"
        fi
        /usr/bin/printf 'mkdir "%s"\nrmdir "%s"\n' "$temp" "$temp"
        /usr/bin/printf 'set_inode_field "%s" atime 1\nset_inode_field "%s" ctime 1\nset_inode_field "%s" mtime 1\nset_inode_field "%s" crtime 1\nset_inode_field "%s" atime_extra 0\nset_inode_field "%s" ctime_extra 0\nset_inode_field "%s" mtime_extra 0\nset_inode_field "%s" crtime_extra 0\n' \
          "$rel" "$rel" "$rel" "$rel" "$rel" "$rel" "$rel" "$rel"
      fi
    done < "$work/gen-sorted"
    /usr/bin/printf '%s\n' \
      'set_inode_field <8> generation 0' \
      'set_super_value mkfs_time 1' \
      'set_super_value wtime 1' \
      'set_super_value lastcheck 1' \
      'set_super_value mtime 1' \
      'set_super_value last_mounted /' \
      'set_super_value journal_uuid 00000000-0000-0000-0000-000000000001' \
      'set_super_value uuid 00000000-0000-0000-0000-000000000000' \
      'set_super_value hash_seed 00000000-0000-0000-0000-000000000001' \
      'set_super_value mnt_count 0' \
      'set_super_value error_count 0' \
      'set_super_value first_error_time 0' \
      'set_super_value last_error_time 0' \
      'set_super_value kbytes_written 0'
  } > "$work/owners"
  SOURCE_DATE_EPOCH=1 E2FSPROGS_FAKE_TIME=1 "$debugfs" -n -w -f "$work/owners" "$image" > "$work/owners.out" 2> "$work/owners.err"
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
policy_image() {
  if [ "$1" = base ]; then printf '%s\n' "$base_ext4"; else printf '%s\n' "$state_ext4"; fi
}
while IFS= read -r spec; do
  [ -n "$spec" ] || continue
  check_image_entry "$(policy_image "${spec%% *}")" "${spec#* }" regular
done < "$work/image_files"
while IFS= read -r spec; do
  [ -n "$spec" ] || continue
  check_image_entry "$(policy_image "${spec%% *}")" "${spec#* }" directory
done < "$work/image_dirs"
while IFS= read -r spec; do
  [ -n "$spec" ] || continue
  image="${spec%% *}"
  tail="${spec#* }"
  entry="${tail%% *}"
  target="${tail#* }"
  check_image_entry "$(policy_image "$image")" "$entry" symlink
  /usr/bin/printf '%s\n' "$entry_stat" | /usr/bin/grep -Fx "Fast link dest: \"$target\""
done < "$work/image_links"
(cd "$base_tree" && /usr/bin/find . -print | /usr/bin/sort) > "$file_manifest"
/bin/cp "$work/metadata" "$payload_metadata"
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
            ctx.file.mke2fs.path,
            ctx.file.debugfs.path,
            ctx.file.mke2fs_config.path,
            ctx.files.rootfs_libs[0].dirname if ctx.files.rootfs_libs else "none",
            ctx.file.assembly_policy.path,
        ],
        inputs = depset(
            direct = pkg_inputs + [
                trees,
                ctx.file.toolchain_identity,
                ctx.file.mke2fs,
                ctx.file.debugfs,
                ctx.file.mke2fs_config,
                ctx.file.assembly_policy,
            ] + ctx.files.rootfs_libs,
        ),
        tools = [ctx.executable.gen_init_cpio],
        outputs = [initramfs, base_ext4, state_ext4, file_manifest, payload_metadata, digest],
        env = _pinned_env(ctx),
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1", "no-sandbox": "1"},
    )
    artifact_identity = declare_artifact_identity(
        ctx,
        "rootfs",
        ctx.file._artifact_identity_serializer,
        artifacts = {
            "base.ext4": base_ext4,
            "file-manifest.txt": file_manifest,
            "initramfs.cpio.gz": initramfs,
            "payload-metadata.txt": payload_metadata,
            "source-input.sha256": digest,
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
    # Copies only the three images. The semantic digest stays on the
    # provider and is not staged. Omitting no-remote-cache leaves this copy
    # disk-cache and BuildBuddy cacheable. Package trees stay on OrlixRootfs.
    ctx.actions.run_shell(
        mnemonic = "OrlixRootfsPayload",
        progress_message = "Staging OrlixOS rootfs payload images",
        command = r"""
set -euo pipefail
/usr/bin/python3 -B "$1" --dest "$2" \
  --image "initramfs.cpio.gz=$3" \
  --image "base.ext4=$4" \
  --image "state.ext4=$5"
""",
        arguments = [
            ctx.file._stage_payload.path,
            root.path,
            info.initramfs.path,
            info.base_ext4.path,
            info.state_ext4.path,
        ],
        inputs = [
            ctx.file._stage_payload,
            info.initramfs,
            info.base_ext4,
            info.state_ext4,
        ],
        outputs = [root],
        env = {
            "PATH": "/usr/bin:/bin",
            "PYTHONDONTWRITEBYTECODE": "1",
        },
        use_default_shell_env = False,
        execution_requirements = {"block-network": "1", "no-remote-exec": "1"},
    )
    return [DefaultInfo(files = depset([root])), info]


orlix_rootfs_payload = rule(
    implementation = _rootfs_payload_impl,
    attrs = {
        "rootfs": attr.label(mandatory = True, providers = [OrlixRootfsInfo]),
        "_stage_payload": attr.label(
            allow_single_file = True,
            default = Label("//bazel/feasibility/rootfs:stage_payload.py"),
        ),
    },
)


orlix_rootfs = rule(
    implementation = _rootfs_impl,
    attrs = {
        "packages": attr.label_list(mandatory = True, providers = [OrlixPackageTreeInfo]),
        "gen_init_cpio": attr.label(allow_single_file = True, executable = True, cfg = "exec", mandatory = True),
        "mke2fs": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/mke2fs")),
        "debugfs": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/bin/debugfs")),
        "mke2fs_config": attr.label(allow_single_file = True, default = Label("@orlix_kernel_toolchain//:tools/mke2fs.conf")),
        "rootfs_libs": attr.label(allow_files = True, default = Label("@orlix_kernel_toolchain//:tool_libs")),
        "toolchain_identity": attr.label(allow_single_file = True, default = "@orlix_kernel_toolchain//:rootfs-identity.json"),
        "assembly_policy": attr.label(
            allow_single_file = True,
            default = Label("//bazel/feasibility/rootfs:assembly_policy.txt"),
        ),
        "_artifact_identity_serializer": attr.label(
            allow_single_file = True,
            default = Label("//bazel:content_digest.py"),
        ),
    },
)
