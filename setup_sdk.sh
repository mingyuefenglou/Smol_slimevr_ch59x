#!/bin/bash
# SlimeVR CH592 SDK 集成脚本
# 用法: ./setup_sdk.sh <ch592_sdk_path>

set -e

SDK_PATH="${1:-./ch592_sdk}"
PROJECT_PATH="./slimevr_ch59x"

echo "=============================================="
echo "   SlimeVR CH592 SDK 集成脚本"
echo "=============================================="

# 检查 SDK 路径
if [ ! -d "$SDK_PATH" ]; then
    echo "错误: SDK 路径不存在: $SDK_PATH"
    echo ""
    echo "请先克隆 CH592 SDK:"
    echo "  git clone https://github.com/openwch/ch592.git $SDK_PATH"
    echo ""
    echo "或者下载 WeAct Studio 版本:"
    echo "  git clone https://github.com/WeActStudio/WeActStudio.WCH-BLE-Core.git $SDK_PATH"
    exit 1
fi

# 查找 EVT 目录
EVT_PATH=""
if [ -d "$SDK_PATH/EVT" ]; then
    EVT_PATH="$SDK_PATH/EVT"
elif [ -d "$SDK_PATH/SDK" ]; then
    EVT_PATH="$SDK_PATH/SDK"
elif [ -d "$SDK_PATH/CH5xx_firmware_library" ]; then
    EVT_PATH="$SDK_PATH/CH5xx_firmware_library"
fi

if [ -z "$EVT_PATH" ]; then
    echo "错误: 无法找到 EVT/SDK 目录"
    exit 1
fi

echo "找到 SDK: $EVT_PATH"

# 创建目标目录
mkdir -p "$PROJECT_PATH/sdk/StdPeriphDriver"
mkdir -p "$PROJECT_PATH/sdk/StdPeriphDriver/inc"
mkdir -p "$PROJECT_PATH/sdk/BLE/LIB"
mkdir -p "$PROJECT_PATH/sdk/BLE/HAL/include"
mkdir -p "$PROJECT_PATH/sdk/RVMSIS"
mkdir -p "$PROJECT_PATH/sdk/Startup"
mkdir -p "$PROJECT_PATH/sdk/Ld"

# 复制 StdPeriphDriver
echo "复制 StdPeriphDriver..."
if [ -d "$EVT_PATH/EXAM/SRC/StdPeriphDriver" ]; then
    cp -r "$EVT_PATH/EXAM/SRC/StdPeriphDriver/"*.c "$PROJECT_PATH/sdk/StdPeriphDriver/" 2>/dev/null || true
    cp -r "$EVT_PATH/EXAM/SRC/StdPeriphDriver/inc/"*.h "$PROJECT_PATH/sdk/StdPeriphDriver/inc/" 2>/dev/null || true
elif [ -d "$EVT_PATH/StdPeriphDriver" ]; then
    cp -r "$EVT_PATH/StdPeriphDriver/"*.c "$PROJECT_PATH/sdk/StdPeriphDriver/" 2>/dev/null || true
    cp -r "$EVT_PATH/StdPeriphDriver/inc/"*.h "$PROJECT_PATH/sdk/StdPeriphDriver/inc/" 2>/dev/null || true
fi

# 复制 BLE 库
echo "复制 BLE 库..."
if [ -d "$EVT_PATH/EXAM/BLE/LIB" ]; then
    cp -r "$EVT_PATH/EXAM/BLE/LIB/"* "$PROJECT_PATH/sdk/BLE/LIB/" 2>/dev/null || true
elif [ -d "$EVT_PATH/BLE/LIB" ]; then
    cp -r "$EVT_PATH/BLE/LIB/"* "$PROJECT_PATH/sdk/BLE/LIB/" 2>/dev/null || true
fi

# 复制 HAL
echo "复制 HAL..."
if [ -d "$EVT_PATH/EXAM/BLE/HAL" ]; then
    cp -r "$EVT_PATH/EXAM/BLE/HAL/include/"*.h "$PROJECT_PATH/sdk/BLE/HAL/include/" 2>/dev/null || true
