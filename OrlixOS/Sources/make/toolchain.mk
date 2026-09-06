
$(ORLIXOS_GAWK_TOOLCHAIN_DIR):
	@mkdir -p "$@"

$(ORLIXOS_GAWK_TOOLCHAIN_STAMP): $(PROJECT_DIR)/Sources/make/toolchain.mk $(PROJECT_DIR)/Sources/make/config.mk | $(ORLIXOS_GAWK_TOOLCHAIN_DIR)
	@set -euo pipefail; \
	{ \
		printf '%s\n' '#!/bin/bash'; \
		printf '%s\n' 'set -euo pipefail' ''; \
		printf '%s\n' ': "$${ORLIXOS_CC:?}"' ': "$${ORLIXOS_MLIBC_SYSROOT:?}"' ': "$${ORLIXOS_MLIBC_HEADERS:?}"' ': "$${ORLIXOS_MLIBC_RTLIB:?}"' ': "$${ORLIXOS_HOSTED_USER_BASE_ADDRESS:?}"' ': "$${ORLIXOS_PACKAGE_TOOLCHAIN_DIR:?}"' ': "$${ORLIXOS_PACKAGE_CODE_MODEL_FLAG:?}"' ''; \
		printf '%s\n' 'unset SDKROOT IPHONEOS_DEPLOYMENT_TARGET TVOS_DEPLOYMENT_TARGET WATCHOS_DEPLOYMENT_TARGET' ''; \
		printf '%s\n' 'export SDKROOT="$$ORLIXOS_MLIBC_SYSROOT"' ''; \
		printf '%s\n' 'link=1' 'program_link=1' 'output=' 'next_output=0' ''; \
		printf '%s\n' 'for arg in "$$@"; do' \
			'if [ "$$next_output" -eq 1 ]; then output="$$arg"; next_output=0; continue; fi' \
			'case "$$arg" in' \
			'-c|-E|-S|-print-search-dirs|-print-prog-name=*) link=0 ;;' \
			'-r|-shared) program_link=0 ;;' \
			'-o) next_output=1 ;;' \
			'esac' \
			'done' ''; \
		printf '%s\n' 'common=(--target=aarch64-linux-gnu -B "$$ORLIXOS_PACKAGE_TOOLCHAIN_DIR" "--sysroot=$$ORLIXOS_MLIBC_SYSROOT" "-isysroot=$$ORLIXOS_MLIBC_SYSROOT" -isystem "$$ORLIXOS_MLIBC_HEADERS" -isystem "$$ORLIXOS_MLIBC_SYSROOT/usr/include" -D_GNU_SOURCE -fhosted -fno-builtin -ffixed-x18 "$$ORLIXOS_PACKAGE_CODE_MODEL_FLAG")' ''; \
		printf '%s\n' 'if [ "$$link" -eq 1 ] && [ "$$program_link" -eq 1 ] && [ "$${output##*.}" != la ]; then' \
			'exec "$$ORLIXOS_CC" "$${common[@]}" "$$@" -static-pie -fuse-ld=lld -nostdlib -Wl,--gc-sections -Wl,-z,max-page-size=0x4000 "$$ORLIXOS_MLIBC_SYSROOT/usr/lib/crt1.o" "$$ORLIXOS_MLIBC_SYSROOT/usr/lib/crti.o" -Wl,--start-group "$$ORLIXOS_MLIBC_SYSROOT/usr/lib/libc.a" "$$ORLIXOS_MLIBC_SYSROOT/usr/lib/libm.a" "$$ORLIXOS_MLIBC_SYSROOT/usr/lib/libpthread.a" "$$ORLIXOS_MLIBC_SYSROOT/usr/lib/libssp_nonshared.a" "$$ORLIXOS_MLIBC_SYSROOT/usr/lib/libssp.a" "$$ORLIXOS_MLIBC_RTLIB" -Wl,--end-group "$$ORLIXOS_MLIBC_SYSROOT/usr/lib/crtn.o"' \
			'fi' ''; \
		printf '%s\n' 'if [ "$$link" -eq 1 ]; then exec "$$ORLIXOS_CC" "$${common[@]}" "$$@" -fuse-ld=lld -nostdlib; fi' ''; \
		printf '%s\n' 'exec "$$ORLIXOS_CC" "$${common[@]}" "$$@"'; \
	} > "$(ORLIXOS_GAWK_TOOLCHAIN_DIR)/aarch64-linux-gnu-gcc"; \
	chmod +x "$(ORLIXOS_GAWK_TOOLCHAIN_DIR)/aarch64-linux-gnu-gcc"; \
	ln -sf "$(ORLIXOS_LD)" "$(ORLIXOS_GAWK_TOOLCHAIN_DIR)/ld"; \
	ln -sf "$(ORLIXOS_LD)" "$(ORLIXOS_GAWK_TOOLCHAIN_DIR)/aarch64-linux-gnu-ld"; \
	ln -sf "$(ORLIXOS_AR)" "$(ORLIXOS_GAWK_TOOLCHAIN_DIR)/aarch64-linux-gnu-ar"; \
	ln -sf "$(ORLIXOS_RANLIB)" "$(ORLIXOS_GAWK_TOOLCHAIN_DIR)/aarch64-linux-gnu-ranlib"; \
	ln -sf "$(ORLIXOS_NM)" "$(ORLIXOS_GAWK_TOOLCHAIN_DIR)/aarch64-linux-gnu-nm"; \
	ln -sf "$(ORLIXOS_STRIP)" "$(ORLIXOS_GAWK_TOOLCHAIN_DIR)/aarch64-linux-gnu-strip"; \
	ln -sf "$(ORLIXOS_OBJDUMP)" "$(ORLIXOS_GAWK_TOOLCHAIN_DIR)/aarch64-linux-gnu-objdump"; \
	ln -sf "$(ORLIXOS_READELF)" "$(ORLIXOS_GAWK_TOOLCHAIN_DIR)/aarch64-linux-gnu-readelf"; \
	touch "$@"

$(ORLIXOS_MLIBC_SYSROOT)/.orlixmlibc-sysroot-ready $(ORLIXOS_MLIBC_RTLIB): $(REPO_ROOT)/OrlixMLibC/Makefile $(ORLIXOS_TARGET_SETTINGS) $(ORLIXOS_MLIBC_PATCHES)
	@set -euo pipefail; \
	$(MLIBC_MAKE) build PROFILE="$(PROFILE)"; \
	[ -s "$(ORLIXOS_MLIBC_SYSROOT)/usr/lib/libc.a" ] || { echo "missing OrlixMLibC libc archive: $(ORLIXOS_MLIBC_SYSROOT)/usr/lib/libc.a" >&2; exit 1; }; \
	[ -s "$(ORLIXOS_MLIBC_RTLIB)" ] || { echo "missing Orlix compiler runtime archive: $(ORLIXOS_MLIBC_RTLIB)" >&2; exit 1; }
