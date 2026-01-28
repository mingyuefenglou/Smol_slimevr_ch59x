#!/bin/bash
#============================================================================
# SlimeVR CH59X 自动化编译系统 v0.5.0
# 
# 功能:
#   - 自动编译 Bootloader
#   - 自动编译 Tracker/Receiver 固件
#   - 自动合并 HEX 文件
#   - 自动生成 UF2 文件
#   - 支持并行编译
#   - 编译产物验证
#
# 用法:
#   ./build_complete.sh [target] [options]
#
# 目标:
#   all         - 编译所有 (默认)
#   tracker     - 仅编译 Tracker (包含 Boot)
#   receiver    - 仅编译 Receiver (包含 Boot)
#   bootloader  - 仅编译 Bootloader
#   clean       - 清理所有
#
# 选项:
#   --no-boot      - 不编译/合并 Bootloader
#   --debug        - 启用调试模式
#   --ota          - 启用 OTA 功能
#   --mcu=CH591    - 指定 MCU 型号 (CH591/CH592)
#   --prefix=xxx   - 指定工具链前缀
#   --parallel     - 并行编译 (同时编译 tracker 和 receiver)
#   --verify       - 编译后验证产物
#   -v, --verbose  - 详细输出
#   -h, --help     - 显示帮助
#
# 示例:
#   ./build_complete.sh all                    # 编译所有
#   ./build_complete.sh tracker --debug       # 调试模式编译 tracker
#   ./build_complete.sh all --mcu=CH591       # 为 CH591 编译
#   ./build_complete.sh all --parallel        # 并行编译
#============================================================================

set -e

#==============================================================================
# 颜色定义
#==============================================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

#==============================================================================
# 默认配置
#==============================================================================
TARGET="all"
MCU="CH592"
DEBUG=0
OTA=0
MERGE_BOOT=1
PARALLEL=0
VERIFY=0
VERBOSE=0
PREFIX="riscv-none-elf-"

# 目录配置
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
OUTPUT_DIR="${SCRIPT_DIR}/output"
BOOT_DIR="${SCRIPT_DIR}/bootloader"
TOOLS_DIR="${SCRIPT_DIR}/tools"

# 版本信息
VERSION="0.5.0"
BUILD_DATE=$(date '+%Y%m%d')

#==============================================================================
# 辅助函数
#==============================================================================

print_header() {
    echo ""
    echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
    echo -e "${BLUE}  $1${NC}"
    echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
}

print_subheader() {
    echo -e "${CYAN}───────────────────────────────────────${NC}"
    echo -e "${CYAN}  $1${NC}"
    echo -e "${CYAN}───────────────────────────────────────${NC}"
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

print_info() {
    echo -e "${MAGENTA}ℹ $1${NC}"
}

print_verbose() {
    if [ $VERBOSE -eq 1 ]; then
        echo -e "${NC}  $1${NC}"
    fi
}

show_help() {
    head -45 "$0" | tail -40
    exit 0
}

show_banner() {
    echo ""
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                                                               ║${NC}"
    echo -e "${GREEN}║   ███████╗██╗     ██╗███╗   ███╗███████╗██╗   ██╗██████╗     ║${NC}"
    echo -e "${GREEN}║   ██╔════╝██║     ██║████╗ ████║██╔════╝██║   ██║██╔══██╗    ║${NC}"
    echo -e "${GREEN}║   ███████╗██║     ██║██╔████╔██║█████╗  ██║   ██║██████╔╝    ║${NC}"
    echo -e "${GREEN}║   ╚════██║██║     ██║██║╚██╔╝██║██╔══╝  ╚██╗ ██╔╝██╔══██╗    ║${NC}"
    echo -e "${GREEN}║   ███████║███████╗██║██║ ╚═╝ ██║███████╗ ╚████╔╝ ██║  ██║    ║${NC}"
    echo -e "${GREEN}║   ╚══════╝╚══════╝╚═╝╚═╝     ╚═╝╚══════╝  ╚═══╝  ╚═╝  ╚═╝    ║${NC}"
    echo -e "${GREEN}║                                                               ║${NC}"
    echo -e "${GREEN}║   CH59X 私有 RF 固件编译系统 v${VERSION}                      ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

#==============================================================================
# 参数解析
#==============================================================================

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
        --ota)
            OTA=1
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
        --parallel)
            PARALLEL=1
            shift
            ;;
        --verify)
            VERIFY=1
            shift
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            show_help
            ;;
        *)
            print_error "未知选项: $1"
            echo "使用 --help 查看帮助"
            exit 1
            ;;
    esac
