#!/bin/bash
#============================================================================
# Board Configuration Syntax Check
# Checks if board configuration files are correct
#============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BOARD="${1:-generic_board}"

echo "=============================================="
echo "Board Configuration Syntax Check"
echo "Board: $BOARD"
echo "=============================================="
echo ""

# Check if board directory exists
BOARD_DIR="$PROJECT_ROOT/board"
if [ ! -d "$BOARD_DIR" ]; then
    echo "✗ ERROR: board/ directory not found"
    exit 1
fi

# Check board.h
if [ ! -f "$BOARD_DIR/board.h" ]; then
    echo "✗ ERROR: board/board.h not found"
    exit 1
else
    echo "✓ board/board.h exists"
fi

# Check board-specific files
case "$BOARD" in
    generic_receiver)
        if [ ! -f "$BOARD_DIR/generic_receiver/config.h" ]; then
            echo "✗ ERROR: board/generic_receiver/config.h not found"
            exit 1
        else
            echo "✓ board/generic_receiver/config.h exists"
        fi
        if [ ! -f "$BOARD_DIR/generic_receiver/pins.h" ]; then
            echo "✗ ERROR: board/generic_receiver/pins.h not found"
            exit 1
        else
            echo "✓ board/generic_receiver/pins.h exists"
        fi
        ;;
    generic_board)
        if [ ! -f "$BOARD_DIR/generic_board/config.h" ]; then
            echo "✗ ERROR: board/generic_board/config.h not found"
            exit 1
        else
            echo "✓ board/generic_board/config.h exists"
        fi
        if [ ! -f "$BOARD_DIR/generic_board/pins.h" ]; then
            echo "✗ ERROR: board/generic_board/pins.h not found"
            exit 1
        else
            echo "✓ board/generic_board/pins.h exists"
        fi
        ;;
    ch591d)
        if [ ! -f "$BOARD_DIR/mingyue_slimevr/ch591d/config.h" ]; then
            echo "✗ ERROR: board/mingyue_slimevr/ch591d/config.h not found"
            exit 1
        else
            echo "✓ board/mingyue_slimevr/ch591d/config.h exists"
        fi
        ;;
    ch592x)
        if [ ! -f "$BOARD_DIR/mingyue_slimevr/ch592x/config.h" ]; then
            echo "✗ ERROR: board/mingyue_slimevr/ch592x/config.h not found"
            exit 1
        else
            echo "✓ board/mingyue_slimevr/ch592x/config.h exists"
        fi
        ;;
    *)
        echo "✗ ERROR: Unknown board: $BOARD"
        echo "Supported boards: generic_receiver, generic_board, ch591d, ch592x"
        exit 1
        ;;
esac

# Check if board.h includes correct files
echo ""
echo "Checking board.h includes..."

if grep -q "BOARD_GENERIC_RECEIVER" "$BOARD_DIR/board.h"; then
    if grep -q "generic_receiver/config.h" "$BOARD_DIR/board.h"; then
        echo "✓ BOARD_GENERIC_RECEIVER includes generic_receiver/config.h"
    else
        echo "✗ ERROR: BOARD_GENERIC_RECEIVER should include generic_receiver/config.h"
        exit 1
    fi
fi

if grep -q "BOARD_GENERIC_BOARD" "$BOARD_DIR/board.h"; then
    if grep -q "generic_board/config.h" "$BOARD_DIR/board.h"; then
        echo "✓ BOARD_GENERIC_BOARD includes generic_board/config.h"
    else
        echo "✗ ERROR: BOARD_GENERIC_BOARD should include generic_board/config.h"
        exit 1
    fi
fi

if grep -q "BOARD_CH591D" "$BOARD_DIR/board.h"; then
    if grep -q "mingyue_slimevr/ch591d/config.h" "$BOARD_DIR/board.h"; then
        echo "✓ BOARD_CH591D includes mingyue_slimevr/ch591d/config.h"
    else
        echo "✗ ERROR: BOARD_CH591D should include mingyue_slimevr/ch591d/config.h"
        exit 1
    fi
fi

if grep -q "BOARD_CH592X" "$BOARD_DIR/board.h"; then
    if grep -q "mingyue_slimevr/ch592x/config.h" "$BOARD_DIR/board.h"; then
        echo "✓ BOARD_CH592X includes mingyue_slimevr/ch592x/config.h"
    else
        echo "✗ ERROR: BOARD_CH592X should include mingyue_slimevr/ch592x/config.h"
        exit 1
    fi
fi

# Check Makefile
echo ""
echo "Checking Makefile configuration..."

if grep -q "BOARD=generic_board" "$PROJECT_ROOT/Makefile" || grep -q "BOARD_GENERIC_BOARD" "$PROJECT_ROOT/Makefile"; then
    echo "✓ Makefile supports generic_board"
else
    echo "✗ WARNING: Makefile may not support generic_board"
fi

# Check if source files include board.h
echo ""
echo "Checking source files include board.h..."

if grep -q '#include "board.h"' "$PROJECT_ROOT/src/main_tracker.c"; then
    echo "✓ src/main_tracker.c includes board.h"
else
    echo "✗ WARNING: src/main_tracker.c may not include board.h"
fi

if grep -q '#include "board.h"' "$PROJECT_ROOT/src/main_receiver.c"; then
    echo "✓ src/main_receiver.c includes board.h"
else
    echo "✗ WARNING: src/main_receiver.c may not include board.h"
fi

echo ""
echo "=============================================="
echo "✓ All checks passed!"
echo "=============================================="

