#!/bin/bash
set -e

# Resolve binary path across standard and Automake out-of-tree builds
TEST_BIN="./test_radio_control"
if [ ! -x "$TEST_BIN" ]; then
    TEST_BIN="./test/test_radio_control"
fi

if [ ! -x "$TEST_BIN" ]; then
    echo "[ERROR] Test binary 'test_radio_control' not found or not executable."
    exit 1
fi

echo "=== Running Radio Control Tests ==="
echo ""

"$TEST_BIN"

# Check the exit code
TEST_RESULT=$?

echo ""
if [ $TEST_RESULT -eq 0 ]; then
    echo "=== All Radio Control Tests Passed ==="
else
    echo "=== Some Radio Control Tests Failed ==="
fi

exit $TEST_RESULT