done

#==============================================================================
# 环境检查
#==============================================================================

check_toolchain() {
    print_subheader "检查工具链"
    
    # 检查编译器
    if ! command -v ${PREFIX}gcc &> /dev/null; then
        print_error "找不到 ${PREFIX}gcc"
        echo ""
        echo "请安装 RISC-V 工具链:"
        echo "  Ubuntu/Debian: sudo apt install gcc-riscv64-unknown-elf"
        echo "  或下载 xPack GNU RISC-V: https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack"
        echo ""
        echo "或指定自定义前缀: --prefix=riscv64-unknown-elf-"
        exit 1
    fi
    
    local GCC_VERSION=$(${PREFIX}gcc --version | head -1)
    print_success "编译器: $GCC_VERSION"
    
    # 检查其他工具
    local tools=("${PREFIX}objcopy" "${PREFIX}size")
    for tool in "${tools[@]}"; do
        if ! command -v $tool &> /dev/null; then
            print_error "找不到 $tool"
            exit 1
        fi
    done
    print_success "工具链完整"
    
    # 检查 Python
    if command -v python3 &> /dev/null; then
        PYTHON="python3"
    elif command -v python &> /dev/null; then
        PYTHON="python"
    else
        print_warning "未找到 Python，某些功能可能不可用"
        PYTHON=""
    fi
    
    if [ -n "$PYTHON" ]; then
        print_success "Python: $($PYTHON --version)"
    fi
}

#==============================================================================
# 编译 Bootloader
#==============================================================================

build_bootloader() {
    print_header "编译 Bootloader"
    
    if [ ! -d "${BOOT_DIR}" ]; then
        print_error "Bootloader 目录不存在: ${BOOT_DIR}"
        return 1
    fi
    
    cd "${BOOT_DIR}"
    
    if [ ! -f "Makefile" ]; then
        print_warning "Bootloader Makefile 不存在，跳过"
        cd "${SCRIPT_DIR}"
        return 0
    fi
    
    # 清理
    make clean 2>/dev/null || true
    
    # 编译
    local MAKE_FLAGS="PREFIX=${PREFIX} MCU=${MCU}"
    
    if [ $VERBOSE -eq 1 ]; then
        make $MAKE_FLAGS
    else
        make $MAKE_FLAGS 2>&1 | tail -10
    fi
    
    # 检查输出
    local BOOT_BIN="output/bootloader_${MCU,,}.bin"
    if [ -f "$BOOT_BIN" ]; then
        local SIZE=$(stat -c%s "$BOOT_BIN" 2>/dev/null || stat -f%z "$BOOT_BIN" 2>/dev/null)
        
        if [ "$SIZE" -gt 4096 ]; then
            print_error "Bootloader 超过 4KB 限制 ($SIZE bytes)"
            cd "${SCRIPT_DIR}"
            return 1
        fi
        
        print_success "Bootloader: $SIZE bytes (限制 4096 bytes)"
        
        # 显示利用率
        local UTIL=$((SIZE * 100 / 4096))
        echo -e "  空间利用率: ${CYAN}${UTIL}%${NC}"
    else
        print_error "Bootloader 编译失败"
        cd "${SCRIPT_DIR}"
        return 1
    fi
    
    cd "${SCRIPT_DIR}"
}

