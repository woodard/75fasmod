#!/bin/bash
set -e

TEST_BIN="./test_vfo"
if [ ! -x "$TEST_BIN" ]; then
    TEST_BIN="./test/test_vfo"
fi

if [ ! -x "$TEST_BIN" ]; then
    echo "[ERROR] Test binary 'test_vfo' not found."
    exit 1
fi

echo "=== Running VFO Control Tests ==="
"$TEST_BIN"
