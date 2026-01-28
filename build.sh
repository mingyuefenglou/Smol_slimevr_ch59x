#!/bin/bash
# SlimeVR CH592 Build Script
# Requires: RISC-V GCC toolchain (MounRiver or xPack)

set -e

#============================================================================
# Configuration
#============================================================================

# Toolchain prefix (change if using different toolchain)
CROSS_PREFIX="${CROSS_PREFIX:-riscv-none-embed-}"

# Alternative toolchain prefixes to try
ALT_PREFIXES=(
    "riscv-none-embed-"
    "riscv64-unknown-elf-"
    "riscv32-unknown-elf-"
    "riscv-none-elf-"
)

# Build target
TARGET="${1:-tracker}"
if [ "$TARGET" != "tracker" ] && [ "$TARGET" != "receiver" ]; then
    echo "Usage: $0 [tracker|receiver]"
    exit 1
fi

# Output directory
BUILD_DIR="build/${TARGET}"
OUTPUT_DIR="output"

#============================================================================
# Find Toolchain
#============================================================================

find_toolchain() {
    for prefix in "${ALT_PREFIXES[@]}"; do
        if command -v "${prefix}gcc" &> /dev/null; then
            CROSS_PREFIX="$prefix"
            return 0
        fi
    done
    return 1
}

if ! command -v "${CROSS_PREFIX}gcc" &> /dev/null; then
    echo "Looking for RISC-V toolchain..."
    if find_toolchain; then
        echo "Found: ${CROSS_PREFIX}gcc"
    else
        echo "ERROR: No RISC-V GCC toolchain found!"
        echo ""
        echo "Install options:"
        echo "  1. MounRiver Studio (includes toolchain)"
        echo "  2. xPack RISC-V GCC: https://github.com/xpack-dev-tools/riscv-none-embed-gcc-xpack"
        echo "  3. Ubuntu/Debian: apt install gcc-riscv64-unknown-elf"
        echo ""
        echo "Then set CROSS_PREFIX environment variable if needed."
        exit 1
    fi
fi

GCC="${CROSS_PREFIX}gcc"
OBJCOPY="${CROSS_PREFIX}objcopy"
SIZE="${CROSS_PREFIX}size"

echo "=============================================="
echo "  SlimeVR CH592 Build"
echo "=============================================="
echo "Target:    ${TARGET}"
echo "Toolchain: ${GCC}"
echo ""

#============================================================================
# Compiler Flags
#============================================================================

CPU_FLAGS="-march=rv32imac -mabi=ilp32 -msmall-data-limit=8"

OPT_FLAGS="-Os -ffunction-sections -fdata-sections -fno-common"
OPT_FLAGS="$OPT_FLAGS -fno-exceptions -fno-unwind-tables"
OPT_FLAGS="$OPT_FLAGS -fshort-enums -fomit-frame-pointer"

WARN_FLAGS="-Wall -Wextra -Wno-unused-parameter"

INCLUDES="-Iinclude -Isdk/StdPeriphDriver/inc -Isdk/RVMSIS"
INCLUDES="$INCLUDES -Isdk/BLE/LIB -Isdk/BLE/HAL/include"

if [ "$TARGET" = "tracker" ]; then
    DEFINES="-DCH59X -DBUILD_TRACKER"
else
    DEFINES="-DCH59X -DBUILD_RECEIVER"
fi

CFLAGS="$CPU_FLAGS $OPT_FLAGS $WARN_FLAGS $INCLUDES $DEFINES"
LDFLAGS="$CPU_FLAGS -Wl,--gc-sections -Wl,-Map=${BUILD_DIR}/${TARGET}.map"
LDFLAGS="$LDFLAGS -Tsdk/Ld/Link.ld -nostartfiles"

#============================================================================
# Source Files
#============================================================================

# Common sources
COMMON_SRC=(
    sdk/StdPeriphDriver/CH59x_gpio.c
    sdk/StdPeriphDriver/CH59x_clk.c
    sdk/StdPeriphDriver/CH59x_timer.c
    sdk/StdPeriphDriver/CH59x_spi.c
    sdk/StdPeriphDriver/CH59x_i2c.c
    sdk/StdPeriphDriver/CH59x_flash.c
    sdk/StdPeriphDriver/CH59x_usbdev.c
    sdk/StdPeriphDriver/CH59x_pwr.c
    sdk/StdPeriphDriver/CH59x_adc.c
    sdk/RVMSIS/core_riscv.c
    src/hal/hal_gpio.c
    src/hal/hal_timer.c
    src/hal/hal_storage.c
    src/rf/rf_ultra.c
)

if [ "$TARGET" = "tracker" ]; then
    APP_SRC=(
        src/main_tracker.c
        src/hal/hal_spi.c
        src/hal/hal_i2c.c
        src/sensor/imu/icm45686.c
        src/sensor/fusion/vqf_ultra.c
        src/rf/rf_transmitter.c
        src/usb/usb_bootloader.c
    )
else
    APP_SRC=(
        src/main_receiver_opt.c
        src/rf/rf_receiver.c
        src/usb/usb_hid_slime.c
    )
fi

ASM_SRC=(
    sdk/Startup/startup_CH592.S
)

SOURCES=("${COMMON_SRC[@]}" "${APP_SRC[@]}")

#============================================================================
# Build
#============================================================================

mkdir -p "${BUILD_DIR}"
mkdir -p "${OUTPUT_DIR}"

echo "Compiling..."

OBJECTS=()
for src in "${SOURCES[@]}"; do
    obj="${BUILD_DIR}/$(basename ${src%.c}.o)"
    echo "  CC $src"
    $GCC $CFLAGS -c "$src" -o "$obj"
    OBJECTS+=("$obj")
done

for src in "${ASM_SRC[@]}"; do
    obj="${BUILD_DIR}/$(basename ${src%.S}.o)"
    echo "  AS $src"
    $GCC $CFLAGS -c "$src" -o "$obj"
    OBJECTS+=("$obj")
done

echo ""
echo "Linking..."
ELF="${BUILD_DIR}/${TARGET}.elf"
$GCC $LDFLAGS "${OBJECTS[@]}" -o "$ELF"

echo ""
echo "Generating output files..."

# Generate HEX file
HEX="${OUTPUT_DIR}/${TARGET}.hex"
$OBJCOPY -O ihex "$ELF" "$HEX"
echo "  HEX: $HEX"

# Generate BIN file
BIN="${OUTPUT_DIR}/${TARGET}.bin"
$OBJCOPY -O binary "$ELF" "$BIN"
echo "  BIN: $BIN"

echo ""
echo "Memory usage:"
$SIZE "$ELF"

echo ""
echo "=============================================="
echo "  Build Complete!"
echo "=============================================="
echo ""
echo "Output files in ${OUTPUT_DIR}/:"
ls -lh "${OUTPUT_DIR}/"*.{hex,bin} 2>/dev/null || true
echo ""
echo "Flash using:"
echo "  wchisp flash ${BIN}"
echo "  or WCHISPTool (Windows)"
