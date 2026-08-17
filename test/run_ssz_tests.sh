#!/usr/bin/env bash
# ============================================================================
# test/run_ssz_tests.sh — run the SSZ regression suite.
#
# Each test/ssz/<name>.ssz exercises one native (C++) SSZ library against
# reference values and writes test/work/<name>_out.txt; the runner diffs it
# against test/ssz/<name>.expected.
#
# Usage:
#   bash test/run_ssz_tests.sh [path-to-ikemen-debug.exe]
#
# Defaults to install/ikemen-debug.exe, then build/Debug/ikemen-debug.exe.
# Requires the ssz_script/lib/*.{ssz,cpp} tree to be installed next to the
# exe (make CONFIG=Debug install) so `<file.ssz>`-style imports resolve.
# ============================================================================

set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

EXE="${1:-}"
if [ -z "$EXE" ]; then
  for candidate in "$ROOT/install/ikemen-debug.exe" "$ROOT/build/Debug/ikemen-debug.exe"; do
    if [ -f "$candidate" ]; then EXE="$candidate"; break; fi
  done
fi
if [ -z "$EXE" ] || [ ! -f "$EXE" ]; then
  echo "error: ikemen-debug.exe not found (build with: make CONFIG=Debug install)" >&2
  exit 1
fi
EXE="$(cd "$(dirname "$EXE")" && pwd)/$(basename "$EXE")"

SSZ_DIR="$ROOT/test/ssz"
WORK="$ROOT/test/work"
mkdir -p "$WORK"

# Convert a path to Windows form if cygpath is available (Git Bash on Windows).
to_win() {
  if command -v cygpath >/dev/null 2>&1; then cygpath -w "$1"; else echo "$1"; fi
}

pass=0
fail=0
for script in "$SSZ_DIR"/*.ssz; do
  name="$(basename "$script" .ssz)"
  expected="$SSZ_DIR/$name.expected"
  if [ ! -f "$expected" ]; then
    echo "SKIP: $name (no $name.expected)"
    continue
  fi
  # Clean per-test artifacts (output + any side files the test writes).
  rm -f "$WORK/${name}_out.txt" "$WORK/${name}_sub_out.txt" \
        "$WORK/${name}_code.txt" "$WORK/${name}_err.txt"
  (cd "$WORK" && "$EXE" "$(to_win "$script")" > "$WORK/$name.log" 2>&1)
  if [ -f "$WORK/${name}_out.txt" ] && \
     diff -q "$WORK/${name}_out.txt" "$expected" >/dev/null 2>&1; then
    echo "PASS: $name"
    pass=$((pass + 1))
  else
    echo "FAIL: $name (expected: $(tr -d '\r\n' < "$expected" 2>/dev/null) | got: $(tr -d '\r\n' < "$WORK/${name}_out.txt" 2>/dev/null))"
    grep -A3 "Error Message" "$WORK/$name.log" | head -6
    fail=$((fail + 1))
  fi
done

echo "=== $pass passed, $fail failed ==="
exit $((fail > 0))
