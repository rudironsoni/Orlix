# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_ORLIX_TCTI_KUNIT_TEST) += orlix_tcti_test.o
orlix_tcti_test-y := tcti_decode_test.o runtime_projection.o \
	tcti_atomic_memory_test.o tcti_lse_decode_test.o tcti_uaccess_test.o \
	tcti_cssc_min_max_immediate_test.o \
	tcti_crypto_decode_boundary_test.o \
	tcti_crc32_decode_boundary_test.o \
	tcti_crypto_extension_decode_boundary_test.o \
	tcti_atomic_128_rmw_test.o \
	tcti_lse_source_bound_test.o \
	tcti_lse128_noncas_test.o \
	tcti_logical_shifted_register_test.o \
	tcti_bitfield_extract_source_bound_test.o

hostprogs := gen_inventory

quiet_cmd_tcti_inventory = GEN     $@
      cmd_tcti_inventory = $(obj)/gen_inventory $@

targets += inventory.h
clean-files += inventory.h

$(obj)/inventory.h: $(obj)/gen_inventory \
			    $(src)/../isa/inventory.def FORCE
	$(call if_changed,tcti_inventory)

$(obj)/tcti_decode_test.o: $(obj)/inventory.h

ccflags-y += -I$(obj)
