#!/bin/bash
# Test compilation with host GCC (syntax check only)

echo "=== Syntax Check with Host GCC ==="

GCC=gcc
CFLAGS="-c -Wall -Wextra -Werror -fsyntax-only"
CFLAGS="$CFLAGS -Iinclude -Isdk/StdPeriphDriver/inc -Isdk/RVMSIS -Isdk/BLE/LIB -Isdk/BLE/HAL/include"
CFLAGS="$CFLAGS -DCH59X -DBUILD_TRACKER"

# Critical source files
SOURCES="
src/sensor/fusion/vqf_ultra.c
src/rf/rf_ultra.c
"

ERRORS=0
for src in $SOURCES; do
    echo -n "Checking $src... "
    if $GCC $CFLAGS "$src" 2>/dev/null; then
        echo "OK"
    else
        echo "ERRORS"
        $GCC $CFLAGS "$src" 2>&1 | head -20
        ERRORS=$((ERRORS + 1))
    fi
done

echo ""
echo "=== Summary ==="
if [ $ERRORS -eq 0 ]; then
    echo "All syntax checks passed!"
else
    echo "Found errors in $ERRORS file(s)"
fi
