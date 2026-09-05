#!/bin/bash
set -e

# Resolve binary path across standard and Automake out-of-tree builds
TEST_BIN="./test_frequency"
if [ ! -x "$TEST_BIN" ]; then
    TEST_BIN="./test/test_frequency"
fi

if [ ! -x "$TEST_BIN" ]; then
    echo "[ERROR] Test binary 'test_frequency' not found or not executable."
    exit 1
fi

echo "=== Step 1: Setting Frequency (-s) ==="
SET_FREQ=$( "$TEST_BIN" -s )
echo "Radio set to: ${SET_FREQ} MHz"

echo "=== Step 2: Querying Frequency (no flags) ==="
GET_FREQ=$( "$TEST_BIN" )
echo "Radio read as: ${GET_FREQ} MHz"

# Floating-point equivalence check with tolerance
MATCH=$( awk -v a="$SET_FREQ" -v b="$GET_FREQ" 'BEGIN { print (abs(a - b) < 0.001) ? "1" : "0" } function abs(x) { return x < 0 ? -x : x }' )

if [ "$MATCH" -eq 1 ]; then
    echo "[PASS] Frequency set (${SET_FREQ} MHz) matches frequency read (${GET_FREQ} MHz)."
    exit 0
else
    echo "[FAIL] Frequency mismatch! Set: ${SET_FREQ} MHz | Read: ${GET_FREQ} MHz"
    exit 1
fi