#==============================================================================
# 编译固件
#==============================================================================

build_firmware() {
    local target=$1
    print_header "编译 ${target^} 固件"
    
    cd "${SCRIPT_DIR}"
    
    # 构建 make 参数
    local MAKE_FLAGS="TARGET=${target} MCU=${MCU} PREFIX=${PREFIX}"
    
    if [ $DEBUG -eq 1 ]; then
        MAKE_FLAGS="$MAKE_FLAGS DEBUG=1"
    fi
    
    if [ $OTA -eq 1 ]; then
        MAKE_FLAGS="$MAKE_FLAGS OTA=1"
    fi
    
    # 清理
    make $MAKE_FLAGS clean 2>/dev/null || true
    
    # 编译
    local START_TIME=$(date +%s)
    
    if [ $VERBOSE -eq 1 ]; then
        make $MAKE_FLAGS
    else
        make $MAKE_FLAGS 2>&1 | grep -E "(Compiling|Linking|Size|Error|warning:)" || true
    fi
    
    local END_TIME=$(date +%s)
    local BUILD_TIME=$((END_TIME - START_TIME))
    
    # 检查输出
    local BIN_FILE="${OUTPUT_DIR}/${target}_${MCU,,}.bin"
    local ELF_FILE="${BUILD_DIR}/${target}/${target}_${MCU,,}.elf"
    
    if [ -f "$BIN_FILE" ]; then
        local SIZE=$(stat -c%s "$BIN_FILE" 2>/dev/null || stat -f%z "$BIN_FILE" 2>/dev/null)
        print_success "${target^}: $SIZE bytes (${BUILD_TIME}s)"
        
        # 显示内存使用
        if [ -f "$ELF_FILE" ]; then
            echo ""
            ${PREFIX}size "$ELF_FILE" 2>/dev/null || true
        fi
    else
        print_error "${target^} 编译失败"
        return 1
    fi
}

#==============================================================================
# 合并 HEX 文件
#==============================================================================

merge_hex_files() {
    local target=$1
    print_subheader "合并 ${target^} HEX"
    
    local BOOT_HEX="${BOOT_DIR}/output/bootloader_${MCU,,}.hex"
    local APP_HEX="${OUTPUT_DIR}/${target}_${MCU,,}.hex"
    local COMBINED_HEX="${OUTPUT_DIR}/${target}_${MCU,,}_combined.hex"
    
    # 检查文件
    if [ ! -f "$BOOT_HEX" ]; then
        print_warning "Bootloader HEX 不存在，跳过合并"
        return 0
    fi
    
    if [ ! -f "$APP_HEX" ]; then
        print_error "Application HEX 不存在: $APP_HEX"
        return 1
    fi
    
    # 合并方法
    if [ -n "$PYTHON" ] && [ -f "${TOOLS_DIR}/merge_hex.py" ]; then
        # 使用 Python 脚本 (更可靠)
        $PYTHON "${TOOLS_DIR}/merge_hex.py" "$BOOT_HEX" "$APP_HEX" -o "$COMBINED_HEX"
    else
        # 简单合并
        head -n -1 "$BOOT_HEX" > "$COMBINED_HEX"
        cat "$APP_HEX" >> "$COMBINED_HEX"
    fi
    
    if [ -f "$COMBINED_HEX" ]; then
        local SIZE=$(stat -c%s "$COMBINED_HEX" 2>/dev/null || stat -f%z "$COMBINED_HEX" 2>/dev/null)
        print_success "合并完成: ${target}_${MCU,,}_combined.hex ($SIZE bytes)"
    else
        print_error "合并失败"
        return 1
    fi
}

#==============================================================================
# 生成 UF2
#==============================================================================

