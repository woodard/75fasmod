#!/bin/bash
set -e

# Resolve binary path across standard and Automake out-of-tree builds
TEST_BIN="./test_mode"
if [ ! -x "$TEST_BIN" ]; then
    TEST_BIN="./test/test_mode"
fi

if [ ! -x "$TEST_BIN" ]; then
    echo "[ERROR] Test binary 'test_mode' not found or not executable."
    exit 1
fi

echo "=== Step 1: Setting Mode (-s USB) ==="
SET_MODE=$("$TEST_BIN" -s USB)
echo "Radio set to: ${SET_MODE}"

echo "=== Step 2: Querying Mode (no flags) ==="
GET_MODE=$("$TEST_BIN")
echo "Radio read as: ${GET_MODE}"

if [ "$SET_MODE" = "$GET_MODE" ]; then
    echo "[PASS] Mode set (${SET_MODE}) matches mode read (${GET_MODE})."
    exit 0
else
    echo "[FAIL] Mode mismatch! Set: ${SET_MODE} | Read: ${GET_MODE}"
    exit 1
fi