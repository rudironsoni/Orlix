# FP16 scalar and AdvSIMD instructions use host H registers in these units.
CFLAGS_switch_debug.o += -march=armv8.2-a+fp16
CFLAGS_fixed_fp.o += -march=armv8.2-a+fp16

obj-y += engine.o
obj-y += report.o
obj-y += switch_debug.o
obj-y += fixed_fp.o
obj-y += sve_state.o
obj-y += sve_decode.o
obj-y += decode_aarch64.o
obj-y += tls_repair.o
obj-y += gadget_program.o
obj-y += block_cache.o
obj-y += tlb.o
obj-$(CONFIG_ORLIX_TCTI_KUNIT_TEST) += tests/
