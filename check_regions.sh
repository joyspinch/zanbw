#!/usr/bin/env bash
# Bit-exact differential test for the regions layer.
#
#   ./check_regions.sh              -- all terrain variants
#   ./check_regions.sh 3            -- one variant
#
# The C++ bed drives the real regions_create() on a synthetic terrain; the Zan
# port reimplements it. The two traces must be byte identical.

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

echo "== building C++ regions bed =="
(cd tools/cppbed && "$CXX" -std=c++17 -I../../../openbw -O0 -g regions_bed.cpp -o regions_bed.exe) || exit 1

echo "== building Zan regions port =="
"$ZANC" src/regions_main.zan src/Regions.zan src/RegionsCreate.zan src/RegionsRefresh.zan \
        src/RegionsMain.zan src/RegionsContours.zan src/RegionsDump.zan src/Terrain.zan \
        src/Fp.zan src/Dir.zan src/Enums.zan \
        --auto-stdlib -o build/regions_zan.exe || exit 1

FAIL=0
for v in $VARIANTS; do
    ./tools/cppbed/regions_bed.exe "$v" > "build/regions_cpp_$v.txt" 2>&1
    ./build/regions_zan.exe "$v" > "build/regions_zan_$v.txt" 2>&1
    if diff -u "build/regions_cpp_$v.txt" "build/regions_zan_$v.txt" > "build/regions_diff_$v.txt"; then
        n=$(grep -c "^R " "build/regions_cpp_$v.txt")
        c=$(grep "^C 0 count" "build/regions_cpp_$v.txt")
        echo "  variant $v: OK ($n regions, $c)"
    else
        echo "  variant $v: MISMATCH -> build/regions_diff_$v.txt"
        head -30 "build/regions_diff_$v.txt"
        FAIL=1
    fi
done

if [ "$FAIL" -ne 0 ]; then
    echo "== FAILED =="
    exit 1
fi
echo "== REGIONS LAYER BIT-IDENTICAL =="
