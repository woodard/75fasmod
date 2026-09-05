#!/bin/bash
set -e

TEST_BIN="./test/test_single_dual"
if [ ! -x "$TEST_BIN" ]; then
    TEST_BIN="./test/test_single_dual"
fi

if [ ! -x "$TEST_BIN" ]; then
    echo "[ERROR] Test binary 'test_single_dual' not found."
    exit 1
fi

echo "=== Running Single/Dual Band Tests ==="
"$TEST_BIN"
