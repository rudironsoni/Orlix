# SPDX-License-Identifier: GPL-2.0

# Recipe-local removal policy for generated trees. Callers install the
# function in their shell with $(orlix_disposable_tree_removal), then pass the
# exact path and its diagnostic name to orlix_remove_disposable_tree.
define orlix_disposable_tree_removal
orlix_remove_disposable_tree() { \
	path="$$1"; \
	diagnostic_name="$$2"; \
	if [ -L "$$path" ]; then \
		echo "refusing symlinked $$diagnostic_name: $$path" >&2; \
		return 1; \
	fi; \
	if [ -d "$$path" ]; then \
		find "$$path" -type d -exec chmod u+w {} +; \
	fi; \
	for attempt in 1 2 3; do \
		rm -rf "$$path" && break; \
		sleep 1; \
	done; \
	[ ! -e "$$path" ] || { \
		echo "failed to remove $$diagnostic_name: $$path" >&2; \
		return 1; \
	}; \
	};
endef

.PHONY: __disposable-tree-tests
__disposable-tree-tests:
	@set -euo pipefail; \
	$(orlix_disposable_tree_removal) \
	tmp="$$(mktemp -d "$${TMPDIR:-/tmp}/orlix-disposable-tree.XXXXXX")"; \
	trap 'chmod -R u+w "$$tmp" 2>/dev/null || true; rm -rf "$$tmp"' EXIT; \
	fail() { printf 'disposable tree test failed: %s\n' "$$*" >&2; exit 1; }; \
	readonly_tree="$$tmp/read-only-tree"; \
	mkdir -p "$$readonly_tree/parent/child"; \
	printf 'generated\n' > "$$readonly_tree/parent/child/artifact"; \
	chmod a-w "$$readonly_tree/parent" "$$readonly_tree/parent/child"; \
	orlix_remove_disposable_tree "$$readonly_tree" "test generated tree"; \
	[ ! -e "$$readonly_tree" ] || fail "read-only generated tree remains"; \
	target="$$tmp/symlink-target"; \
	link="$$tmp/symlink-tree"; \
	mkdir -p "$$target"; \
	printf 'preserve\n' > "$$target/sentinel"; \
	ln -s "$$target" "$$link"; \
	if orlix_remove_disposable_tree "$$link" "test generated tree" 2>/dev/null; then \
		fail "top-level symlink was accepted"; \
	fi; \
	[ -L "$$link" ] || fail "refused symlink was modified"; \
	grep -Fxq preserve "$$target/sentinel" || fail "symlink target was modified"; \
	orlix_remove_disposable_tree "$$tmp/missing-tree" "test missing tree"; \
	printf 'disposable tree removal test: passed\n'