generate_uf2() {
    local target=$1
    
    if [ -z "$PYTHON" ] || [ ! -f "${TOOLS_DIR}/bin2uf2.py" ]; then
        print_verbose "跳过 UF2 生成 (缺少工具)"
        return 0
    fi
    
    local BIN_FILE="${OUTPUT_DIR}/${target}_${MCU,,}.bin"
    local UF2_FILE="${OUTPUT_DIR}/${target}_${MCU,,}.uf2"
    
    if [ -f "$BIN_FILE" ]; then
        # WCH CH59X family ID
        $PYTHON "${TOOLS_DIR}/bin2uf2.py" "$BIN_FILE" "$UF2_FILE" \
            --base 0x1000 --family 0x699b62ec 2>/dev/null
        
        if [ -f "$UF2_FILE" ]; then
            print_success "UF2: ${target}_${MCU,,}.uf2"
        fi
    fi
}

#==============================================================================
# 验证编译产物
#==============================================================================

verify_output() {
    local target=$1
    print_subheader "验证 ${target^}"
    
    local BIN_FILE="${OUTPUT_DIR}/${target}_${MCU,,}.bin"
    local HEX_FILE="${OUTPUT_DIR}/${target}_${MCU,,}_combined.hex"
    
    local ERRORS=0
    
    # 检查文件存在
    if [ ! -f "$BIN_FILE" ]; then
        print_error "BIN 文件不存在"
        ((ERRORS++))
    fi
    
    if [ $MERGE_BOOT -eq 1 ] && [ ! -f "$HEX_FILE" ]; then
        print_error "合并 HEX 文件不存在"
        ((ERRORS++))
    fi
    
    # 检查文件大小
    if [ -f "$BIN_FILE" ]; then
        local SIZE=$(stat -c%s "$BIN_FILE" 2>/dev/null || stat -f%z "$BIN_FILE" 2>/dev/null)
        local MAX_SIZE=$((224 * 1024))  # 224KB
        
        if [ "$SIZE" -gt "$MAX_SIZE" ]; then
            print_error "固件超过最大限制 ($SIZE > $MAX_SIZE bytes)"
            ((ERRORS++))
        else
            print_success "固件大小检查通过"
        fi
    fi
    
    # 检查向量表
    if [ -f "$BIN_FILE" ]; then
        local FIRST_WORD=$(xxd -l 4 -e "$BIN_FILE" 2>/dev/null | awk '{print $2}')
        if [ -n "$FIRST_WORD" ]; then
            print_verbose "向量表首地址: 0x$FIRST_WORD"
        fi
    fi
    
    if [ $ERRORS -eq 0 ]; then
        print_success "${target^} 验证通过"
        return 0
    else
        print_error "${target^} 验证失败 ($ERRORS 个错误)"
        return 1
    fi
}

#==============================================================================
# 清理
#==============================================================================

