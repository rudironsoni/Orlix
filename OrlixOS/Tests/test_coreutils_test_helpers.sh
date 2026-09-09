#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

here=$(dirname "$0")
valgrind="$here/coreutils-test-valgrind.sh"
cc="$here/coreutils-test-cc.sh"
if [ -n "${ORLIXOS_HELPER_TEST_SCRATCH:-}" ]; then
	scratch=$ORLIXOS_HELPER_TEST_SCRATCH
else
	scratch=$(mktemp -d "${TMPDIR:-/tmp}/orlix-coreutils-helpers.XXXXXX")
	trap 'rm -rf "$scratch"' EXIT INT TERM
fi

chmod +x "$valgrind" "$cc"

"$valgrind" --error-exitcode=1 true
"$valgrind" --quiet --error-exitcode=3 sh -c 'exit 0'
if "$valgrind" >/dev/null 2>"$scratch/valgrind-missing.err"; then
	echo "valgrind helper accepted a missing program" >&2
	exit 1
fi
grep -F -q 'missing program' "$scratch/valgrind-missing.err"

mkdir -p "$scratch/bin"
printf '#!/bin/sh\nprintf "tcc %%s\\n" "$*"\n' >"$scratch/bin/tcc"
chmod +x "$scratch/bin/tcc"
ORLIXOS_TEST_TCC="$scratch/bin/tcc" "$cc" -Wall -shared --std=gnu99 -fPIC -O2 -xc -o "$scratch/d.so" - \
	>"$scratch/cc.out"
grep -F -q -- '-std=gnu99' "$scratch/cc.out"
grep -F -q -- '-shared' "$scratch/cc.out"
if ORLIXOS_TEST_TCC="$scratch/missing-tcc" "$cc" -shared >/dev/null 2>"$scratch/cc-missing.err"; then
	echo "cc helper accepted a missing tcc" >&2
	exit 1
fi
grep -F -q 'missing tcc' "$scratch/cc-missing.err"

echo "pass: coreutils test helpers"
