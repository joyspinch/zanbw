#!/usr/bin/env bash
# Bit-exact differential test for the path execution layer:
# path_t lifecycle, path_progress(), unit_path_to() and create_single_step_path().
#
#   ./check_path.sh              -- all terrain variants
#   ./check_path.sh 3            -- one variant

set -uo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
CXX="${CXX:-g++}"

cd "$ROOT"

if [ "$#" -ge 1 ]; then
    VARIANTS="$1"
else
    VARIANTS="0 1 2 3 4 5 6"
fi

echo "== building C++ path bed =="
(cd tools/cppbed && "$CXX" -std=c++17 -I../../../openbw -O0 -g path_bed.cpp -o path_bed.exe) || exit 1

echo "== building Zan path port =="
"$ZANC" src/path_main.zan src/Regions.zan src/RegionsCreate.zan src/RegionsRefresh.zan \
        src/RegionsMain.zan src/RegionsContours.zan src/RegionsDump.zan src/Terrain.zan \
        src/Pathfinder.zan src/AStar.zan src/ShortPath.zan src/ShortPathNeighbors.zan \
        src/ShortPathAddNeighbors.zan src/ShortPathSearch.zan src/ShortPathRun.zan \
        src/ShortPathNext.zan src/Path.zan src/PathProgress.zan \
        src/Fp.zan src/Dir.zan src/Enums.zan src/Unit.zan src/Sim.zan src/Kinematics.zan \
        src/Rng.zan src/Tables.zan \
        --auto-stdlib -o build/path_zan.exe || exit 1

FAIL=0
for v in $VARIANTS; do
    for s in 0 1 2 3 4 5 6 7 8 9; do
        ./tools/cppbed/path_bed.exe "$v" "$s" > "build/pp_cpp_${v}_$s.txt" 2>&1
        ./build/path_zan.exe "$v" "$s" > "build/pp_zan_${v}_$s.txt" 2>&1
        if ! diff -u "build/pp_cpp_${v}_$s.txt" "build/pp_zan_${v}_$s.txt" > "build/pp_diff_${v}_$s.txt"; then
            echo "  variant $v scenario $s: MISMATCH -> build/pp_diff_${v}_$s.txt"
            head -20 "build/pp_diff_${v}_$s.txt"
            FAIL=1
        fi
    done
    if [ "$FAIL" -eq 0 ]; then
        echo "  variant $v: OK (10 scenarios)"
    fi
done

if [ "$FAIL" -ne 0 ]; then
    echo "== FAILED =="
    exit 1
fi
echo "== PATH EXECUTION BIT-IDENTICAL =="
