#!/usr/bin/env bash
# Bit-exact differential test: the Zan movement state machine vs the C++
# reference bed.
#
#   ./check_state.sh              -- all 7 terrain variants x 16 scenarios
#   ./check_state.sh 3            -- one variant, all scenarios
#   ./check_state.sh 3 5          -- one variant, one scenario
#
# The state machine is not frame based in the same way as the kinematics
# layer: the unit is driven by execute_movement(), and the trace is a set
# of snapshots (every 4th frame plus the final one).
#
# The unit-to-unit collision checks are not part of this layer: they need
# the unit finder, which the bed does not have. In a single unit bed they
# always report "no collision".

set -uo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
CXX="${CXX:-g++}"

cd "$ROOT"

VARIANTS="${1:-0 1 2 3 4 5 6}"
SCENARIOS="${2:-0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15}"

ZAN_SRC=(
    src/state_main.zan
    src/StateMachine.zan
    src/MovementStates.zan
    src/Path.zan
    src/PathProgress.zan
    src/Pathfinder.zan
    src/AStar.zan
    src/ShortPath.zan
    src/ShortPathNeighbors.zan
    src/ShortPathAddNeighbors.zan
    src/ShortPathSearch.zan
    src/ShortPathRun.zan
    src/ShortPathNext.zan
    src/Regions.zan
    src/RegionsCreate.zan
    src/RegionsRefresh.zan
    src/RegionsMain.zan
    src/RegionsContours.zan
    src/RegionsDump.zan
    src/Terrain.zan
    src/Unit.zan
    src/Sim.zan
    src/Kinematics.zan
    src/Fp.zan
    src/Dir.zan
    src/Enums.zan
    src/Rng.zan
    src/Tables.zan
)

echo "== building the Zan side =="
"$ZANC" "${ZAN_SRC[@]}" --auto-stdlib -o build/state_zan.exe || exit 1

echo "== building the C++ side =="
"$CXX" -std=c++17 -I../openbw -O0 -g tools/cppbed/state_bed.cpp -o tools/cppbed/state_bed.exe || exit 1

mkdir -p build
FAIL=0
for v in $VARIANTS; do
    for s in $SCENARIOS; do
        ./build/state_zan.exe "$v" "$s" > build/state_zan.txt 2>&1
        ./tools/cppbed/state_bed.exe "$v" "$s" > build/state_cpp.txt 2>&1
        if ! diff -u build/state_cpp.txt build/state_zan.txt > build/state_diff.txt; then
            echo "FAIL variant $v scenario $s"
            head -20 build/state_diff.txt
            FAIL=1
        fi
    done
    echo "variant $v done"
done

if [ "$FAIL" -ne 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "all state machine scenarios match bit for bit"
