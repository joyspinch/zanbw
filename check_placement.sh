#!/usr/bin/env bash
# The differential test for the placement layer.
#
# The C++ side drives the real openbw find_unit_placement; the Zan side
# drives the port. Both dump the same lines, so the traces must match
# byte for byte.
#
# Usage: check_placement.sh [variants]

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
ZAN_STDLIB_ARG=""
if [ -n "$ZAN_STDLIB" ]; then ZAN_STDLIB_ARG="--stdlib-path $ZAN_STDLIB"; fi
CXX="${CXX:-g++}"

cd "$ROOT"

. ./zan_sources.sh

VARIANTS="${1:-0 1 2 3 4 5 6}"

ZAN_SRC=(
    src/beds/placement_main.zan \
    "${ZAN_SOURCES_BASE[@]}" \
    "${ZAN_SOURCES_SCENARIOS[@]}" \
    "${ZAN_SOURCES_CORE[@]}" \
    "${ZAN_SOURCES_REGIONS[@]}" \
    "${ZAN_SOURCES_PATH[@]}" \
    "${ZAN_SOURCES_COLLISION[@]}"
)

echo "== building the Zan side =="
"$ZANC" "${ZAN_SRC[@]}" --auto-stdlib $ZAN_STDLIB_ARG -o build/placement_zan.exe || exit 1

echo "== building the C++ side =="
"$CXX" -std=c++17 -I../openbw -O0 -g tools/cppbed/placement_bed.cpp -o ./tools/cppbed/placement_bed.exe || exit 1

mkdir -p build
FAIL=0
for v in $VARIANTS; do
    ./build/placement_zan.exe "$v" > build/placement_zan.txt 2>&1
    ./tools/cppbed/placement_bed.exe "$v" > build/placement_cpp.txt 2>&1
    if ! diff -u build/placement_cpp.txt build/placement_zan.txt > build/placement_diff.txt; then
        echo "FAIL variant $v"
        head -30 build/placement_diff.txt
        FAIL=1
    fi
    echo "variant $v done"
done

if [ "$FAIL" != 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "all placements match bit for bit"
