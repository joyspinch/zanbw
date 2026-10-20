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

. ./zan_sources.sh

if [ "$#" -ge 1 ]; then
    VARIANTS="$1"
else
    VARIANTS="0 1 2 3 4 5 6"
fi

echo "== building C++ path bed =="
(cd tools/cppbed && "$CXX" -std=c++17 -I../../../openbw -O0 -g path_bed.cpp -o path_bed.exe) || exit 1

echo "== building Zan path port =="
"$ZANC" src/beds/path_main.zan \
        "${ZAN_SOURCES_BASE[@]}" \
        "${ZAN_SOURCES_SCENARIOS[@]}" \
        "${ZAN_SOURCES_CORE[@]}" \
        "${ZAN_SOURCES_REGIONS[@]}" \
        "${ZAN_SOURCES_PATH[@]}" \
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
