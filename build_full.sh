#!/bin/bash
#==============================================================================
# SlimeVR CH59X 完整构建脚本
# SlimeVR CH59X Full Build Script
#
# 功能:
# 1. 编译 Bootloader (4KB)
# 2. 编译 Tracker 固件
# 3. 编译 Receiver 固件
# 4. 合并 HEX 文件 (Bootloader + Application)
# 5. 生成 UF2 文件
#
# 用法: ./build_full.sh [options]
#   -t, --target    指定目标 (all/tracker/receiver/bootloader)
#   -c, --chip      指定芯片 (CH591/CH592)
#   -v, --version   指定版本号
#   --clean         清理后编译
#   --no-merge      不合并HEX
#   --verbose       详细输出
#   -h, --help      显示帮助
#
# 示例:
#   ./build_full.sh                     # 编译全部
#   ./build_full.sh -t tracker          # 仅编译 Tracker
#   ./build_full.sh -t receiver -c CH591  # CH591 Receiver
#   ./build_full.sh --clean             # 清理后编译
#==============================================================================

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 默认配置
TARGET="all"
CHIP="CH591"
VERSION=""
CLEAN=false
MERGE=true
VERBOSE=false

# 目录
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/output"
BOOTLOADER_DIR="${SCRIPT_DIR}/bootloader"
TOOLS_DIR="${SCRIPT_DIR}/tools"

# 版本信息
if [ -z "$VERSION" ]; then
    if [ -f "${SCRIPT_DIR}/VERSION" ]; then
        VERSION=$(cat "${SCRIPT_DIR}/VERSION" | head -1 | tr -d '\n')
    else
        VERSION="0.4.13"
    fi
fi

#==============================================================================
# 辅助函数
#==============================================================================

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[OK]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

show_help() {
    echo "SlimeVR CH59X 完整构建脚本"
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  -t, --target TARGET   指定目标 (all/tracker/receiver/bootloader)"
    echo "  -c, --chip CHIP       指定芯片 (CH591/CH592，默认 CH591)"
    echo "  -v, --version VER     指定版本号"
    echo "  --clean               清理后编译"
    echo "  --no-merge            不合并 Bootloader 到 HEX"
    echo "  --verbose             详细输出"
    echo "  -h, --help            显示帮助"
    echo ""
    echo "示例:"
    echo "  $0                    编译全部 (Bootloader + Tracker + Receiver)"
    echo "  $0 -t tracker         仅编译 Tracker"
    echo "  $0 -c CH591           使用 CH591 芯片"
    echo "  $0 --clean            清理后重新编译"
}

check_toolchain() {
    if ! command -v riscv-none-elf-gcc &> /dev/null; then
        if ! command -v riscv-none-embed-gcc &> /dev/null; then
            log_error "未找到 RISC-V 工具链"
            log_info "请安装 riscv-none-elf-gcc 或 riscv-none-embed-gcc"
            exit 1
        else
            export CROSS_COMPILE=riscv-none-embed-
        fi
    else
        export CROSS_COMPILE=riscv-none-elf-
    fi
    log_success "工具链: ${CROSS_COMPILE}gcc"
}

#==============================================================================
# 解析参数
#==============================================================================

while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--target)
            TARGET="$2"
            shift 2
            ;;
        -c|--chip)
            CHIP="$2"
            shift 2
            ;;
        -v|--version)
            VERSION="$2"
            shift 2
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        --no-merge)
            MERGE=false
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            log_error "未知参数: $1"
            show_help
            exit 1
            ;;
    esac
done

#==============================================================================
# 编译 Bootloader
#==============================================================================

build_bootloader() {
    log_info "===== 编译 Bootloader ====="
    
    cd "$BOOTLOADER_DIR"
    
    if [ "$CLEAN" = true ]; then
        make clean || true
    fi
    
    if [ "$VERBOSE" = true ]; then
        make CHIP=$CHIP V=1
    else
        make CHIP=$CHIP
    fi
    
    # 检查输出
    if [ -f "output/bootloader_${CHIP,,}.hex" ]; then
        log_success "Bootloader 编译成功"
        BOOT_HEX="${BOOTLOADER_DIR}/output/bootloader_${CHIP,,}.hex"
        
        # 检查大小
        BOOT_SIZE=$(stat -f%z "output/bootloader_${CHIP,,}.bin" 2>/dev/null || stat -c%s "output/bootloader_${CHIP,,}.bin")
        if [ "$BOOT_SIZE" -gt 4096 ]; then
            log_error "Bootloader 大小超过 4KB 限制: ${BOOT_SIZE} bytes"
            exit 1
        fi
        log_info "Bootloader 大小: ${BOOT_SIZE} bytes (限制: 4096)"
    else
        log_error "Bootloader 编译失败"
        exit 1
    fi
    
    cd "$SCRIPT_DIR"
}

