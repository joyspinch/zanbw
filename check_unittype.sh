#!/usr/bin/env bash
# The differential test for the unit type predicate layer.
#
# The C++ side drives the real openbw unit type predicates over the
# whole unit type table; the Zan side drives the port. Both dump the
# same lines, so the traces must match byte for byte.
#
# Usage: check_unittype.sh [variants]

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
ZAN_STDLIB_ARG=""
if [ -n "$ZAN_STDLIB" ]; then ZAN_STDLIB_ARG="--stdlib-path $ZAN_STDLIB"; fi
CXX="${CXX:-g++}"

cd "$ROOT"

. ./zan_sources.sh

VARIANTS="${1:-0 1 2 3 4 5 6}"

ZAN_SRC=(
    src/beds/unittype_main.zan \
    "${ZAN_SOURCES_BASE[@]}" \
    "${ZAN_SOURCES_CORE[@]}"
)

echo "== building the Zan side =="
"$ZANC" "${ZAN_SRC[@]}" --auto-stdlib $ZAN_STDLIB_ARG -o build/unittype_zan.exe || exit 1

echo "== building the C++ side =="
"$CXX" -std=c++17 -I../openbw -O0 -g tools/cppbed/unittype_bed.cpp -o ./tools/cppbed/unittype_bed.exe || exit 1

mkdir -p build
FAIL=0
for v in $VARIANTS; do
    ./build/unittype_zan.exe "$v" > build/unittype_zan.txt 2>&1
    ./tools/cppbed/unittype_bed.exe "$v" > build/unittype_cpp.txt 2>&1
    if ! diff -u build/unittype_cpp.txt build/unittype_zan.txt > build/unittype_diff.txt; then
        echo "FAIL variant $v"
        head -30 build/unittype_diff.txt
        FAIL=1
    fi
    echo "variant $v done"
done

if [ "$FAIL" != 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "all unit type predicates match bit for bit"
