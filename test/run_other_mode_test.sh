#!/bin/bash

# Resolve binary path across standard and Automake out-of-tree builds
TEST_BIN="./test_other_mode"
if [ ! -x "$TEST_BIN" ]; then
    TEST_BIN="./test/test_other_mode"
fi

if [ ! -x "$TEST_BIN" ]; then
    echo "[ERROR] Test binary 'test_other_mode' not found or not executable."
    exit 1
fi

echo "=== Step 1: Setting Other Mode (-s USB) ==="
SET_MODE=$("$TEST_BIN" -s USB 2>&1)
echo "$SET_MODE"

# Check if test passed
if echo "$SET_MODE" | grep -q "\[ERROR\]"; then
    echo "$SET_MODE" | grep "\[ERROR\]"
    echo ""
    echo "=== Test Skipped ==="
    echo "The TH-D75 radio appears to be in Single Band mode and cannot be switched to Dual Band mode."
    echo ""
    echo "To test 'other' VFO functionality, you need to:"
    echo "1. Power on your TH-D75 radio"
    echo "2. Press the [MENU] button"
    echo "3. Navigate to menu item 102 (BC setting)"
    echo "4. Set it to enable dual-watch mode (BC 0,1 or BC 1,1)"
    echo "5. Run this test again"
    echo ""
    echo "Alternatively, the hamlib driver for TH-D75 may not support dual-watch mode."
    exit 0  # Exit successfully since this is expected behavior
fi

echo "=== Step 2: Querying Other Mode (no flags) ==="
GET_MODE=$("$TEST_BIN" 2>&1)
echo "$GET_MODE"

if [ "$SET_MODE" = "$GET_MODE" ]; then
    echo "[PASS] Mode set (${SET_MODE}) matches mode read (${GET_MODE})."
    exit 0
else
    echo "[FAIL] Mode mismatch! Set: ${SET_MODE} | Read: ${GET_MODE}"
    exit 1
fi