clean_all() {
    print_header "清理所有构建文件"
    
    # 清理主项目
    rm -rf "${BUILD_DIR}"
    rm -f "${OUTPUT_DIR}"/*.bin "${OUTPUT_DIR}"/*.hex "${OUTPUT_DIR}"/*.uf2 "${OUTPUT_DIR}"/*.map
    
    # 清理 Bootloader
    if [ -d "${BOOT_DIR}" ]; then
        cd "${BOOT_DIR}"
        make clean 2>/dev/null || true
        cd "${SCRIPT_DIR}"
    fi
    
    print_success "清理完成"
}

#==============================================================================
# 显示结果
#==============================================================================

show_results() {
    print_header "编译结果"
    
    echo ""
    echo "输出文件:"
    echo "─────────────────────────────────────────"
    
    # 列出文件
    for file in "${OUTPUT_DIR}"/*.bin "${OUTPUT_DIR}"/*_combined.hex "${OUTPUT_DIR}"/*.uf2; do
        if [ -f "$file" ]; then
            local SIZE=$(stat -c%s "$file" 2>/dev/null || stat -f%z "$file" 2>/dev/null)
            local NAME=$(basename "$file")
            printf "  %-40s %8d bytes\n" "$NAME" "$SIZE"
        fi
    done
    
    echo ""
    echo "烧录命令:"
    echo "─────────────────────────────────────────"
    
    if [ -f "${OUTPUT_DIR}/tracker_${MCU,,}_combined.hex" ]; then
        echo "  Tracker:"
        echo "    wch-link -w ${OUTPUT_DIR}/tracker_${MCU,,}_combined.hex"
    fi
    
    if [ -f "${OUTPUT_DIR}/receiver_${MCU,,}_combined.hex" ]; then
        echo "  Receiver:"
        echo "    wch-link -w ${OUTPUT_DIR}/receiver_${MCU,,}_combined.hex"
    fi
    
    echo ""
    echo "配置信息:"
    echo "─────────────────────────────────────────"
    echo "  MCU:    ${MCU}"
    echo "  Debug:  $([ $DEBUG -eq 1 ] && echo '启用' || echo '禁用')"
    echo "  OTA:    $([ $OTA -eq 1 ] && echo '启用' || echo '禁用')"
    echo "  日期:   ${BUILD_DATE}"
}

#==============================================================================
# 并行编译
#==============================================================================

build_parallel() {
    print_header "并行编译 Tracker 和 Receiver"
    
    # 后台编译 tracker
    build_firmware tracker &
    local PID_TRACKER=$!
    
    # 后台编译 receiver
    build_firmware receiver &
    local PID_RECEIVER=$!
    
    # 等待完成
    local RESULT=0
    
    wait $PID_TRACKER || RESULT=1
    wait $PID_RECEIVER || RESULT=1
    
    if [ $RESULT -ne 0 ]; then
        print_error "并行编译失败"
        return 1
    fi
    
    print_success "并行编译完成"
}

#==============================================================================
# 主流程
#==============================================================================

main() {
    show_banner
    
    echo "配置:"
    echo "  目标:    ${TARGET}"
    echo "  MCU:     ${MCU}"
    echo "  Debug:   $([ $DEBUG -eq 1 ] && echo '启用' || echo '禁用')"
    echo "  OTA:     $([ $OTA -eq 1 ] && echo '启用' || echo '禁用')"
    echo "  合并:    $([ $MERGE_BOOT -eq 1 ] && echo '启用' || echo '禁用')"
    echo "  并行:    $([ $PARALLEL -eq 1 ] && echo '启用' || echo '禁用')"
    
    # 创建目录
    mkdir -p "${BUILD_DIR}" "${OUTPUT_DIR}"
    
    # 检查工具链
    check_toolchain
    
    # 执行编译
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
                merge_hex_files tracker
            fi
            generate_uf2 tracker
            if [ $VERIFY -eq 1 ]; then
                verify_output tracker
            fi
            ;;
            
        receiver)
            if [ $MERGE_BOOT -eq 1 ]; then
                build_bootloader
            fi
            build_firmware receiver
            if [ $MERGE_BOOT -eq 1 ]; then
                merge_hex_files receiver
            fi
            generate_uf2 receiver
            if [ $VERIFY -eq 1 ]; then
                verify_output receiver
            fi
            ;;
            
        all)
            build_bootloader
            
            if [ $PARALLEL -eq 1 ]; then
                build_parallel
            else
                build_firmware tracker
                build_firmware receiver
            fi
            
            merge_hex_files tracker
            merge_hex_files receiver
            generate_uf2 tracker
            generate_uf2 receiver
            
            if [ $VERIFY -eq 1 ]; then
                verify_output tracker
                verify_output receiver
            fi
            ;;
            
        clean)
            clean_all
            exit 0
            ;;
    esac
    
    # 显示结果
    show_results
    
    echo ""
    print_success "编译完成!"
    echo ""
}

# 运行
main
