#!/bin/bash
#============================================================================
# SlimeVR CH59X 统一编译脚本
# 
# 功能:
#   - 编译 Bootloader
#   - 编译 Tracker/Receiver 固件
#   - 自动合并 HEX 文件
#   - 生成 UF2 文件
#
# 用法:
#   ./build_all.sh [target] [options]
#
# 目标:
#   all       - 编译所有 (默认)
#   tracker   - 仅编译 Tracker
#   receiver  - 仅编译 Receiver
#   bootloader- 仅编译 Bootloader
#   clean     - 清理所有
#
# 选项:
#   --no-boot   - 不合并 Bootloader
#   --debug     - 启用调试模式
#   --mcu=CH591 - 指定 MCU 型号
#============================================================================

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 默认配置
TARGET="all"
MCU="CH592"
DEBUG=0
MERGE_BOOT=1
PREFIX="riscv-none-elf-"

# 目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/output"
BOOT_DIR="${SCRIPT_DIR}/bootloader"
TOOLS_DIR="${SCRIPT_DIR}/tools"

# 解析参数
while [[ $# -gt 0 ]]; do
    case $1 in
        tracker|receiver|bootloader|all|clean)
            TARGET="$1"
            shift
            ;;
        --no-boot)
            MERGE_BOOT=0
            shift
            ;;
        --debug)
            DEBUG=1
            shift
            ;;
        --mcu=*)
            MCU="${1#*=}"
            shift
            ;;
        --prefix=*)
            PREFIX="${1#*=}"
            shift
            ;;
        -h|--help)
            head -30 "$0" | tail -25
            exit 0
            ;;
        *)
            echo -e "${RED}未知选项: $1${NC}"
            exit 1
            ;;
    esac
done

