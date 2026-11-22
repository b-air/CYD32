#!/usr/bin/env bash
# Host tests for the shared pure logic (not run by CI). Needs a C++17 compiler
# (CXX, default c++; "python -m ziglang c++" works too).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$here/../.."
out="${TMPDIR:-/tmp}/cyd32-hosttest"
${CXX:-c++} -std=c++17 -O1 -Wall -Wextra -I"$root/include" \
  "$here/test_core.cpp" "$root/src/bearing.cpp" "$root/src/signatures.cpp" -o "$out"
"$out"
"$here/guard_selftest.sh"
