#!/bin/bash
#============================================================================
# SlimeVR CH592/CH591 固件编译脚本 (带版本管理)
# SlimeVR CH592/CH591 Firmware Build Script (with Version Management)
#
# 功能 / Features:
#   - 自动生成 bin, hex, uf2 文件
#   - 支持版本号管理
#   - 支持从 SlimeVR 官方源更新
#   - 支持 CH591 和 CH592
#============================================================================

set -e

# 颜色定义 / Color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 项目配置 / Project configuration
PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
OUTPUT_DIR="${PROJECT_DIR}/output"
VERSION_FILE="${PROJECT_DIR}/VERSION"
SLIMEVR_SRC_DIR="${PROJECT_DIR}/slimevr_src"

# 默认版本 / Default version
VERSION_MAJOR=1
VERSION_MINOR=0
VERSION_PATCH=0

# UF2 配置 / UF2 configuration
UF2_FAMILY_CH592=0x4b50e11a  # WCH CH592 family ID
UF2_BASE_ADDR=0x00001000     # Flash base address

#============================================================================
# 辅助函数 / Helper functions
#============================================================================

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }
log_step() { echo -e "${BLUE}[STEP]${NC} $1"; }

#============================================================================
# 版本管理 / Version management
#============================================================================

load_version() {
    if [ -f "$VERSION_FILE" ]; then
        source "$VERSION_FILE"
    fi
    VERSION="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
    log_info "当前版本 / Current version: v${VERSION}"
}

save_version() {
    cat > "$VERSION_FILE" << EOF
# SlimeVR CH592 Firmware Version
VERSION_MAJOR=${VERSION_MAJOR}
VERSION_MINOR=${VERSION_MINOR}
VERSION_PATCH=${VERSION_PATCH}
BUILD_DATE="$(date +%Y-%m-%d)"
BUILD_TIME="$(date +%H:%M:%S)"
EOF
}

bump_version() {
    case "$1" in
        major) VERSION_MAJOR=$((VERSION_MAJOR + 1)); VERSION_MINOR=0; VERSION_PATCH=0 ;;
        minor) VERSION_MINOR=$((VERSION_MINOR + 1)); VERSION_PATCH=0 ;;
        patch) VERSION_PATCH=$((VERSION_PATCH + 1)) ;;
    esac
    save_version
    log_info "版本更新为 / Version bumped to: v${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
}

#============================================================================
# UF2 转换工具 / UF2 conversion tool
#============================================================================

create_uf2_tool() {
    mkdir -p "${PROJECT_DIR}/tools"
    cat > "${PROJECT_DIR}/tools/bin2uf2.py" << 'PYTHON'
#!/usr/bin/env python3
"""
BIN to UF2 Converter for CH592/CH591
Converts binary firmware to UF2 format for drag-and-drop flashing
"""

import sys
import struct

UF2_MAGIC_START0 = 0x0A324655  # "UF2\n"
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY = 0x00002000

def convert_to_uf2(input_file, output_file, family_id=0x4b50e11a, base_addr=0x00001000):
    with open(input_file, 'rb') as f:
        data = f.read()
    
    blocks = []
    num_blocks = (len(data) + 255) // 256
    
    for block_no in range(num_blocks):
        ptr = block_no * 256
        chunk = data[ptr:ptr + 256]
        chunk = chunk.ljust(256, b'\x00')
        
        flags = UF2_FLAG_FAMILY
        block = struct.pack('<IIIIIIII',
            UF2_MAGIC_START0,
            UF2_MAGIC_START1,
            flags,
            base_addr + ptr,
            256,
            block_no,
            num_blocks,
            family_id
        )
        block += chunk
        block += struct.pack('<I', UF2_MAGIC_END)
        block = block.ljust(512, b'\x00')
        blocks.append(block)
    
    with open(output_file, 'wb') as f:
        f.write(b''.join(blocks))
    
    print(f"Created {output_file}: {len(blocks)} blocks, {len(data)} bytes")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: bin2uf2.py input.bin output.uf2 [family_id] [base_addr]")
        sys.exit(1)
    
    family = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x4b50e11a
    base = int(sys.argv[4], 0) if len(sys.argv) > 4 else 0x00001000
    convert_to_uf2(sys.argv[1], sys.argv[2], family, base)
PYTHON
    chmod +x "${PROJECT_DIR}/tools/bin2uf2.py"
}

#============================================================================
# 工具链检测 / Toolchain detection
#============================================================================

