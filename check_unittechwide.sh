#!/usr/bin/env bash
# The differential test for the research rules layer.
#
# The C++ side drives the real openbw unit_can_research,
# unit_can_upgrade and unit_can_use_tech; the Zan side drives the port.
# Both dump the same lines, so the traces must match byte for byte.
#
# Usage: check_techrules.sh [variants]

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
ZAN_STDLIB_ARG=""
if [ -n "$ZAN_STDLIB" ]; then ZAN_STDLIB_ARG="--stdlib-path $ZAN_STDLIB"; fi
CXX="${CXX:-g++}"

cd "$ROOT"

. ./zan_sources.sh

VARIANTS="${1:-0 1 2 3 4 5 6}"

ZAN_SRC=(
    src/beds/unittechwide_main.zan \
    "${ZAN_SOURCES_BASE[@]}" \
    "${ZAN_SOURCES_CORE[@]}"
)

echo "== building the Zan side =="
"$ZANC" "${ZAN_SRC[@]}" --auto-stdlib $ZAN_STDLIB_ARG -o build/unittechwide_zan.exe || exit 1

echo "== building the C++ side =="
"$CXX" -std=c++17 -I../openbw -O0 -g tools/cppbed/unittechwide_bed.cpp -o ./tools/cppbed/unittechwide_bed.exe || exit 1

mkdir -p build
FAIL=0
for v in $VARIANTS; do
    ./build/unittechwide_zan.exe "$v" > build/unittechwide_zan.txt 2>&1
    ./tools/cppbed/unittechwide_bed.exe "$v" > build/unittechwide_cpp.txt 2>&1
    if ! diff -u build/unittechwide_cpp.txt build/unittechwide_zan.txt > build/unittechwide_diff.txt; then
        echo "FAIL variant $v"
        head -30 build/unittechwide_diff.txt
        FAIL=1
    fi
    echo "variant $v done"
done

if [ "$FAIL" != 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "all research rules match bit for bit"
