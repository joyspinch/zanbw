#!/usr/bin/env bash
# Bit-exact differential test: the Zan creep layer vs the C++ reference bed.
#
#   ./check_creep.sh              -- all 7 terrain variants x 12 scenarios
#   ./check_creep.sh 3            -- one variant, all scenarios
#   ./check_creep.sh 3 5          -- one variant, one scenario
#
# The bed checks the creep flags on every tile, the creep life
# bookkeeping (the nine neighbour-count lists, their order, the free
# list) and the random number state -- recede_creep() picks a random
# entry out of one of those lists, so both the list order and the RNG
# state have to match.

set -uo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
CXX="${CXX:-g++}"

cd "$ROOT"

. ./zan_sources.sh

VARIANTS="${1:-0 1 2 3 4 5 6}"
SCENARIOS="${2:-0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17}"

ZAN_SRC=(
    src/beds/creep_main.zan \
    "${ZAN_SOURCES_BASE[@]}" \
    "${ZAN_SOURCES_CORE[@]}" \
    "${ZAN_SOURCES_REGIONS[@]}" \
    "${ZAN_SOURCES_VISION[@]}" \
    "${ZAN_SOURCES_CREEP[@]}"
)

echo "== building the Zan side =="
"$ZANC" "${ZAN_SRC[@]}" --auto-stdlib -o build/creep_zan.exe || exit 1

echo "== building the C++ side =="
"$CXX" -std=c++17 -I../openbw -O0 -g tools/cppbed/creep_bed.cpp -o ./tools/cppbed/creep_bed.exe || exit 1

mkdir -p build
FAIL=0
for v in $VARIANTS; do
    for s in $SCENARIOS; do
        ./build/creep_zan.exe "$v" "$s" > build/creep_zan.txt 2>&1
        ./tools/cppbed/creep_bed.exe "$v" "$s" > build/creep_cpp.txt 2>&1
        if ! diff -u build/creep_cpp.txt build/creep_zan.txt > build/creep_diff.txt; then
            echo "FAIL variant $v scenario $s"
            head -30 build/creep_diff.txt
            FAIL=1
        fi
    done
    echo "variant $v done"
done

if [ "$FAIL" -ne 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "all creep scenarios match bit for bit"
