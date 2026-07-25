#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Verify that a reusable archive is newer than every direct and discovered input.
set -euo pipefail

archive=""
symbols=""
declare -a required_symbols=() cache_deps=() sources=() objects=() depfiles=()

usage() {
	printf '%s\n' "usage: $0 --archive PATH --symbols PATH [--symbol NAME] [--cache-dep PATH] [--source PATH --object PATH --depfile PATH]" >&2
	exit 2
}

while [ "$#" -gt 0 ]; do
	case "$1" in
		--archive) [ "$#" -ge 2 ] || usage; archive="$2"; shift 2 ;;
		--symbols) [ "$#" -ge 2 ] || usage; symbols="$2"; shift 2 ;;
		--symbol) [ "$#" -ge 2 ] || usage; required_symbols+=("$2"); shift 2 ;;
		--cache-dep) [ "$#" -ge 2 ] || usage; cache_deps+=("$2"); shift 2 ;;
		--source) [ "$#" -ge 2 ] || usage; sources+=("$2"); shift 2 ;;
		--object) [ "$#" -ge 2 ] || usage; objects+=("$2"); shift 2 ;;
		--depfile) [ "$#" -ge 2 ] || usage; depfiles+=("$2"); shift 2 ;;
		*) usage ;;
	esac
done

[ -n "$archive" ] && [ -n "$symbols" ] || usage
[ "${#sources[@]}" -eq "${#objects[@]}" ] || usage
[ "${#sources[@]}" -eq "${#depfiles[@]}" ] || usage
[ -s "$archive" ] && [ -s "$symbols" ] || exit 1

for symbol in "${required_symbols[@]}"; do grep -q "$symbol" "$symbols" || exit 1; done
for dependency in "${cache_deps[@]}"; do [ -e "$dependency" ] && [ "$archive" -nt "$dependency" ] || exit 1; done

for index in "${!sources[@]}"; do
	source="${sources[$index]}"; object="${objects[$index]}"; depfile="${depfiles[$index]}"
	[ -s "$source" ] && [ -s "$object" ] && [ -s "$depfile" ] || exit 1
	[ "$archive" -nt "$source" ] && [ "$archive" -nt "$object" ] || exit 1
	while IFS= read -r dependency; do
		[ -n "$dependency" ] || continue
		[ -e "$dependency" ] && [ "$archive" -nt "$dependency" ] || exit 1
	done < <(perl -0pe 's/\\\n/ /g; s/^[^:]*:\s*//' "$depfile" | tr ' ' '\n')
done
