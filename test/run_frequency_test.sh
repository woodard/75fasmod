#!/bin/bash
# Script runner for test_frequency
# This script runs the frequency test twice and compares the results

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
TEST_BIN="${PROJECT_DIR}/test_frequency"

# Check if the test binary exists
if [ ! -x "$TEST_BIN" ]; then
    echo "[ERROR] Test binary not found at $TEST_BIN"
    echo "[INFO] Run 'make check' from the project directory first"
    exit 1
fi

# Set a test frequency (within the 2m ham band: 144-148 MHz)
SET_FREQ="145.500"

echo "========================================"
echo "Frequency Test - Setting Frequency"
echo "========================================"

# First invocation: Set the frequency
echo "[RUNNING] First invocation with -s $SET_FREQ..."
FIRST_OUTPUT=$( "$TEST_BIN" -s "$SET_FREQ" 2>&1 )
echo "$FIRST_OUTPUT"

# Extract the frequency that was set (from [OUTPUT] line)
FIRST_FREQ=$( echo "$FIRST_OUTPUT" | grep "^\[OUTPUT\]" | awk '{print $2}' | tr -d ' ' )

echo ""
echo "========================================"
echo "Frequency Test - Getting Frequency"
echo "========================================"

# Second invocation: Get the frequency
echo "[RUNNING] Second invocation without -s..."
SECOND_OUTPUT=$( "$TEST_BIN" 2>&1 )
echo "$SECOND_OUTPUT"

# Extract the frequency that was read (from [OUTPUT] line)
SECOND_FREQ=$( echo "$SECOND_OUTPUT" | grep "^\[OUTPUT\]" | awk '{print $2}' | tr -d ' ' )

echo ""
echo "========================================"
echo "Comparison"
echo "========================================"

echo "First frequency (set):  $FIRST_FREQ MHz"
echo "Second frequency (get): $SECOND_FREQ MHz"

# Compare the two frequencies (with some tolerance for floating point)
FIRST_INT=$( echo "$FIRST_FREQ" | awk '{printf "%.0f", $1 * 1000}' )
SECOND_INT=$( echo "$SECOND_FREQ" | awk '{printf "%.0f", $1 * 1000}' )

if [ "$FIRST_INT" = "$SECOND_INT" ]; then
    echo "[SUCCESS] Frequencies match!"
    exit 0
else
    echo "[FAIL] Frequencies do not match!"
    echo "  Expected: $FIRST_FREQ MHz"
    echo "  Got:      $SECOND_FREQ MHz"
    exit 1
fi
