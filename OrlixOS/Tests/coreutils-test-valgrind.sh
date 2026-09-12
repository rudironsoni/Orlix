#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Test-fixture valgrind. The guest runs under OrlixTCTI, so a real valgrind
# cannot ptrace that CPU. GNU require_valgrind_ only checks
# `valgrind --error-exitcode=1 true`. This helper strips valgrind options and
# execs the program so those tests run the same cp/sort/shuf commands.
set -eu

while [ "$#" -gt 0 ]; do
	case "$1" in
	--)
		shift
		break
		;;
	--error-exitcode)
		shift
		if [ "$#" -gt 0 ]; then
			shift
		fi
		;;
	--tool)
		shift
		if [ "$#" -gt 0 ]; then
			shift
		fi
		;;
	--error-exitcode=*|--tool=*|--quiet|--suppressions=*|-q)
		shift
		;;
	-*)
		shift
		;;
	*)
		break
		;;
	esac
done

if [ "$#" -eq 0 ]; then
	echo "valgrind: missing program" >&2
	exit 1
fi

exec "$@"