elif [ -d "$EVT_PATH/BLE/HAL" ]; then
    cp -r "$EVT_PATH/BLE/HAL/include/"*.h "$PROJECT_PATH/sdk/BLE/HAL/include/" 2>/dev/null || true
fi

# 复制 RVMSIS
echo "复制 RVMSIS..."
if [ -d "$EVT_PATH/EXAM/SRC/RVMSIS" ]; then
    cp -r "$EVT_PATH/EXAM/SRC/RVMSIS/"* "$PROJECT_PATH/sdk/RVMSIS/" 2>/dev/null || true
elif [ -d "$EVT_PATH/RVMSIS" ]; then
    cp -r "$EVT_PATH/RVMSIS/"* "$PROJECT_PATH/sdk/RVMSIS/" 2>/dev/null || true
fi

# 复制 Startup
echo "复制 Startup..."
if [ -d "$EVT_PATH/EXAM/SRC/Startup" ]; then
    cp -r "$EVT_PATH/EXAM/SRC/Startup/"* "$PROJECT_PATH/sdk/Startup/" 2>/dev/null || true
elif [ -d "$EVT_PATH/Startup" ]; then
    cp -r "$EVT_PATH/Startup/"* "$PROJECT_PATH/sdk/Startup/" 2>/dev/null || true
fi

# 复制 Linker Script
echo "复制链接脚本..."
if [ -d "$EVT_PATH/EXAM/SRC/Ld" ]; then
    cp -r "$EVT_PATH/EXAM/SRC/Ld/"* "$PROJECT_PATH/sdk/Ld/" 2>/dev/null || true
elif [ -d "$EVT_PATH/Ld" ]; then
    cp -r "$EVT_PATH/Ld/"* "$PROJECT_PATH/sdk/Ld/" 2>/dev/null || true
fi

# 统计复制的文件
echo ""
echo "=============================================="
echo "   SDK 集成完成"
echo "=============================================="
echo ""
echo "复制的文件统计:"
echo "  StdPeriphDriver: $(ls -1 $PROJECT_PATH/sdk/StdPeriphDriver/*.c 2>/dev/null | wc -l) .c 文件"
echo "  StdPeriphDriver/inc: $(ls -1 $PROJECT_PATH/sdk/StdPeriphDriver/inc/*.h 2>/dev/null | wc -l) .h 文件"
echo "  BLE/LIB: $(ls -1 $PROJECT_PATH/sdk/BLE/LIB/* 2>/dev/null | wc -l) 文件"
echo "  RVMSIS: $(ls -1 $PROJECT_PATH/sdk/RVMSIS/* 2>/dev/null | wc -l) 文件"
echo ""

# 检查关键文件
echo "关键文件检查:"
check_file() {
    if [ -f "$1" ]; then
        echo "  ✓ $2"
    else
        echo "  ✗ $2 (缺失)"
    fi
}

check_file "$PROJECT_PATH/sdk/StdPeriphDriver/CH59x_usbdev.c" "CH59x_usbdev.c"
check_file "$PROJECT_PATH/sdk/StdPeriphDriver/CH59x_gpio.c" "CH59x_gpio.c"
check_file "$PROJECT_PATH/sdk/StdPeriphDriver/CH59x_spi.c" "CH59x_spi.c"
check_file "$PROJECT_PATH/sdk/StdPeriphDriver/CH59x_i2c.c" "CH59x_i2c.c"
check_file "$PROJECT_PATH/sdk/BLE/LIB/LIBCH59xBLE.a" "LIBCH59xBLE.a"
check_file "$PROJECT_PATH/sdk/RVMSIS/core_riscv.h" "core_riscv.h"

echo ""
echo "下一步:"
echo "  1. cd $PROJECT_PATH"
echo "  2. make TARGET=tracker  # 编译追踪器"
echo "  3. make TARGET=receiver # 编译接收器"
