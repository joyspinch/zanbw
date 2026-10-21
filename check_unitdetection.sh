#!/usr/bin/env bash
# The differential test for the detection and sight range layer.
#
# The C++ side drives the real openbw unit queries; the Zan side drives
# the port. Both dump the same lines, so the traces must match byte for
# byte.
#
# Usage: check_unitdetection.sh [variants]

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
ZAN_STDLIB_ARG=""
if [ -n "$ZAN_STDLIB" ]; then ZAN_STDLIB_ARG="--stdlib-path $ZAN_STDLIB"; fi
CXX="${CXX:-g++}"

cd "$ROOT"

. ./zan_sources.sh

VARIANTS="${1:-0 1 2 3 4 5 6}"

ZAN_SRC=(
    src/beds/unitdetection_main.zan \
    "${ZAN_SOURCES_BASE[@]}" \
    "${ZAN_SOURCES_CORE[@]}"
)

echo "== building the Zan side =="
"$ZANC" "${ZAN_SRC[@]}" --auto-stdlib $ZAN_STDLIB_ARG -o build/unitdetection_zan.exe || exit 1

echo "== building the C++ side =="
"$CXX" -std=c++17 -I../openbw -O0 -g tools/cppbed/unitdetection_bed.cpp -o ./tools/cppbed/unitdetection_bed.exe || exit 1

mkdir -p build
FAIL=0
for v in $VARIANTS; do
    ./build/unitdetection_zan.exe "$v" > build/unitdetection_zan.txt 2>&1
    ./tools/cppbed/unitdetection_bed.exe "$v" > build/unitdetection_cpp.txt 2>&1
    if ! diff -u build/unitdetection_cpp.txt build/unitdetection_zan.txt > build/unitdetection_diff.txt; then
        echo "FAIL variant $v"
        head -30 build/unitdetection_diff.txt
        FAIL=1
    fi
    echo "variant $v done"
done

if [ "$FAIL" != 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "all detection and sight queries match bit for bit"
