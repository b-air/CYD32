#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 CYD32 contributors
# Checks files against a PRIVATE deny-list (one fixed string per line, case-insensitive).
#   denylist-check.sh <denylist-file> [files...]   (default: all tracked files)
# Prints only the names of offending files and a count, never the matched words.
set -uo pipefail
list="${1:?denylist file}"; shift
[[ -s "$list" ]] || { echo "denylist: empty or missing list"; exit 2; }
if [[ $# -eq 0 ]]; then mapfile -t files < <(git ls-files); else files=("$@"); fi
bad=()
for f in "${files[@]}"; do
  [[ -f "$f" ]] || continue
  if grep -qiFI -f <(grep -v '^\s*$' "$list") -- "$f" 2>/dev/null; then bad+=("$f"); fi
  # PNG metadata (tEXt/zTXt/iTXt/eXIf), decompressed
  case "$f" in *.png|*.PNG)
    if python3 "$(dirname "$0")/png_text.py" "$f" | grep -qiF -f <(grep -v '^\s*$' "$list"); then bad+=("(png metadata) $f"); fi;;
  esac
  # also check the path itself
  if printf '%s\n' "$f" | grep -qiF -f <(grep -v '^\s*$' "$list"); then bad+=("(path) $f"); fi
done
# commit metadata of the checked range (author/committer/message), if requested
if [[ -n "${DENYLIST_GIT_RANGE:-}" ]]; then
  if git log --format='%an%n%ae%n%cn%n%ce%n%B' "$DENYLIST_GIT_RANGE" | grep -qiF -f <(grep -v '^\s*$' "$list"); then bad+=("(commit metadata in $DENYLIST_GIT_RANGE)"); fi
fi
if (( ${#bad[@]} )); then
  echo "denylist: ${#bad[@]} hit(s) in:"; printf '  %s\n' "${bad[@]}"; exit 1
fi
echo "denylist: OK (${#files[@]} files)"
