#!/usr/bin/env bash
# The differential test for the build queue layer.
#
# The C++ side drives the real openbw get_modified_unit_speed,
# get_modified_unit_acceleration and get_modified_unit_turn_rate; the
# Zan side drives the port. Both dump the same lines, so the traces
# must match byte for byte.
#
# Usage: check_buildqueue.sh [variants]

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
ZAN_STDLIB_ARG=""
if [ -n "$ZAN_STDLIB" ]; then ZAN_STDLIB_ARG="--stdlib-path $ZAN_STDLIB"; fi
CXX="${CXX:-g++}"

cd "$ROOT"

. ./zan_sources.sh

VARIANTS="${1:-0 1 2 3 4 5 6}"

ZAN_SRC=(
    src/beds/buildqueue_main.zan \
    "${ZAN_SOURCES_BASE[@]}" \
    "${ZAN_SOURCES_CORE[@]}"
)

echo "== building the Zan side =="
"$ZANC" "${ZAN_SRC[@]}" --auto-stdlib $ZAN_STDLIB_ARG -o build/buildqueue_zan.exe || exit 1

echo "== building the C++ side =="
"$CXX" -std=c++17 -I../openbw -O0 -g tools/cppbed/buildqueue_bed.cpp -o ./tools/cppbed/buildqueue_bed.exe || exit 1

mkdir -p build
FAIL=0
for v in $VARIANTS; do
    ./build/buildqueue_zan.exe "$v" > build/buildqueue_zan.txt 2>&1
    ./tools/cppbed/buildqueue_bed.exe "$v" > build/buildqueue_cpp.txt 2>&1
    if ! diff -u build/buildqueue_cpp.txt build/buildqueue_zan.txt > build/buildqueue_diff.txt; then
        echo "FAIL variant $v"
        head -30 build/buildqueue_diff.txt
        FAIL=1
    fi
    echo "variant $v done"
done

if [ "$FAIL" != 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "all build queues match bit for bit"
