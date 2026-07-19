$(ORLIXOS_INIT_BINARY): $(ORLIXOS_INIT_SOURCE) $(ORLIXOS_CONSOLE_POLICY_SOURCE) $(ORLIXOS_CONSOLE_POLICY_HEADER) $(ORLIXOS_TERMINAL_MUX_SOURCE) $(ORLIXOS_TERMINAL_MUX_HEADER) $(ORLIXOS_MLIBC_SYSROOT)/.orlixmlibc-sysroot-ready $(ORLIXOS_MLIBC_RTLIB) $(PROJECT_DIR)/Sources/make/rootfs.mk
	@set -euo pipefail; \
	sysroot="$(ORLIXOS_MLIBC_SYSROOT)"; \
	headers="$(ORLIXOS_MLIBC_HEADERS)"; \
	rtlib="$(ORLIXOS_MLIBC_RTLIB)"; \
	[ -s "$$sysroot/usr/lib/libc.a" ] || { echo "missing OrlixMLibC libc archive: $$sysroot/usr/lib/libc.a" >&2; exit 1; }; \
	[ -d "$$headers" ] || { echo "missing Orlix Linux UAPI headers: $$headers" >&2; exit 1; }; \
	[ -s "$$rtlib" ] || { echo "missing Orlix compiler runtime archive: $$rtlib" >&2; exit 1; }; \
	command -v "$(ORLIXOS_CC)" >/dev/null 2>&1 || { echo "clang is required to build Orlix init; set ORLIXOS_CC=/path/to/clang" >&2; exit 1; }; \
	command -v "$(ORLIXOS_STRIP)" >/dev/null 2>&1 || { echo "llvm-strip is required to package Orlix init; set ORLIXOS_STRIP=/path/to/llvm-strip" >&2; exit 1; }; \
	command -v "$(ORLIXOS_OBJDUMP)" >/dev/null 2>&1 || { echo "llvm-objdump is required to inspect Orlix init; set ORLIXOS_OBJDUMP=/path/to/llvm-objdump" >&2; exit 1; }; \
	mkdir -p "$(dir $(ORLIXOS_INIT_BINARY))"; \
	"$(ORLIXOS_CC)" --target=aarch64-linux-gnu --sysroot="$$sysroot" -isystem "$$headers" -D_GNU_SOURCE -std=c17 -O2 -fhosted -fno-builtin -ffixed-x18 -fPIE -static-pie -fuse-ld=lld -nostdlib -Wl,--gc-sections -Wl,-z,max-page-size=0x4000 "$$sysroot/usr/lib/crt1.o" "$$sysroot/usr/lib/crti.o" "$(ORLIXOS_INIT_SOURCE)" "$(ORLIXOS_CONSOLE_POLICY_SOURCE)" "$(ORLIXOS_TERMINAL_MUX_SOURCE)" -Wl,--start-group "$$sysroot/usr/lib/libc.a" "$$sysroot/usr/lib/libm.a" "$$sysroot/usr/lib/libpthread.a" "$$sysroot/usr/lib/libssp_nonshared.a" "$$sysroot/usr/lib/libssp.a" "$$rtlib" -Wl,--end-group "$$sysroot/usr/lib/crtn.o" -o "$(ORLIXOS_INIT_BINARY)"; \
	"$(ORLIXOS_STRIP)" "$(ORLIXOS_INIT_BINARY)"; \
	file "$(ORLIXOS_INIT_BINARY)" | grep -F -q 'ELF 64-bit LSB pie executable, ARM aarch64' || { file "$(ORLIXOS_INIT_BINARY)" >&2; exit 1; }; \
	"$(ORLIXOS_OBJDUMP)" -p "$(ORLIXOS_INIT_BINARY)" | awk '/^[[:space:]]*LOAD[[:space:]]/ && $$0 !~ /align 2\*\*14/ { print "Orlix init PT_LOAD is not 16 KiB aligned: " $$0 > "/dev/stderr"; bad=1 } END { exit bad }'; \
	printf 'profile=%s\ndistribution=%s\nchannel=%s\nprogram=init\ntransport=linux-console-policy\nterminal=devpts-pty\nshell=/bin/sh\n' "$(PROFILE)" "$(ORLIXOS_DISTRIBUTION_ID)" "$(ORLIXOS_DISTRIBUTION_CHANNEL)" > "$(ORLIXOS_PACKAGE_INSTALL_DIR)/init.stamp"; \
	echo "built OrlixOS first-stage init: $(ORLIXOS_INIT_BINARY)"

