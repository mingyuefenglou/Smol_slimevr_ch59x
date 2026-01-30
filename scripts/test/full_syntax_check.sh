#!/bin/bash
echo "=== Full Syntax Check ==="

GCC=gcc
CFLAGS="-c -Wall -Wextra -fsyntax-only"
CFLAGS="$CFLAGS -Iinclude -Isdk/StdPeriphDriver/inc -Isdk/RVMSIS -Isdk/BLE/LIB -Isdk/BLE/HAL/include"
CFLAGS="$CFLAGS -DCH59X -DBUILD_TRACKER -D__INTERRUPT= -D__HIGH_CODE="

echo ""
echo "=== Core Algorithm Files ==="
for src in src/sensor/fusion/*.c; do
    echo -n "  $src: "
    if $GCC $CFLAGS "$src" 2>/dev/null; then
        echo "OK"
    else
        echo "FAIL"
        $GCC $CFLAGS "$src" 2>&1 | head -5
    fi
done

echo ""
echo "=== RF Protocol Files ==="
for src in src/rf/*.c; do
    echo -n "  $src: "
    if $GCC $CFLAGS "$src" 2>/dev/null; then
        echo "OK"
    else
        echo "FAIL"
        $GCC $CFLAGS "$src" 2>&1 | head -5
    fi
done

echo ""
echo "=== SDK Stub Files ==="
for src in sdk/StdPeriphDriver/*.c sdk/RVMSIS/*.c; do
    [ -f "$src" ] || continue
    echo -n "  $src: "
    if $GCC $CFLAGS "$src" 2>/dev/null; then
        echo "OK"
    else
        echo "FAIL"
        $GCC $CFLAGS "$src" 2>&1 | head -5
    fi
done

echo ""
echo "=== Complete ==="
