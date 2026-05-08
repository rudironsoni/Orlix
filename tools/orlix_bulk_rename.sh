#!/bin/zsh
set -euo pipefail

if [[ "$#" -lt 1 ]]; then
  echo "usage: $0 <repo> [extra_path ...]" >&2
  exit 1
fi

repo="$1"
shift

cd "$repo"

tmp_files="$(mktemp)"
tmp_paths="$(mktemp)"
trap '/bin/rm -f "$tmp_files" "$tmp_paths"' EXIT

git ls-files -z \
  ':(exclude)third_party/linux/**' \
  ':(exclude)third_party/linux-*/**' > "$tmp_files"

for extra in "$@"; do
  if [[ -e "$extra" ]]; then
    /usr/bin/find "$extra" \
      \( -path '*/.git' -o -path '*/build' -o -path '*/.build' -o -path '*/DerivedData*' -o -path '*/third_party/linux' -o -path '*/third_party/linux-*' -o -path '*/.worktrees' -o -path '*/.artifacts' -o -path '*/.cache' -o -path '*/.clang-tidy-build' -o -path '*/.clangd-cache' \) -prune -o \
      -type f -print0 >> "$tmp_files"
  fi
done

/usr/bin/xargs -0 perl -0pi -e '
  s{Library/Application Support/IXLand}{Library/Application Support/Orlix}g;
  s{Library/Caches/IXLand}{Library/Caches/Orlix}g;
  s{Application Support/IXLand}{Application Support/Orlix}g;
  s{Caches/IXLand}{Caches/Orlix}g;
  s{com\.ixland}{com.orlix}g;
  s{app\.ixland}{app.orlix}g;
  s{org\.ixland}{org.orlix}g;
  s{IX Land}{Orlix}g;
  s{ix_land}{orlix}g;
  s{IXLAND}{ORLIX}g;
  s{IXLand}{Orlix}g;
  s{ixland}{orlix}g;
' < "$tmp_files"

{
  git ls-files
  for extra in "$@"; do
    if [[ -e "$extra" ]]; then
      /usr/bin/find "$extra" \
        \( -path '*/.git' -o -path '*/build' -o -path '*/.build' -o -path '*/DerivedData*' -o -path '*/third_party/linux' -o -path '*/third_party/linux-*' -o -path '*/.worktrees' -o -path '*/.artifacts' -o -path '*/.cache' -o -path '*/.clang-tidy-build' -o -path '*/.clangd-cache' \) -prune -o \
        -print
    fi
  done
} | awk 'NF' | sort -u > "$tmp_paths"

while IFS= read -r path; do
  [[ -e "$path" ]] || continue
  target="$path"
  target="${target//IXLand/Orlix}"
  target="${target//ixland/orlix}"
  target="${target//IXLAND/ORLIX}"
  if [[ "$target" == "$path" ]]; then
    continue
  fi
  if [[ -e "$target" ]]; then
    continue
  fi
  /bin/mkdir -p "$(/usr/bin/dirname "$target")"
  /bin/mv "$path" "$target"
done < <(sort -r "$tmp_paths")
