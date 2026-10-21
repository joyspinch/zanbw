#!/usr/bin/env bash
# The differential test for the weapon modification layer.
#
# The C++ side drives the real openbw weapon_max_range and
# get_modified_weapon_cooldown; the Zan side drives the port. Both dump
# the same lines, so the traces must match byte for byte.
#
# Usage: check_weaponmods.sh [variants]

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
CXX="${CXX:-g++}"

cd "$ROOT"

. ./zan_sources.sh

VARIANTS="${1:-0 1 2 3 4 5 6}"

ZAN_SRC=(
    src/beds/weaponmods_main.zan \
    "${ZAN_SOURCES_BASE[@]}" \
    "${ZAN_SOURCES_CORE[@]}"
)

echo "== building the Zan side =="
"$ZANC" "${ZAN_SRC[@]}" --auto-stdlib -o build/weaponmods_zan.exe || exit 1

echo "== building the C++ side =="
"$CXX" -std=c++17 -I../openbw -O0 -g tools/cppbed/weaponmods_bed.cpp -o ./tools/cppbed/weaponmods_bed.exe || exit 1

mkdir -p build
FAIL=0
for v in $VARIANTS; do
    ./build/weaponmods_zan.exe "$v" > build/weaponmods_zan.txt 2>&1
    ./tools/cppbed/weaponmods_bed.exe "$v" > build/weaponmods_cpp.txt 2>&1
    if ! diff -u build/weaponmods_cpp.txt build/weaponmods_zan.txt > build/weaponmods_diff.txt; then
        echo "FAIL variant $v"
        head -30 build/weaponmods_diff.txt
        FAIL=1
    fi
    echo "variant $v done"
done

if [ "$FAIL" != 0 ]; then
    echo "FAILED"
    exit 1
fi
echo "all weapon modifications match bit for bit"
