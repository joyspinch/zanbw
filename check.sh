#!/usr/bin/env bash
# Bit-exact differential test: the Zan port vs the C++ reference bed.
#
#   ./check.sh              -- all built-in scenarios, 600 frames each
#   ./check.sh s1 300       -- one scenario, 300 frames
#
# Builds both sides, runs them over the same frames, and diffs the traces.
# Any difference (including the RNG audit line) is a hard failure.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
ZANC="${ZANC:-D:/project/zan-lang/build/zanc.exe}"
CXX="${CXX:-g++}"

cd "$ROOT"

if [ "$#" -ge 1 ]; then
    SCENARIOS="$1"
    FRAMES="${2:-600}"
else
    SCENARIOS="s0 s1 s2 s3 s4"
    FRAMES=600
fi

echo "== building C++ bed =="
(cd tools/cppbed && "$CXX" -std=c++17 -I../../../openbw -O0 -g main.cpp -o bed.exe) || exit 1

echo "== building Zan port =="
"$ZANC" src/main.zan src/Enums.zan src/Fp.zan src/Dir.zan src/Rng.zan \
        src/Tables.zan src/Unit.zan src/Kinematics.zan src/Sim.zan src/Scenario.zan \
        --auto-stdlib -o build/zanbw.exe || exit 1

FAIL=0
for s in $SCENARIOS; do
    ./tools/cppbed/bed.exe "$s" "$FRAMES" > "build/trace_cpp_$s.txt"
    ./build/zanbw.exe "$s" "$FRAMES" > "build/trace_zan_$s.txt"
    if diff -u "build/trace_cpp_$s.txt" "build/trace_zan_$s.txt" > "build/trace_diff_$s.txt"; then
        echo "  $s: OK ($FRAMES frames bit-identical)"
    else
        echo "  $s: MISMATCH -> build/trace_diff_$s.txt"
        head -30 "build/trace_diff_$s.txt"
        FAIL=1
    fi
done

if [ "$FAIL" -ne 0 ]; then
    echo "== FAILED =="
    exit 1
fi
echo "== ALL SCENARIOS BIT-IDENTICAL =="
