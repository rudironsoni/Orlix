KBUILD_DEFCONFIG := defconfig

core-y += arch/orlix/kernel/
core-y += arch/orlix/mm/
core-y += arch/orlix/boot/
core-y += arch/orlix/hosted_exec/

# Orlix product proof is app-hosted. A manual standalone vmlinux
# invocation is retained only as an explicit Linux Kbuild compatibility
# check; the default Orlix Kbuild path prepares generated state only.
all: prepare
