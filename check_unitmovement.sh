#!/usr/bin/env bash
# The differential test for the movement modification layer.
#
# The C++ side drives the real openbw get_modified_unit_speed,
# get_modified_unit_acceleration and get_modified_unit_turn_rate; the
# Zan side drives the port. Both dump the same lines, so the traces
# must match byte for byte.
#
# Usage: check_unitmovement.sh [variants]

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
CXX="${CXX:-g++}"

cd "$ROOT"

. ./zan_sources.sh

VARIANTS="${1:-0 1 2 3 4 5 6}"

ZAN_SRC=(
    src/beds/unitmovement_main.zan \
    "${ZAN_SOURCES_BASE[@]}" \
    "${ZAN_SOURCES_CORE[@]}"
)

echo "== building the Zan side =="
"$ZANC" "${ZAN_SRC[@]}" --auto-stdlib -o build/unitmovement_zan.exe || exit 1

echo "== building the C++ side =="
"$CXX" -std=c++17 -I../openbw -O0 -g tools/cppbed/unitmovement_bed.cpp -o ./tools/cppbed/unitmovement_bed.exe || exit 1

mkdir -p build
FAIL=0
for v in $VARIANTS; do
    ./build/unitmovement_zan.exe "$v" > build/unitmovement_zan.txt 2>&1
    ./tools/cppbed/unitmovement_bed.exe "$v" > build/unitmovement_cpp.txt 2>&1
    if ! diff -u build/unitmovement_cpp.txt build/unitmovement_zan.txt > build/unitmovement_diff.txt; then
        echo "FAIL variant $v"
        head -30 build/unitmovement_diff.txt
        FAIL=1
    fi
    echo "variant $v done"
done

if [ "$FAIL" != 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "all movement modifications match bit for bit"