#==============================================================================
# 编译 Tracker
#==============================================================================

build_tracker() {
    log_info "===== 编译 Tracker ====="
    
    if [ "$CLEAN" = true ]; then
        make clean TARGET=tracker || true
    fi
    
    if [ "$VERBOSE" = true ]; then
        make TARGET=tracker CHIP=$CHIP VERSION=$VERSION V=1
    else
        make TARGET=tracker CHIP=$CHIP VERSION=$VERSION
    fi
    
    if [ -f "${OUTPUT_DIR}/tracker_${CHIP}_v${VERSION}.hex" ]; then
        log_success "Tracker 编译成功"
        TRACKER_HEX="${OUTPUT_DIR}/tracker_${CHIP}_v${VERSION}.hex"
    else
        log_error "Tracker 编译失败"
        exit 1
    fi
}

#==============================================================================
# 编译 Receiver
#==============================================================================

build_receiver() {
    log_info "===== 编译 Receiver ====="
    
    if [ "$CLEAN" = true ]; then
        make clean TARGET=receiver || true
    fi
    
    if [ "$VERBOSE" = true ]; then
        make TARGET=receiver CHIP=$CHIP VERSION=$VERSION V=1
    else
        make TARGET=receiver CHIP=$CHIP VERSION=$VERSION
    fi
    
    if [ -f "${OUTPUT_DIR}/receiver_${CHIP}_v${VERSION}.hex" ]; then
        log_success "Receiver 编译成功"
        RECEIVER_HEX="${OUTPUT_DIR}/receiver_${CHIP}_v${VERSION}.hex"
    else
        log_error "Receiver 编译失败"
        exit 1
    fi
}

#==============================================================================
# 合并 HEX 文件
#==============================================================================

merge_hex_files() {
    if [ "$MERGE" = false ]; then
        log_warn "跳过 HEX 合并"
        return
    fi
    
    log_info "===== 合并 HEX 文件 ====="
    
    # 检查合并工具
    MERGE_TOOL="${TOOLS_DIR}/merge_hex.py"
    if [ ! -f "$MERGE_TOOL" ]; then
        log_warn "未找到合并工具，尝试使用 srec_cat"
        
        if command -v srec_cat &> /dev/null; then
            USE_SREC=true
        else
            log_warn "srec_cat 不可用，跳过合并"
            return
        fi
    fi
    
    # 合并 Tracker
    if [ -n "$TRACKER_HEX" ] && [ -n "$BOOT_HEX" ]; then
        TRACKER_COMBINED="${OUTPUT_DIR}/tracker_${CHIP}_v${VERSION}_combined.hex"
        
        if [ "$USE_SREC" = true ]; then
            srec_cat "$BOOT_HEX" -Intel "$TRACKER_HEX" -Intel -o "$TRACKER_COMBINED" -Intel
        else
            python3 "$MERGE_TOOL" "$BOOT_HEX" "$TRACKER_HEX" -o "$TRACKER_COMBINED"
        fi
        
        if [ -f "$TRACKER_COMBINED" ]; then
            log_success "Tracker 合并完成: $TRACKER_COMBINED"
        fi
    fi
    
    # 合并 Receiver
    if [ -n "$RECEIVER_HEX" ] && [ -n "$BOOT_HEX" ]; then
        RECEIVER_COMBINED="${OUTPUT_DIR}/receiver_${CHIP}_v${VERSION}_combined.hex"
        
        if [ "$USE_SREC" = true ]; then
            srec_cat "$BOOT_HEX" -Intel "$RECEIVER_HEX" -Intel -o "$RECEIVER_COMBINED" -Intel
        else
            python3 "$MERGE_TOOL" "$BOOT_HEX" "$RECEIVER_HEX" -o "$RECEIVER_COMBINED"
        fi
        
        if [ -f "$RECEIVER_COMBINED" ]; then
            log_success "Receiver 合并完成: $RECEIVER_COMBINED"
        fi
    fi
}

#==============================================================================
# 生成 UF2 文件
#==============================================================================

