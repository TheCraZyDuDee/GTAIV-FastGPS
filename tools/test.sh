#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
temp="$(mktemp -d)"
trap 'rm -rf "$temp"' EXIT
for source in tests/test_*.c; do
    echo "== $source =="
    clang -O1 -g -Isrc -fsanitize=address,undefined "$source" -o "$temp/fastgps-test"
    "$temp/fastgps-test"
done