find_toolchain() {
    TOOLCHAINS=(
        "riscv-none-elf-"
        "riscv-none-embed-"
        "riscv64-unknown-elf-"
        "riscv32-unknown-elf-"
    )
    
    for tc in "${TOOLCHAINS[@]}"; do
        if command -v "${tc}gcc" &> /dev/null; then
            CROSS_PREFIX="$tc"
            return 0
        fi
    done
    return 1
}

#============================================================================
# 编译函数 / Build function
#============================================================================

build_target() {
    local target=$1
    local chip=${2:-CH591}
    
    log_step "编译 ${target} 固件 (${chip})..."
    
    # 设置编译器
    if ! find_toolchain; then
        log_error "未找到 RISC-V 工具链! 请安装 xPack RISC-V GCC"
    fi
    
    GCC="${CROSS_PREFIX}gcc"
    OBJCOPY="${CROSS_PREFIX}objcopy"
    SIZE="${CROSS_PREFIX}size"
    
    # 编译标志
    CPU_FLAGS="-march=rv32imac -mabi=ilp32 -msmall-data-limit=8"
    OPT_FLAGS="-Os -ffunction-sections -fdata-sections -fno-common"
    
    INCLUDES="-Iinclude -Isdk/StdPeriphDriver/inc -Isdk/RVMSIS"
    INCLUDES="$INCLUDES -Isdk/BLE/LIB -Isdk/BLE/HAL/include"
    
    # 版本定义
    DEFINES="-D${chip} -DBUILD_${target^^}"
    DEFINES="$DEFINES -DVERSION_MAJOR=${VERSION_MAJOR}"
    DEFINES="$DEFINES -DVERSION_MINOR=${VERSION_MINOR}"
    DEFINES="$DEFINES -DVERSION_PATCH=${VERSION_PATCH}"
    
    # CH591 特定优化
    if [ "$chip" = "CH591" ]; then
        DEFINES="$DEFINES -DMAX_TRACKERS=16"  # CH591 RAM 较小
    else
        DEFINES="$DEFINES -DMAX_TRACKERS=24"
    fi
    
    CFLAGS="$CPU_FLAGS $OPT_FLAGS $INCLUDES $DEFINES -Wall"
    LDFLAGS="$CPU_FLAGS -Wl,--gc-sections -Tsdk/Ld/Link.ld -nostartfiles"
    
    # 创建目录
    mkdir -p "${BUILD_DIR}/${target}" "${OUTPUT_DIR}"
    
    # 源文件
    if [ "$target" = "tracker" ]; then
        SOURCES=$(find src/main_tracker.c src/hal/*.c src/rf/rf_*.c \
                  src/sensor/fusion/vqf_opt.c src/sensor/imu/*.c \
                  src/usb/usb_bootloader.c \
                  sdk/StdPeriphDriver/*.c sdk/RVMSIS/*.c \
                  -name "*.c" 2>/dev/null | tr '\n' ' ')
    else
        SOURCES=$(find src/main_receiver*.c src/hal/*.c src/rf/rf_*.c \
                  src/usb/usb_hid_slime.c \
                  sdk/StdPeriphDriver/*.c sdk/RVMSIS/*.c \
                  -name "*.c" 2>/dev/null | tr '\n' ' ')
    fi
    
    # 编译
    OBJECTS=""
    for src in $SOURCES; do
        obj="${BUILD_DIR}/${target}/$(basename ${src%.c}.o)"
        $GCC $CFLAGS -c "$src" -o "$obj" 2>/dev/null || true
        OBJECTS="$OBJECTS $obj"
    done
    
    # 汇编启动文件
    $GCC $CFLAGS -c sdk/Startup/startup_CH592.S -o "${BUILD_DIR}/${target}/startup.o"
    OBJECTS="$OBJECTS ${BUILD_DIR}/${target}/startup.o"
    
    # 链接
    ELF="${BUILD_DIR}/${target}/slimevr_${target}.elf"
    $GCC $LDFLAGS $OBJECTS -o "$ELF"
    
    # 生成输出文件 (带版本号)
    VERSION_TAG="v${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
    
    # BIN 文件
    BIN="${OUTPUT_DIR}/${target}_${chip}_${VERSION_TAG}.bin"
    $OBJCOPY -O binary "$ELF" "$BIN"
    
    # HEX 文件
    HEX="${OUTPUT_DIR}/${target}_${chip}_${VERSION_TAG}.hex"
    $OBJCOPY -O ihex "$ELF" "$HEX"
    
    # UF2 文件
    UF2="${OUTPUT_DIR}/${target}_${chip}_${VERSION_TAG}.uf2"
    python3 "${PROJECT_DIR}/tools/bin2uf2.py" "$BIN" "$UF2" $UF2_FAMILY_CH592 $UF2_BASE_ADDR
    
    # 创建不带版本号的符号链接 (方便使用)
    ln -sf "$(basename $BIN)" "${OUTPUT_DIR}/${target}.bin"
    ln -sf "$(basename $HEX)" "${OUTPUT_DIR}/${target}.hex"
    ln -sf "$(basename $UF2)" "${OUTPUT_DIR}/${target}.uf2"
    
    # 显示结果
    log_info "${target} 编译完成:"
    $SIZE "$ELF"
    ls -lh "$BIN" "$HEX" "$UF2"
}

#============================================================================
# SlimeVR 源码更新 / SlimeVR source update
#============================================================================

update_slimevr_source() {
    local src_path="$1"
    
    if [ -z "$src_path" ]; then
        log_error "请指定 SlimeVR 源码路径"
    fi
    
    if [ ! -d "$src_path" ]; then
        log_error "源码路径不存在: $src_path"
    fi
    
    log_step "从 $src_path 更新 SlimeVR 源码..."
    
    # 备份当前版本
    if [ -d "$SLIMEVR_SRC_DIR" ]; then
        mv "$SLIMEVR_SRC_DIR" "${SLIMEVR_SRC_DIR}_backup_$(date +%Y%m%d_%H%M%S)"
    fi
    
    # 复制新源码
    mkdir -p "$SLIMEVR_SRC_DIR"
    cp -r "$src_path"/* "$SLIMEVR_SRC_DIR/"
    
    # 检测版本 (如果有版本文件)
    if [ -f "$SLIMEVR_SRC_DIR/version.txt" ]; then
        NEW_VERSION=$(cat "$SLIMEVR_SRC_DIR/version.txt")
        log_info "检测到 SlimeVR 版本: $NEW_VERSION"
    fi
    
    # 自动增加补丁版本
    bump_version patch
    
    log_info "SlimeVR 源码更新完成，新版本: v${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
}

#============================================================================
# 清理 / Clean
#============================================================================

clean_build() {
    log_step "清理构建目录..."
    rm -rf "${BUILD_DIR}"
    rm -f "${OUTPUT_DIR}"/*.bin "${OUTPUT_DIR}"/*.hex "${OUTPUT_DIR}"/*.uf2
    rm -f "${OUTPUT_DIR}"/*.elf "${OUTPUT_DIR}"/*.map
    log_info "清理完成"
}

#============================================================================
# 使用说明 / Usage
#============================================================================

usage() {
    cat << EOF
SlimeVR CH592/CH591 固件编译脚本 v2.0

用法 / Usage:
    $0 [命令] [选项]

命令 / Commands:
    tracker         编译追踪器固件 / Build tracker firmware
    receiver        编译接收器固件 / Build receiver firmware
    all             编译全部固件 / Build all firmware
    clean           清理构建目录 / Clean build directory
    
    version         显示版本 / Show version
    bump <type>     增加版本号 / Bump version (major/minor/patch)
    update <path>   从指定路径更新 SlimeVR 源码 / Update SlimeVR source

选项 / Options:
    --chip <chip>   目标芯片 (CH591/CH592, 默认 CH591)
    --help          显示帮助 / Show help

示例 / Examples:
    $0 tracker                    # 编译追踪器
    $0 all --chip CH591           # 为 CH591 编译全部
    $0 bump minor                 # 增加次版本号
    $0 update ~/slimevr-new       # 更新源码

输出文件 / Output files:
    output/tracker_CH592_v1.0.0.bin
    output/tracker_CH592_v1.0.0.hex
    output/tracker_CH592_v1.0.0.uf2
    output/receiver_CH592_v1.0.0.bin
    output/receiver_CH592_v1.0.0.hex
    output/receiver_CH592_v1.0.0.uf2
EOF
}

#============================================================================
# 主程序 / Main
#============================================================================

cd "$PROJECT_DIR"

# 初始化
load_version
create_uf2_tool

# 默认芯片
CHIP="CH591"

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        tracker)
            build_target tracker $CHIP
            shift
            ;;
        receiver)
            build_target receiver $CHIP
            shift
            ;;
        all)
            build_target tracker $CHIP
            build_target receiver $CHIP
            shift
            ;;
        clean)
            clean_build
            shift
            ;;
        version)
            echo "v${VERSION}"
            shift
            ;;
        bump)
            bump_version "$2"
            shift 2
            ;;
        update)
            update_slimevr_source "$2"
            shift 2
            ;;
        --chip)
            CHIP="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            log_error "未知命令: $1"
            ;;
    esac
done

if [ $# -eq 0 ] && [ -z "$1" ]; then
    usage
fi
