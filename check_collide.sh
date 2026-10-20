#!/usr/bin/env bash
# Bit-exact differential test: the Zan unit-to-unit collision layer vs the
# C++ reference bed.
#
#   ./check_collide.sh              -- all 7 terrain variants x 17 scenarios
#   ./check_collide.sh 3            -- one variant, all scenarios
#   ./check_collide.sh 3 5          -- one variant, one scenario
#
# The bed creates two ground units and drives them at each other, so the
# unit finder has something to find and the collision states are reached.

set -uo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
CXX="${CXX:-g++}"

cd "$ROOT"

. ./zan_sources.sh

VARIANTS="${1:-0 1 2 3 4 5 6}"
SCENARIOS="${2:-0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16}"

ZAN_SRC=(
    src/beds/collide_main.zan \
    "${ZAN_SOURCES_BASE[@]}" \
    "${ZAN_SOURCES_SCENARIOS[@]}" \
    "${ZAN_SOURCES_CORE[@]}" \
    "${ZAN_SOURCES_REGIONS[@]}" \
    "${ZAN_SOURCES_PATH[@]}" \
    "${ZAN_SOURCES_COLLISION[@]}"
)

echo "== building the Zan side =="
"$ZANC" "${ZAN_SRC[@]}" --auto-stdlib -o build/collide_zan.exe || exit 1

echo "== building the C++ side =="
"$CXX" -std=c++17 -I../openbw -O0 -g tools/cppbed/collide_bed.cpp -o ./tools/cppbed/collide_bed.exe || exit 1

mkdir -p build
FAIL=0
for v in $VARIANTS; do
    for s in $SCENARIOS; do
        ./build/collide_zan.exe "$v" "$s" > build/collide_zan.txt 2>&1
        ./tools/cppbed/collide_bed.exe "$v" "$s" > build/collide_cpp.txt 2>&1
        if diff -q build/collide_zan.txt build/collide_cpp.txt > /dev/null; then
            echo "  ok   variant=$v scenario=$s"
        else
            echo "  FAIL variant=$v scenario=$s"
            diff build/collide_cpp.txt build/collide_zan.txt | head -12
            FAIL=1
        fi
    done
done

if [ "$FAIL" = "0" ]; then
    echo "all scenarios match"
else
    echo "mismatches found"
fi
exit $FAIL
