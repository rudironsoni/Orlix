# OrlixOS payload staging implementation. Public workflows enter through Make.

.PHONY: __sync-payload __payload-sync-tests

__sync-payload:
	@set -euo pipefail; \
	source_payload="$(ORLIX_PAYLOAD_SOURCE)"; \
	destination="$(ORLIX_PAYLOAD_DESTINATION)"; \
	[ -n "$$source_payload" ] || { echo "ORLIX_PAYLOAD_SOURCE is required" >&2; exit 2; }; \
	[ -n "$$destination" ] || { echo "ORLIX_PAYLOAD_DESTINATION is required" >&2; exit 2; }; \
	source_stamp="$$source_payload/.orlix-payload-ready"; \
	destination_stamp="$$destination/.orlix-payload-ready"; \
	source_manifest="$$source_payload/OrlixOSManifest.plist"; \
	destination_manifest="$$destination/OrlixOSManifest.plist"; \
	[ -s "$$source_stamp" ] || { echo "missing payload identity stamp: $$source_stamp" >&2; exit 1; }; \
	[ -s "$$source_manifest" ] || { echo "missing payload manifest: $$source_manifest" >&2; exit 1; }; \
	if [ -s "$$destination_stamp" ] && [ -s "$$destination_manifest" ] && [ -d "$$destination/rootfs" ] && [ -d "$$destination/arch" ] && cmp -s "$$source_stamp" "$$destination_stamp"; then \
		echo "reusing unchanged embedded OrlixOS resources: $$destination"; exit 0; \
	fi; \
	temporary="$$destination.tmp.$$$$"; \
	trap 'rm -rf "$$temporary"' EXIT; \
	rm -rf "$$temporary"; \
	mkdir -p "$$temporary"; \
	/usr/bin/rsync -a --delete "$$source_payload/" "$$temporary/"; \
	cmp -s "$$source_stamp" "$$temporary/.orlix-payload-ready" || { echo "copied payload identity does not match source" >&2; exit 1; }; \
	mkdir -p "$$destination"; \
	/usr/bin/rsync -a --checksum --delete "$$temporary/rootfs/" "$$destination/rootfs/"; \
	/usr/bin/rsync -a --checksum --delete "$$temporary/arch/" "$$destination/arch/"; \
	install -m 0644 "$$temporary/OrlixOSManifest.plist" "$$destination_manifest"; \
	install -m 0644 "$$temporary/.orlix-payload-ready" "$$destination_stamp"; \
	echo "embedded direct OrlixOS resources: $$destination"; \
	trap - EXIT; \
	rm -rf "$$temporary"

__payload-sync-tests:
	@set -euo pipefail; \
	tmp="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-payload-sync-test.XXXXXX")"; \
	trap 'rm -rf "$$tmp"' EXIT; \
	source_payload="$$tmp/source"; destination="$$tmp/destination"; \
	mkdir -p "$$source_payload/rootfs" "$$source_payload/arch/orlix" "$$destination"; \
	printf 'identity=one\n' > "$$source_payload/.orlix-payload-ready"; \
	printf 'manifest-one\n' > "$$source_payload/OrlixOSManifest.plist"; \
	printf 'payload-one\n' > "$$source_payload/rootfs/base.ext4"; \
	printf 'architecture-one\n' > "$$source_payload/arch/orlix/identity"; \
	printf 'framework-owned\n' > "$$destination/preserved"; \
	$(MAKE) --no-print-directory -f "$(PROJECT_DIR)/Makefile" __sync-payload ORLIX_PAYLOAD_SOURCE="$$source_payload" ORLIX_PAYLOAD_DESTINATION="$$destination" >/dev/null; \
	[ -f "$$destination/OrlixOSManifest.plist" ] && [ ! -e "$$destination/Info.plist" ] && [ -f "$$destination/preserved" ]; \
	inode_before="$$(stat -f '%i' "$$destination/rootfs/base.ext4")"; mtime_before="$$(stat -f '%m' "$$destination/rootfs/base.ext4")"; \
	$(MAKE) --no-print-directory -f "$(PROJECT_DIR)/Makefile" __sync-payload ORLIX_PAYLOAD_SOURCE="$$source_payload" ORLIX_PAYLOAD_DESTINATION="$$destination" >/dev/null; \
	[ "$$(stat -f '%i' "$$destination/rootfs/base.ext4")" = "$$inode_before" ]; \
	[ "$$(stat -f '%m' "$$destination/rootfs/base.ext4")" = "$$mtime_before" ]; \
	printf 'identity=two\n' > "$$source_payload/.orlix-payload-ready"; \
	printf 'manifest-two\n' > "$$source_payload/OrlixOSManifest.plist"; \
	printf 'payload-two\n' > "$$source_payload/rootfs/base.ext4"; \
	$(MAKE) --no-print-directory -f "$(PROJECT_DIR)/Makefile" __sync-payload ORLIX_PAYLOAD_SOURCE="$$source_payload" ORLIX_PAYLOAD_DESTINATION="$$destination" >/dev/null; \
	grep -Fxq 'payload-two' "$$destination/rootfs/base.ext4"; \
	grep -Fxq 'manifest-two' "$$destination/OrlixOSManifest.plist"; \
	[ -f "$$destination/preserved" ]; \
	[ ! -e "$$destination.tmp.$$$$" ]; \
	echo "pass: payload sync"
