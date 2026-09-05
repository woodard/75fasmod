#!/bin/bash
set -e

# Resolve binary path across standard and Automake out-of-tree builds
TEST_BIN="./test_power"
if [ ! -x "$TEST_BIN" ]; then
    TEST_BIN="./test/test_power"
fi

if [ ! -x "$TEST_BIN" ]; then
    echo "[ERROR] Test binary 'test_power' not found or not executable."
    exit 1
fi

echo "=== Step 1: Setting Power Level (-s) ==="
SET_LEVEL=$( "$TEST_BIN" -s )
echo "Radio set to: ${SET_LEVEL}"

echo "=== Step 2: Querying Power Level (no flags) ==="
GET_LEVEL=$( "$TEST_BIN" )
echo "Radio read as: ${GET_LEVEL}"

if [ "$SET_LEVEL" = "$GET_LEVEL" ]; then
    echo "[PASS] Power level set (${SET_LEVEL}) matches power level read (${GET_LEVEL})."
    exit 0
else
    echo "[FAIL] Power level mismatch! Set: ${SET_LEVEL} | Read: ${GET_LEVEL}"
    exit 1
fi