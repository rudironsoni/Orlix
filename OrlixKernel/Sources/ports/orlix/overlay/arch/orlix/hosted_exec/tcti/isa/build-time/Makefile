# SPDX-License-Identifier: GPL-2.0-only
#
# Kernel-owned maintainer lane for the pinned AARCHMRS target inventory.
# Normal TCTI builds consume checked C artifacts only.  This lane is explicit
# because it reads the pinned Arm source to regenerate or verify those
# artifacts.

CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic
ORLIX_BUILD_ROOT ?= ../../../../../../../../../Build
OUT ?= $(abspath $(ORLIX_BUILD_ROOT)/OrlixKernel/tcti-isa-maintainer)

INSTRUCTIONS ?= $(ORLIX_AARCHMRS_INSTRUCTIONS)
.PHONY: import-test classification-seed

import-test:
	@test -n '$(INSTRUCTIONS)' || { echo 'ORLIX_AARCHMRS_INSTRUCTIONS is required' >&2; exit 2; }
	@mkdir -p '$(OUT)'
	@$(CC) $(CFLAGS) target_inventory_import.c target_inventory_import_test.c \
		-o '$(OUT)/target_inventory_import_test'
	@'$(OUT)/target_inventory_import_test' '$(INSTRUCTIONS)'

classification-seed:
	@test -n '$(INSTRUCTIONS)' || { echo 'ORLIX_AARCHMRS_INSTRUCTIONS is required' >&2; exit 2; }
	@mkdir -p '$(OUT)'
	@$(CC) $(CFLAGS) target_inventory_import.c target_classification_generator.c \
		-o '$(OUT)/target_classification_generator'
	@'$(OUT)/target_classification_generator' '$(INSTRUCTIONS)' \
		> '$(OUT)/target_classification.generated.def'
