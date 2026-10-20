#!/usr/bin/env bash
# Bit-exact differential test: the Zan movement state machine vs the C++
# reference bed, for the states that only unusual unit types reach.
#
#   ./check_state2.sh              -- all 7 terrain variants x 12 scenarios
#   ./check_state2.sh 3            -- one variant, all scenarios
#   ./check_state2.sh 3 5          -- one variant, one scenario
#
# The plain state bed drives one ground unit that can move and turn, so it
# never touches UM_Turret, UM_BldgTurret, UM_Flyer, UM_Bunker, UM_Hidden,
# UM_Lump, UM_LumpWannabe or UM_InitSeq. This bed creates one unit per
# scenario with the type flags and status flags that scenario needs.

set -uo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
CXX="${CXX:-g++}"

cd "$ROOT"

. ./zan_sources.sh

VARIANTS="${1:-0 1 2 3 4 5 6}"
SCENARIOS="${2:-0 1 2 3 4 5 6 7 8 9 10 11}"

ZAN_SRC=(
    src/beds/state2_main.zan \
    "${ZAN_SOURCES_BASE[@]}" \
    "${ZAN_SOURCES_SCENARIOS[@]}" \
    "${ZAN_SOURCES_CORE[@]}" \
    "${ZAN_SOURCES_REGIONS[@]}" \
    "${ZAN_SOURCES_PATH[@]}" \
    "${ZAN_SOURCES_COLLISION[@]}"
)

echo "== building the Zan side =="
"$ZANC" "${ZAN_SRC[@]}" --auto-stdlib -o build/state2_zan.exe || exit 1

echo "== building the C++ side =="
"$CXX" -std=c++17 -I../openbw -O0 -g tools/cppbed/state2_bed.cpp -o ./tools/cppbed/state2_bed.exe || exit 1

mkdir -p build
FAIL=0
for v in $VARIANTS; do
    for s in $SCENARIOS; do
        ./build/state2_zan.exe "$v" "$s" > build/state2_zan.txt 2>&1
        ./tools/cppbed/state2_bed.exe "$v" "$s" > build/state2_cpp.txt 2>&1
        if ! diff -u build/state2_cpp.txt build/state2_zan.txt > build/state2_diff.txt; then
            echo "FAIL variant $v scenario $s"
            head -20 build/state2_diff.txt
            FAIL=1
        fi
    done
    echo "variant $v done"
done

if [ "$FAIL" -ne 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "all movement state scenarios match bit for bit"
