#!/usr/bin/env bash
# Bit-exact differential test: the Zan visibility layer vs the C++
# reference bed.
#
#   ./check_vision.sh              -- all 7 terrain variants x 12 scenarios
#   ./check_vision.sh 3            -- one variant, all scenarios
#   ./check_vision.sh 3 5          -- one variant, one scenario
#
# The bed checks two things: the generated sight range table (the table
# that says which tiles a unit of a given sight range sees, and in which
# order the visibility propagates) and the per-tile visible / explored
# masks after one reveal operation.

set -uo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
CXX="${CXX:-g++}"

cd "$ROOT"

. ./zan_sources.sh

VARIANTS="${1:-0 1 2 3 4 5 6}"
SCENARIOS="${2:-0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15}"

ZAN_SRC=(
    src/beds/vision_main.zan \
    "${ZAN_SOURCES_BASE[@]}" \
    "${ZAN_SOURCES_CORE[@]}" \
    "${ZAN_SOURCES_REGIONS[@]}" \
    "${ZAN_SOURCES_VISION[@]}"
)

echo "== building the Zan side =="
"$ZANC" "${ZAN_SRC[@]}" --auto-stdlib -o build/vision_zan.exe || exit 1

echo "== building the C++ side =="
"$CXX" -std=c++17 -I../openbw -O0 -g tools/cppbed/vision_bed.cpp -o ./tools/cppbed/vision_bed.exe || exit 1

mkdir -p build
FAIL=0
for v in $VARIANTS; do
    for s in $SCENARIOS; do
        ./build/vision_zan.exe "$v" "$s" > build/vision_zan.txt 2>&1
        ./tools/cppbed/vision_bed.exe "$v" "$s" > build/vision_cpp.txt 2>&1
        if ! diff -u build/vision_cpp.txt build/vision_zan.txt > build/vision_diff.txt; then
            echo "FAIL variant $v scenario $s"
            head -30 build/vision_diff.txt
            FAIL=1
        fi
    done
    echo "variant $v done"
done

if [ "$FAIL" -ne 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "all visibility scenarios match bit for bit"