generate_uf2() {
    log_info "===== 生成 UF2 文件 ====="
    
    UF2_TOOL="${TOOLS_DIR}/bin2uf2.py"
    if [ ! -f "$UF2_TOOL" ]; then
        log_warn "未找到 UF2 工具，跳过 UF2 生成"
        return
    fi
    
    # Tracker UF2
    if [ -f "${OUTPUT_DIR}/tracker_${CHIP}_v${VERSION}.bin" ]; then
        python3 "$UF2_TOOL" \
            "${OUTPUT_DIR}/tracker_${CHIP}_v${VERSION}.bin" \
            "${OUTPUT_DIR}/tracker_${CHIP}_v${VERSION}.uf2" \
            0x4b50e11a 0x00001000
        log_success "Tracker UF2 生成完成"
    fi
    
    # Receiver UF2
    if [ -f "${OUTPUT_DIR}/receiver_${CHIP}_v${VERSION}.bin" ]; then
        python3 "$UF2_TOOL" \
            "${OUTPUT_DIR}/receiver_${CHIP}_v${VERSION}.bin" \
            "${OUTPUT_DIR}/receiver_${CHIP}_v${VERSION}.uf2" \
            0x4b50e11a 0x00001000
        log_success "Receiver UF2 生成完成"
    fi
}

#==============================================================================
# 构建摘要
#==============================================================================

show_summary() {
    echo ""
    echo "=============================================="
    echo -e "${GREEN}构建完成${NC}"
    echo "=============================================="
    echo "版本: v${VERSION}"
    echo "芯片: ${CHIP}"
    echo ""
    echo "输出文件:"
    
    if [ -f "${BOOTLOADER_DIR}/output/bootloader_${CHIP,,}.hex" ]; then
        SIZE=$(stat -f%z "${BOOTLOADER_DIR}/output/bootloader_${CHIP,,}.bin" 2>/dev/null || stat -c%s "${BOOTLOADER_DIR}/output/bootloader_${CHIP,,}.bin")
        echo "  Bootloader: bootloader_${CHIP,,}.hex (${SIZE} bytes)"
    fi
    
    if [ -f "${OUTPUT_DIR}/tracker_${CHIP}_v${VERSION}.hex" ]; then
        SIZE=$(stat -f%z "${OUTPUT_DIR}/tracker_${CHIP}_v${VERSION}.bin" 2>/dev/null || stat -c%s "${OUTPUT_DIR}/tracker_${CHIP}_v${VERSION}.bin" 2>/dev/null || echo "N/A")
        echo "  Tracker:    tracker_${CHIP}_v${VERSION}.hex"
        if [ -f "${OUTPUT_DIR}/tracker_${CHIP}_v${VERSION}_combined.hex" ]; then
            echo "              tracker_${CHIP}_v${VERSION}_combined.hex (含 Bootloader)"
        fi
    fi
    
    if [ -f "${OUTPUT_DIR}/receiver_${CHIP}_v${VERSION}.hex" ]; then
        SIZE=$(stat -f%z "${OUTPUT_DIR}/receiver_${CHIP}_v${VERSION}.bin" 2>/dev/null || stat -c%s "${OUTPUT_DIR}/receiver_${CHIP}_v${VERSION}.bin" 2>/dev/null || echo "N/A")
        echo "  Receiver:   receiver_${CHIP}_v${VERSION}.hex"
        if [ -f "${OUTPUT_DIR}/receiver_${CHIP}_v${VERSION}_combined.hex" ]; then
            echo "              receiver_${CHIP}_v${VERSION}_combined.hex (含 Bootloader)"
        fi
    fi
    
    echo ""
    echo "烧录命令:"
    echo "  Tracker:  wchisp flash ${OUTPUT_DIR}/tracker_${CHIP}_v${VERSION}_combined.hex"
    echo "  Receiver: wchisp flash ${OUTPUT_DIR}/receiver_${CHIP}_v${VERSION}_combined.hex"
    echo "=============================================="
}

#==============================================================================
# 主流程
#==============================================================================

main() {
    echo "=============================================="
    echo "SlimeVR CH59X 完整构建脚本"
    echo "版本: v${VERSION} | 芯片: ${CHIP} | 目标: ${TARGET}"
    echo "=============================================="
    
    # 检查工具链
    check_toolchain
    
    # 创建输出目录
    mkdir -p "$OUTPUT_DIR"
    
    # 根据目标执行构建
    case $TARGET in
        all)
            build_bootloader
            build_tracker
            build_receiver
            merge_hex_files
            generate_uf2
            ;;
        tracker)
            build_bootloader
            build_tracker
            merge_hex_files
            generate_uf2
            ;;
        receiver)
            build_bootloader
            build_receiver
            merge_hex_files
            generate_uf2
            ;;
        bootloader)
            build_bootloader
            ;;
        *)
            log_error "未知目标: $TARGET"
            exit 1
            ;;
    esac
    
    show_summary
}

# 执行
main
