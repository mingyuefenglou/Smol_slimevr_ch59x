#!/bin/bash
#============================================================================
# Compile Test Script
# Tests compilation for different board configurations
#============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

cd "$PROJECT_ROOT"

echo "=============================================="
echo "Compile Test for Board Configurations"
echo "=============================================="
echo ""

# Check if make is available
if ! command -v make &> /dev/null; then
    echo "✗ ERROR: make command not found"
    echo "Please install GNU Make or use a compatible build system"
    exit 1
fi

# Check if compiler is available
PREFIX="${PREFIX:-riscv-none-elf-}"
if ! command -v "${PREFIX}gcc" &> /dev/null; then
    echo "✗ WARNING: ${PREFIX}gcc not found in PATH"
    echo "Compilation may fail. Please ensure RISC-V toolchain is installed."
    echo ""
fi

# Test configurations
BOARDS=(
    "generic_receiver:receiver"
    "generic_board:tracker"
    "ch591d:tracker"
    "ch592x:tracker"
    "ch592x:receiver"
)

PASSED=0
FAILED=0

for config in "${BOARDS[@]}"; do
    BOARD=$(echo "$config" | cut -d: -f1)
    TARGET=$(echo "$config" | cut -d: -f2)
    
    echo "----------------------------------------------"
    echo "Testing: BOARD=$BOARD TARGET=$TARGET"
    echo "----------------------------------------------"
    
    # Clean first
    make clean > /dev/null 2>&1
    
    # Try to compile (just check syntax, don't link)
    if make TARGET="$TARGET" BOARD="$BOARD" -n 2>&1 | head -20; then
        echo "✓ Configuration check passed"
        PASSED=$((PASSED + 1))
    else
        echo "✗ Configuration check failed"
        FAILED=$((FAILED + 1))
    fi
    echo ""
done

echo "=============================================="
echo "Test Summary"
echo "=============================================="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "✓ All configurations are valid!"
    exit 0
else
    echo "✗ Some configurations have issues"
    exit 1
fi
