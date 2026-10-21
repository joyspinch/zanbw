#!/usr/bin/env bash
# The differential test for the order layer.
#
# The C++ side drives the real openbw order functions; the Zan side
# drives the port. Both run the same script of order operations and
# dump the same state after every step. The traces must match byte for
# byte.
#
# Usage: check_order.sh [variants] [scenarios]

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
CXX="${CXX:-g++}"

cd "$ROOT"

. ./zan_sources.sh

VARIANTS="${1:-0 1 2 3 4 5 6}"
SCENARIOS="${2:-0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15}"

ZAN_SRC=(
    src/beds/order_main.zan \
    "${ZAN_SOURCES_BASE[@]}" \
    "${ZAN_SOURCES_CORE[@]}"
)

echo "== building the Zan side =="
"$ZANC" "${ZAN_SRC[@]}" --auto-stdlib -o build/order_zan.exe || exit 1

echo "== building the C++ side =="
"$CXX" -std=c++17 -I../openbw -O0 -g tools/cppbed/order_bed.cpp -o ./tools/cppbed/order_bed.exe || exit 1

mkdir -p build
FAIL=0
for v in $VARIANTS; do
    for s in $SCENARIOS; do
        ./build/order_zan.exe "$v" "$s" > build/order_zan.txt 2>&1
        ./tools/cppbed/order_bed.exe "$v" "$s" > build/order_cpp.txt 2>&1
        if ! diff -u build/order_cpp.txt build/order_zan.txt > build/order_diff.txt; then
            echo "FAIL variant $v scenario $s"
            head -30 build/order_diff.txt
            FAIL=1
        fi
    done
    echo "variant $v done"
done

if [ "$FAIL" != 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "all order scenarios match bit for bit"
