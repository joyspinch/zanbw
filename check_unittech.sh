#!/usr/bin/env bash
# The differential test for the unit tech / upgrade permission layer.
#
# The C++ side drives the real openbw unit_can_research /
# unit_can_use_tech / unit_can_upgrade over every tech, upgrade and
# unit type; the Zan side drives the port. Both dump the same lines, so
# the traces must match byte for byte.
#
# Usage: check_unittech.sh [variants]

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
CXX="${CXX:-g++}"

cd "$ROOT"

. ./zan_sources.sh

VARIANTS="${1:-0 1 2 3 4 5 6}"

ZAN_SRC=(
    src/beds/unittech_main.zan \
    "${ZAN_SOURCES_BASE[@]}" \
    "${ZAN_SOURCES_CORE[@]}"
)

echo "== building the Zan side =="
"$ZANC" "${ZAN_SRC[@]}" --auto-stdlib -o build/unittech_zan.exe || exit 1

echo "== building the C++ side =="
"$CXX" -std=c++17 -I../openbw -O0 -g tools/cppbed/unittech_bed.cpp -o ./tools/cppbed/unittech_bed.exe || exit 1

mkdir -p build
FAIL=0
for v in $VARIANTS; do
    ./build/unittech_zan.exe "$v" > build/unittech_zan.txt 2>&1
    ./tools/cppbed/unittech_bed.exe "$v" > build/unittech_cpp.txt 2>&1
    if ! diff -u build/unittech_cpp.txt build/unittech_zan.txt > build/unittech_diff.txt; then
        echo "FAIL variant $v"
        head -30 build/unittech_diff.txt
        FAIL=1
    fi
    echo "variant $v done"
done

if [ "$FAIL" != 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "all unit tech predicates match bit for bit"