$(ORLIXOS_ROOT_INIT_BINARY): $(ORLIXOS_ROOT_INIT_SOURCE) $(ORLIXOS_MLIBC_SYSROOT)/.orlixmlibc-sysroot-ready $(ORLIXOS_MLIBC_RTLIB) $(PROJECT_DIR)/Sources/make/rootfs.mk
	@set -euo pipefail; \
	sysroot="$(ORLIXOS_MLIBC_SYSROOT)"; \
	headers="$(ORLIXOS_MLIBC_HEADERS)"; \
	rtlib="$(ORLIXOS_MLIBC_RTLIB)"; \
	[ -s "$$sysroot/usr/lib/libc.a" ] || { echo "missing OrlixMLibC libc archive: $$sysroot/usr/lib/libc.a" >&2; exit 1; }; \
	[ -d "$$headers" ] || { echo "missing Orlix Linux UAPI headers: $$headers" >&2; exit 1; }; \
	[ -s "$$rtlib" ] || { echo "missing Orlix compiler runtime archive: $$rtlib" >&2; exit 1; }; \
	command -v "$(ORLIXOS_CC)" >/dev/null 2>&1 || { echo "clang is required to build Orlix root init; set ORLIXOS_CC=/path/to/clang" >&2; exit 1; }; \
	command -v "$(ORLIXOS_STRIP)" >/dev/null 2>&1 || { echo "llvm-strip is required to package Orlix root init; set ORLIXOS_STRIP=/path/to/llvm-strip" >&2; exit 1; }; \
	command -v "$(ORLIXOS_OBJDUMP)" >/dev/null 2>&1 || { echo "llvm-objdump is required to inspect Orlix root init; set ORLIXOS_OBJDUMP=/path/to/llvm-objdump" >&2; exit 1; }; \
	mkdir -p "$(dir $(ORLIXOS_ROOT_INIT_BINARY))"; \
	"$(ORLIXOS_CC)" --target=aarch64-linux-gnu --sysroot="$$sysroot" -isystem "$$headers" -D_GNU_SOURCE -std=c17 -O2 -fhosted -fno-builtin -ffixed-x18 -fPIE -static-pie -fuse-ld=lld -nostdlib -Wl,--gc-sections -Wl,-z,max-page-size=0x4000 "$$sysroot/usr/lib/crt1.o" "$$sysroot/usr/lib/crti.o" "$(ORLIXOS_ROOT_INIT_SOURCE)" -Wl,--start-group "$$sysroot/usr/lib/libc.a" "$$sysroot/usr/lib/libm.a" "$$sysroot/usr/lib/libpthread.a" "$$sysroot/usr/lib/libssp_nonshared.a" "$$sysroot/usr/lib/libssp.a" "$$rtlib" -Wl,--end-group "$$sysroot/usr/lib/crtn.o" -o "$(ORLIXOS_ROOT_INIT_BINARY)"; \
	"$(ORLIXOS_STRIP)" "$(ORLIXOS_ROOT_INIT_BINARY)"; \
	file "$(ORLIXOS_ROOT_INIT_BINARY)" | grep -F -q 'ELF 64-bit LSB pie executable, ARM aarch64' || { file "$(ORLIXOS_ROOT_INIT_BINARY)" >&2; exit 1; }; \
	"$(ORLIXOS_OBJDUMP)" -p "$(ORLIXOS_ROOT_INIT_BINARY)" | awk '/^[[:space:]]*LOAD[[:space:]]/ && $$0 !~ /align 2\*\*14/ { print "Orlix root init PT_LOAD is not 16 KiB aligned: " $$0 > "/dev/stderr"; bad=1 } END { exit bad }'; \
	printf 'profile=%s\ndistribution=%s\nchannel=%s\nprogram=rootinit\nroot_mode=%s\n' "$(PROFILE)" "$(ORLIXOS_DISTRIBUTION_ID)" "$(ORLIXOS_DISTRIBUTION_CHANNEL)" "$(ORLIXOS_PROFILE_ROOT_MODE)" > "$(ORLIXOS_PACKAGE_INSTALL_DIR)/rootinit.stamp"; \
	echo "built OrlixOS root initramfs init: $(ORLIXOS_ROOT_INIT_BINARY)"

