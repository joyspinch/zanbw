#!/usr/bin/env bash
# Bit-exact differential test for the long-path (region level) A* search.
#
#   ./check_astar.sh              -- all terrain variants
#   ./check_astar.sh 3            -- one variant

set -uo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
CXX="${CXX:-g++}"

cd "$ROOT"

. ./zan_sources.sh

if [ "$#" -ge 1 ]; then
    VARIANTS="$1"
else
    VARIANTS="0 1 2 3 4 5 6"
fi

echo "== building C++ A* bed =="
(cd tools/cppbed && "$CXX" -std=c++17 -I../../../openbw -O0 -g astar_bed.cpp -o astar_bed.exe) || exit 1

echo "== building Zan A* port =="
"$ZANC" src/beds/astar_main.zan \
        "${ZAN_SOURCES_BASE[@]}" \
        "${ZAN_SOURCES_REGIONS[@]}" \
        "${ZAN_SOURCES_CORE[@]}" \
        "${ZAN_SOURCES_ASTAR[@]}" \
        --auto-stdlib -o build/astar_zan.exe || exit 1

FAIL=0
for v in $VARIANTS; do
    ./tools/cppbed/astar_bed.exe "$v" > "build/astar_cpp_$v.txt" 2>&1
    ./build/astar_zan.exe "$v" > "build/astar_zan_$v.txt" 2>&1
    if diff -u "build/astar_cpp_$v.txt" "build/astar_zan_$v.txt" > "build/astar_diff_$v.txt"; then
        n=$(grep -c "^Q " "build/astar_cpp_$v.txt")
        echo "  variant $v: OK ($n queries)"
    else
        echo "  variant $v: MISMATCH -> build/astar_diff_$v.txt"
        head -20 "build/astar_diff_$v.txt"
        FAIL=1
    fi
done

if [ "$FAIL" -ne 0 ]; then
    echo "== FAILED =="
    exit 1
fi
echo "== LONG PATH A* BIT-IDENTICAL =="
