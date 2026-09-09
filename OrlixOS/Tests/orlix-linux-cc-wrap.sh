#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Host clang wrapper that emits OrlixMLibC aarch64 Linux objects or static-pie ELFs.
set -eu

cc=${ORLIXOS_LINUX_CC:?ORLIXOS_LINUX_CC is required}
sysroot=${ORLIXOS_LINUX_SYSROOT:?ORLIXOS_LINUX_SYSROOT is required}
headers=${ORLIXOS_LINUX_HEADERS:?ORLIXOS_LINUX_HEADERS is required}
rtlib=${ORLIXOS_LINUX_RTLIB:?ORLIXOS_LINUX_RTLIB is required}

compile_only=0
for arg in "$@"; do
	case "$arg" in
	-c|-E|-S)
		compile_only=1
		;;
	esac
done

if [ "$compile_only" = 1 ]; then
	exec "$cc" --target=aarch64-linux-gnu --sysroot="$sysroot" -isystem "$headers" \
		-D_GNU_SOURCE -fhosted -fno-builtin -ffixed-x18 -fPIE "$@"
fi

exec "$cc" --target=aarch64-linux-gnu --sysroot="$sysroot" -isystem "$headers" \
	-D_GNU_SOURCE -fhosted -fno-builtin -ffixed-x18 -fPIE -static-pie -fuse-ld=lld \
	-nostdlib -Wl,--gc-sections -Wl,-z,max-page-size=0x4000 \
	-L"$sysroot/usr/lib" "$sysroot/usr/lib/crt1.o" "$sysroot/usr/lib/crti.o" \
	-Wl,--start-group "$@" \
	"$sysroot/usr/lib/libc.a" "$sysroot/usr/lib/libm.a" "$sysroot/usr/lib/libpthread.a" \
	"$sysroot/usr/lib/libdl.a" "$sysroot/usr/lib/libssp_nonshared.a" "$sysroot/usr/lib/libssp.a" "$rtlib" \
	-Wl,--end-group "$sysroot/usr/lib/crtn.o"
