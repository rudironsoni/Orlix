#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Test-fixture CC. GNU require_gcc_shared_ runs
# `$CC -Wall -shared --std=gnu99 -fPIC -O2 -xc`. Map GNU cc flags onto tcc.
set -eu

tcc=${ORLIXOS_TEST_TCC:-/bin/tcc}
if [ ! -x "$tcc" ]; then
	echo "cc: missing tcc at $tcc" >&2
	exit 1
fi

mapped=
for arg in "$@"; do
	case "$arg" in
	--std=*)
		mapped="$mapped -std=${arg#--std=}"
		;;
	-std=*|-Wall|-shared|-fPIC|-O2|-O0|-g|-xc|-c|-o|-ldl|-I*|-D*|-Werror|-Wno-*|-fno-*|-pthread)
		mapped="$mapped $arg"
		;;
	-*)
		mapped="$mapped $arg"
		;;
	*)
		mapped="$mapped $arg"
		;;
	esac
done

# shellcheck disable=SC2086
exec "$tcc" $mapped
