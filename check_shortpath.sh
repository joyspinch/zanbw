#!/usr/bin/env bash
# Bit-exact differential test for the short-path (contour) search.
#
#   ./check_shortpath.sh              -- all terrain variants
#   ./check_shortpath.sh 3            -- one variant

set -uo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
ZAN_STDLIB_ARG=""
if [ -n "$ZAN_STDLIB" ]; then ZAN_STDLIB_ARG="--stdlib-path $ZAN_STDLIB"; fi
CXX="${CXX:-g++}"

cd "$ROOT"

. ./zan_sources.sh

if [ "$#" -ge 1 ]; then
    VARIANTS="$1"
else
    VARIANTS="0 1 2 3 4 5 6"
fi

echo "== building C++ short path bed =="
(cd tools/cppbed && "$CXX" -std=c++17 -I../../../openbw -O0 -g shortpath_bed.cpp -o shortpath_bed.exe) || exit 1

# The step-by-step trace bed compiles a patched copy of the real openbw
# source, so every traced step provably comes from the original.
python3 tools/cppbed/gen_traced.py > /dev/null || exit 1
(cd tools/cppbed && "$CXX" -std=c++17 -I. -I../../../openbw -O0 -g shortpath_trace2.cpp -o shortpath_trace2.exe) || exit 1

echo "== building Zan short path port =="
"$ZANC" src/beds/shortpath_main.zan \
        "${ZAN_SOURCES_BASE[@]}" \
        "${ZAN_SOURCES_SCENARIOS[@]}" \
        "${ZAN_SOURCES_CORE[@]}" \
        "${ZAN_SOURCES_REGIONS[@]}" \
        "${ZAN_SOURCES_ASTAR[@]}" \
        "${ZAN_SOURCES_SHORTPATH[@]}" \
        --auto-stdlib $ZAN_STDLIB_ARG -o build/shortpath_zan.exe || exit 1

FAIL=0
for v in $VARIANTS; do
    ./tools/cppbed/shortpath_bed.exe "$v" > "build/sp_cpp_$v.txt" 2>&1
    ./build/shortpath_zan.exe "$v" > "build/sp_zan_$v.txt" 2>&1
    if diff -u "build/sp_cpp_$v.txt" "build/sp_zan_$v.txt" > "build/sp_diff_$v.txt"; then
        n=$(grep -c "^Q " "build/sp_cpp_$v.txt")
        echo "  variant $v: OK ($n queries)"
    else
        echo "  variant $v: MISMATCH -> build/sp_diff_$v.txt"
        head -20 "build/sp_diff_$v.txt"
        FAIL=1
    fi
done

if [ "$FAIL" -ne 0 ]; then
    echo "== FAILED =="
    exit 1
fi
echo "== SHORT PATH BIT-IDENTICAL =="
