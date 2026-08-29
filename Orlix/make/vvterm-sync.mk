# Exact-source vvterm synchronization. Public entry points are proxied by the root Makefile.

VVTERM_SYNC_TOOL := $(PROJECT_DIR)/make/vvterm_sync.py
VVTERM_COMMIT ?=
VVTERM_RESOLUTION_PATH := $(if $(strip $(VVTERM_RESOLUTION)),$(VVTERM_RESOLUTION),$(PROJECT_DIR)/make/vvterm-resolutions/$(VVTERM_COMMIT).json)
VVTERM_TEST_DESTINATION ?= platform=iOS Simulator,name=iPhone 17 Pro

.PHONY: vvterm-sync vvterm-sync-resolve vvterm-reconcile vvterm-sync-complete vvterm-source-check vvterm-sync-tests vvterm-upstream-tests

vvterm-sync:
	@python3 "$(VVTERM_SYNC_TOOL)" sync --commit "$(VVTERM_COMMIT)"

vvterm-sync-resolve:
	@python3 "$(VVTERM_SYNC_TOOL)" resolve --manifest "$(VVTERM_RESOLUTION_PATH)"

vvterm-reconcile:
	@python3 "$(VVTERM_SYNC_TOOL)" reconcile

vvterm-sync-complete:
	@python3 "$(VVTERM_SYNC_TOOL)" complete --commit "$(VVTERM_COMMIT)"

vvterm-source-check:
	@python3 "$(VVTERM_SYNC_TOOL)" check

vvterm-sync-tests:
	@python3 -m unittest discover -s "$(PROJECT_DIR)/make/tests" -p 'test_vvterm_sync.py'

vvterm-upstream-tests:
	@python3 "$(VVTERM_SYNC_TOOL)" upstream-tests --destination "$(VVTERM_TEST_DESTINATION)"
