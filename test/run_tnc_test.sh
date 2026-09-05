#!/bin/bash
set -e

TEST_BIN="./test/test_tnc"
if [ ! -x "$TEST_BIN" ]; then
    TEST_BIN="./test/test_tnc"
fi

if [ ! -x "$TEST_BIN" ]; then
    echo "[ERROR] Test binary 'test_tnc' not found."
    exit 1
fi

echo "=== Running TNC Control Tests ==="
"$TEST_BIN"