$(ORLIXOS_INITRAMFS_CPIO): $(ORLIXOS_ROOT_INIT_BINARY) $(ORLIXOS_MANIFEST) $(PROJECT_DIR)/Sources/make/rootfs.mk
	@set -euo pipefail; \
	$(KERNEL_MAKE) prepare PROFILE="$(PROFILE)" >/dev/null; \
	gen_init_cpio="$(ORLIX_BUILD_ROOT)/OrlixKernel/build/$(PROFILE)/usr/gen_init_cpio"; \
	output="$(ORLIXOS_INITRAMFS_CPIO)"; \
	cpio_list="$(ORLIXOS_ROOTFS_DIR)/initramfs.list"; \
	case "$$output" in "$(ORLIX_BUILD_ROOT)"/OrlixOS/rootfs/*/rootfs/initramfs.cpio.gz) ;; *) echo "refusing to write OrlixOS initramfs outside configured OrlixOS rootfs build root: $$output" >&2; exit 1 ;; esac; \
	[ -x "$$gen_init_cpio" ] || { echo "missing Linux gen_init_cpio: $$gen_init_cpio" >&2; exit 1; }; \
	mkdir -p "$(ORLIXOS_INITRAMFS_DIR)"; \
	{ \
		printf 'dir /dev 0755 0 0\n'; \
		printf 'nod /dev/console 0600 0 0 c 5 1\n'; \
		printf 'dir /root 0700 0 0\n'; \
		printf 'file /init %s 0755 0 0\n' "$(ORLIXOS_ROOT_INIT_BINARY)"; \
	} > "$$cpio_list"; \
	"$$gen_init_cpio" "$$cpio_list" > "$$output.tmp"; \
	gzip -n -c "$$output.tmp" > "$$output"; \
	rm -f "$$output.tmp"; \
	[ -s "$$output" ] || { echo "missing generated OrlixOS initramfs: $$output" >&2; exit 1; }; \
	echo "built OrlixOS product initramfs: $$output"

$(ORLIXOS_ROOTFS_STAMP): $(ORLIXOS_BASH_BINARY) $(ORLIXOS_COREUTILS_STAMP) $(ORLIXOS_GREP_BINARY) $(ORLIXOS_FINDUTILS_STAMP) $(ORLIXOS_GETCONF_BINARY) $(ORLIXOS_MKE2FS_BINARY) $(ORLIXOS_MKFS_EXT4_BINARY) $(ORLIXOS_DEBUGFS_BINARY) $(ORLIXOS_E2FSCK_BINARY) $(ORLIXOS_INIT_BINARY) $(ORLIXOS_INITRAMFS_CPIO) $(ORLIXOS_MANIFEST) $(ORLIXOS_TARGET_SETTINGS) $(ORLIX_PROJECT_YML)
	@set -euo pipefail; \
	[ -n "$(ORLIX_PRODUCT_VERSION)" ] || { echo "project.yml lacks MARKETING_VERSION" >&2; exit 1; }; \
	case "$(ORLIX_PRODUCT_BUILD_ID)" in ''|*[!0-9]*) echo "project.yml CURRENT_PROJECT_VERSION must be an integer" >&2; exit 1 ;; esac; \
	root_tree="$(ORLIXOS_BASE_ROOT_TREE)"; \
	state_tree="$(ORLIXOS_STATE_ROOT_TREE)"; \
	case "$$root_tree" in "$(ORLIX_BUILD_ROOT)"/OrlixOS/rootfs/*/base-tree) ;; *) echo "refusing to write OrlixOS root tree outside configured OrlixOS rootfs build root: $$root_tree" >&2; exit 1 ;; esac; \
	case "$$state_tree" in "$(ORLIX_BUILD_ROOT)"/OrlixOS/rootfs/*/state-tree) ;; *) echo "refusing to write OrlixOS state tree outside configured OrlixOS rootfs build root: $$state_tree" >&2; exit 1 ;; esac; \
	for path in "$(ORLIX_BUILD_ROOT)" "$(ORLIXOS_BUILD_ROOT)" "$(ORLIXOS_ROOTFS_DIR)" "$$root_tree" "$$state_tree"; do \
		if [ -L "$$path" ]; then echo "refusing to package OrlixOS root tree through symlinked path: $$path" >&2; exit 1; fi; \
	done; \
	rm -rf "$$root_tree" "$$state_tree"; \
	mkdir -p "$$root_tree/bin" "$$root_tree/dev" "$$root_tree/etc" "$$root_tree/proc" "$$root_tree/root" "$$root_tree/run" "$$root_tree/sbin" "$$root_tree/sys" "$$root_tree/tmp" "$$root_tree/usr/bin" "$$root_tree/usr/share/orlixos" "$$root_tree/var/tmp"; \
	mkdir -p "$$state_tree/upper" "$$state_tree/work"; \
	install -m 0755 "$(ORLIXOS_BASH_BINARY)" "$$root_tree/bin/bash"; \
	install -m 0755 "$(ORLIXOS_GREP_BINARY)" "$$root_tree/bin/grep"; \
	for program in $(ORLIXOS_COREUTILS_PROGRAMS); do install -m 0755 "$(ORLIXOS_PACKAGE_INSTALL_DIR)/usr/bin/$$program" "$$root_tree/bin/$$program"; done; \
	for program in $(ORLIXOS_FINDUTILS_PROGRAMS); do install -m 0755 "$(ORLIXOS_PACKAGE_INSTALL_DIR)/usr/bin/$$program" "$$root_tree/bin/$$program"; done; \
	install -m 0755 "$(ORLIXOS_MKE2FS_BINARY)" "$$root_tree/bin/mke2fs"; \
	install -m 0755 "$(ORLIXOS_MKFS_EXT4_BINARY)" "$$root_tree/bin/mkfs.ext4"; \
	install -m 0755 "$(ORLIXOS_DEBUGFS_BINARY)" "$$root_tree/bin/debugfs"; \
	install -m 0755 "$(ORLIXOS_E2FSCK_BINARY)" "$$root_tree/bin/e2fsck"; \
	install -m 0755 "$(ORLIXOS_GETCONF_BINARY)" "$$root_tree/usr/bin/getconf"; \
	install -m 0755 "$(ORLIXOS_INIT_BINARY)" "$$root_tree/sbin/init"; \
	ln -s bash "$$root_tree/bin/sh"; \
	ln -s ../../bin/grep "$$root_tree/usr/bin/grep"; \
	printf '%s\n' 'root:x:0:0:root:/root:/bin/sh' > "$$root_tree/etc/passwd"; \
	printf '%s\n' 'root:x:0:root' > "$$root_tree/etc/group"; \
	printf '%s\n' 'NAME=$(ORLIXOS_DISTRIBUTION_NAME)' 'ID=$(ORLIXOS_DISTRIBUTION_ID)' 'PRETTY_NAME=$(ORLIXOS_DISTRIBUTION_NAME)' > "$$root_tree/etc/os-release"; \
	printf '%s\n' 'ORLIX-TCTI-PACKAGE-BEHAVIOR-OK' > "$$root_tree/usr/share/orlixos/package-behavior.txt"; \
	{ \
		printf 'distribution=%s\n' "$(ORLIXOS_DISTRIBUTION_ID)"; \
		printf 'product_version=%s\n' "$(ORLIX_PRODUCT_VERSION)"; \
		printf 'product_build_id=%s\n' "$(ORLIX_PRODUCT_BUILD_ID)"; \
		printf 'profile=%s\n' "$(PROFILE)"; \
		printf 'channel=%s\n' "$(ORLIXOS_DISTRIBUTION_CHANNEL)"; \
		printf 'root_modes=%s\n' "$(ORLIXOS_ROOT_MODES)"; \
		printf 'selected_root_mode=%s\n' "$(ORLIXOS_PROFILE_ROOT_MODE)"; \
		printf 'base_root_device=%s\n' "$(ORLIXOS_BASE_ROOT_DEVICE)"; \
		printf 'state_root_device=%s\n' "$(ORLIXOS_STATE_ROOT_DEVICE)"; \
		printf 'packages=%s\n' "$(ORLIXOS_PROFILE_PACKAGES)"; \
		printf 'downloaded_binary_repositories=%s\n' "$(ORLIXOS_DOWNLOADED_BINARY_REPOSITORIES)"; \
	} > "$$root_tree/usr/share/orlixos/distribution.manifest"; \
	chmod 0755 "$$root_tree" "$$root_tree/bin" "$$root_tree/dev" "$$root_tree/etc" "$$root_tree/proc" "$$root_tree/run" "$$root_tree/sbin" "$$root_tree/sys" "$$root_tree/usr" "$$root_tree/usr/bin" "$$root_tree/usr/share" "$$root_tree/usr/share/orlixos" "$$root_tree/var"; \
	chmod 0700 "$$root_tree/root"; \
	chmod 1777 "$$root_tree/tmp" "$$root_tree/var/tmp"; \
	chmod 0755 "$$state_tree" "$$state_tree/upper" "$$state_tree/work"; \
	printf 'profile=%s\nproduct_version=%s\nproduct_build_id=%s\ndistribution=%s\nchannel=%s\nroot_modes=%s\nselected_root_mode=%s\nbase_root_device=%s\nstate_root_device=%s\ninitramfs=%s\nbase_root_tree=%s\nstate_root_tree=%s\ninit=/sbin/init\ntransport=linux-console-policy\nterminal=devpts-pty\nshell=/bin/sh\nbase_packages=bash coreutils grep findutils e2fsprogs\ncoreutils_programs=%s\nfindutils_programs=%s\ne2fsprogs_programs=mke2fs mkfs.ext4 debugfs e2fsck\nbash_version=%s\ncoreutils_version=%s\ngrep_version=%s\nfindutils_version=%s\npackage_behavior_marker=/usr/share/orlixos/package-behavior.txt\n' "$(PROFILE)" "$(ORLIX_PRODUCT_VERSION)" "$(ORLIX_PRODUCT_BUILD_ID)" "$(ORLIXOS_DISTRIBUTION_ID)" "$(ORLIXOS_DISTRIBUTION_CHANNEL)" "$(ORLIXOS_ROOT_MODES)" "$(ORLIXOS_PROFILE_ROOT_MODE)" "$(ORLIXOS_BASE_ROOT_DEVICE)" "$(ORLIXOS_STATE_ROOT_DEVICE)" "$(ORLIXOS_INITRAMFS_CPIO)" "$$root_tree" "$$state_tree" "$(ORLIXOS_COREUTILS_PROGRAMS)" "$(ORLIXOS_FINDUTILS_PROGRAMS)" "$(BASH_VERSION)" "$(COREUTILS_VERSION)" "$(GREP_VERSION)" "$(FINDUTILS_VERSION)" > "$(ORLIXOS_ROOTFS_STAMP)"; \
	echo "built OrlixOS base root tree: $$root_tree"