# 打印函数
print_header() {
    echo ""
    echo -e "${BLUE}============================================${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}============================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

# 检查工具链
check_toolchain() {
    if ! command -v ${PREFIX}gcc &> /dev/null; then
        print_error "找不到 ${PREFIX}gcc"
        echo "请安装 RISC-V 工具链或设置 --prefix 参数"
        exit 1
    fi
    print_success "工具链: $(${PREFIX}gcc --version | head -1)"
}

# 编译 Bootloader
build_bootloader() {
    print_header "编译 Bootloader"
    
    cd "${BOOT_DIR}"
    
    if [ ! -f "Makefile" ]; then
        print_error "Bootloader Makefile 不存在"
        return 1
    fi
    
    make clean 2>/dev/null || true
    make PREFIX="${PREFIX}" MCU="${MCU}"
    
    if [ -f "output/bootloader_${MCU,,}.bin" ]; then
        local size=$(stat -c%s "output/bootloader_${MCU,,}.bin")
        if [ $size -gt 4096 ]; then
            print_error "Bootloader 超过 4KB ($size bytes)"
            return 1
        fi
        print_success "Bootloader: $size bytes (max 4096)"
    else
        print_error "Bootloader 编译失败"
        return 1
    fi
    
    cd "${SCRIPT_DIR}"
}

# 编译固件
build_firmware() {
    local target=$1
    print_header "编译 ${target^} 固件"
    
    cd "${SCRIPT_DIR}"
    
    local debug_flag=""
    if [ $DEBUG -eq 1 ]; then
        debug_flag="DEBUG=1"
    fi
    
    make TARGET="${target}" MCU="${MCU}" PREFIX="${PREFIX}" ${debug_flag} clean 2>/dev/null || true
    make TARGET="${target}" MCU="${MCU}" PREFIX="${PREFIX}" ${debug_flag}
    
    local bin_file="${OUTPUT_DIR}/${target}_${MCU,,}.bin"
    if [ -f "$bin_file" ]; then
        local size=$(stat -c%s "$bin_file")
        print_success "${target^}: $size bytes"
    else
        print_error "${target^} 编译失败"
        return 1
    fi
}

# 合并 HEX 文件
merge_hex() {
    local target=$1
    print_header "合并 ${target^} HEX"
    
    local boot_hex="${BOOT_DIR}/output/bootloader_${MCU,,}.hex"
    local app_hex="${OUTPUT_DIR}/${target}_${MCU,,}.hex"
    local combined_hex="${OUTPUT_DIR}/${target}_${MCU,,}_combined.hex"
    
    if [ ! -f "$boot_hex" ]; then
        print_warning "Bootloader HEX 不存在，跳过合并"
        return 0
    fi
    
    if [ ! -f "$app_hex" ]; then
        print_error "Application HEX 不存在"
        return 1
    fi
    
    # 使用 Python 脚本合并
    if [ -f "${TOOLS_DIR}/merge_hex.py" ]; then
        python3 "${TOOLS_DIR}/merge_hex.py" "$boot_hex" "$app_hex" -o "$combined_hex"
    else
        # 简单合并: 移除 Bootloader 的 EOF，追加 Application
        head -n -1 "$boot_hex" > "$combined_hex"
        cat "$app_hex" >> "$combined_hex"
    fi
    
    if [ -f "$combined_hex" ]; then
        print_success "合并完成: $combined_hex"
    else
        print_error "合并失败"
        return 1
    fi
}

# 生成 UF2
generate_uf2() {
    local target=$1
    local bin_file="${OUTPUT_DIR}/${target}_${MCU,,}.bin"
    local uf2_file="${OUTPUT_DIR}/${target}_${MCU,,}.uf2"
    
    if [ ! -f "${TOOLS_DIR}/bin2uf2.py" ]; then
        print_warning "bin2uf2.py 不存在，跳过 UF2 生成"
        return 0
    fi
    
    if [ -f "$bin_file" ]; then
        python3 "${TOOLS_DIR}/bin2uf2.py" "$bin_file" "$uf2_file" --base 0x1000 --family 0x699b62ec
        if [ -f "$uf2_file" ]; then
            print_success "UF2: $uf2_file"
        fi
    fi
}

# 清理
clean_all() {
    print_header "清理所有构建文件"
    
    rm -rf "${BUILD_DIR}"
    rm -f "${OUTPUT_DIR}"/*.bin "${OUTPUT_DIR}"/*.hex "${OUTPUT_DIR}"/*.uf2 "${OUTPUT_DIR}"/*.map
    
    if [ -d "${BOOT_DIR}" ]; then
        cd "${BOOT_DIR}"
        make clean 2>/dev/null || true
        cd "${SCRIPT_DIR}"
    fi
    
    print_success "清理完成"
}

# 显示结果
show_results() {
    print_header "编译结果"
    
    echo ""
    echo "输出文件:"
    ls -lh "${OUTPUT_DIR}"/*.bin "${OUTPUT_DIR}"/*.hex 2>/dev/null | while read line; do
        echo "  $line"
    done
    
    echo ""
    echo "烧录命令:"
    if [ -f "${OUTPUT_DIR}/tracker_${MCU,,}_combined.hex" ]; then
        echo "  Tracker:  wch-link -w ${OUTPUT_DIR}/tracker_${MCU,,}_combined.hex"
    fi
    if [ -f "${OUTPUT_DIR}/receiver_${MCU,,}_combined.hex" ]; then
        echo "  Receiver: wch-link -w ${OUTPUT_DIR}/receiver_${MCU,,}_combined.hex"
    fi
}

#============================================================================
# 主流程
#============================================================================

echo ""
echo -e "${GREEN}╔══════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║   SlimeVR CH59X 固件编译系统 v0.4.12         ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════╝${NC}"
echo ""
echo "目标: ${TARGET}"
echo "MCU:  ${MCU}"
echo "调试: $([ $DEBUG -eq 1 ] && echo '启用' || echo '禁用')"
echo "合并: $([ $MERGE_BOOT -eq 1 ] && echo '启用' || echo '禁用')"

mkdir -p "${BUILD_DIR}" "${OUTPUT_DIR}"

check_toolchain

case $TARGET in
    bootloader)
        build_bootloader
        ;;
    tracker)
        if [ $MERGE_BOOT -eq 1 ]; then
            build_bootloader
        fi
        build_firmware tracker
        if [ $MERGE_BOOT -eq 1 ]; then
            merge_hex tracker
        fi
        generate_uf2 tracker
        ;;
    receiver)
        if [ $MERGE_BOOT -eq 1 ]; then
            build_bootloader
        fi
        build_firmware receiver
        if [ $MERGE_BOOT -eq 1 ]; then
            merge_hex receiver
        fi
        generate_uf2 receiver
        ;;
    all)
        build_bootloader
        build_firmware tracker
        build_firmware receiver
        merge_hex tracker
        merge_hex receiver
        generate_uf2 tracker
        generate_uf2 receiver
        ;;
    clean)
        clean_all
        exit 0
        ;;
esac

show_results

echo ""
print_success "编译完成!"
