#!/bin/bash
#============================================================================
# SlimeVR CH592 本地编译脚本
# 支持: 本地工具链 / Docker 编译
#============================================================================

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
OUTPUT_DIR="${PROJECT_DIR}/output"

#============================================================================
# 颜色输出
#============================================================================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

#============================================================================
# 使用说明
#============================================================================
usage() {
    echo "SlimeVR CH592 编译脚本"
    echo ""
    echo "用法: $0 [选项] [目标]"
    echo ""
    echo "目标:"
    echo "  tracker     编译追踪器固件 (默认)"
    echo "  receiver    编译接收器固件"
    echo "  all         编译两者"
    echo "  clean       清理构建目录"
    echo ""
    echo "选项:"
    echo "  --docker    使用 Docker 编译 (推荐)"
    echo "  --local     使用本地工具链"
    echo "  --sdk PATH  指定 CH592 SDK 路径"
    echo "  --help      显示帮助"
    echo ""
    echo "示例:"
    echo "  $0 tracker                    # 本地编译追踪器"
    echo "  $0 --docker all               # Docker 编译全部"
    echo "  $0 --sdk ../ch592 tracker     # 指定 SDK 路径"
}

#============================================================================
# 参数解析
#============================================================================
USE_DOCKER=0
SDK_PATH=""
TARGET="tracker"

while [[ $# -gt 0 ]]; do
    case $1 in
        --docker)   USE_DOCKER=1; shift ;;
        --local)    USE_DOCKER=0; shift ;;
        --sdk)      SDK_PATH="$2"; shift 2 ;;
        --help|-h)  usage; exit 0 ;;
        tracker|receiver|all|clean)  TARGET="$1"; shift ;;
        *)          error "未知参数: $1" ;;
    esac
done

#============================================================================
# Docker 编译
#============================================================================
build_with_docker() {
    local target=$1
    
    info "使用 Docker 编译 ${target}..."
    
    # 检查 Docker
    if ! command -v docker &> /dev/null; then
        error "Docker 未安装。请安装 Docker 或使用 --local 选项"
    fi
    
    # 创建 Dockerfile (如果不存在)
    if [ ! -f "${PROJECT_DIR}/Dockerfile" ]; then
        cat > "${PROJECT_DIR}/Dockerfile" << 'DOCKERFILE'
FROM ubuntu:22.04

# 安装依赖
RUN apt-get update && apt-get install -y \
    build-essential \
    git \
    wget \
    && rm -rf /var/lib/apt/lists/*

# 下载 xPack RISC-V 工具链
RUN mkdir -p /opt/xpack && cd /opt/xpack && \
    wget -q https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases/download/v13.2.0-2/xpack-riscv-none-elf-gcc-13.2.0-2-linux-x64.tar.gz && \
    tar xf xpack-riscv-none-elf-gcc-13.2.0-2-linux-x64.tar.gz && \
    rm xpack-riscv-none-elf-gcc-13.2.0-2-linux-x64.tar.gz

ENV PATH="/opt/xpack/xpack-riscv-none-elf-gcc-13.2.0-2/bin:${PATH}"

WORKDIR /build
DOCKERFILE
        info "创建 Dockerfile"
    fi
    
    # 构建 Docker 镜像
    info "构建 Docker 镜像..."
    docker build -t slimevr-ch592-builder "${PROJECT_DIR}" || error "Docker 镜像构建失败"
    
    # 运行编译
    info "编译 ${target}..."
    docker run --rm \
        -v "${PROJECT_DIR}:/build" \
        -e "CROSS_PREFIX=riscv-none-elf-" \
        slimevr-ch592-builder \
        /build/build.sh "${target}"
    
    info "Docker 编译完成!"
}

#============================================================================
# 本地编译
#============================================================================
build_local() {
    local target=$1
    
    info "本地编译 ${target}..."
    
    # 查找工具链
    TOOLCHAINS=(
        "riscv-none-embed-"
        "riscv-none-elf-"
        "riscv64-unknown-elf-"
        "riscv32-unknown-elf-"
    )
    
    CROSS_PREFIX=""
    for tc in "${TOOLCHAINS[@]}"; do
        if command -v "${tc}gcc" &> /dev/null; then
            CROSS_PREFIX="$tc"
            break
        fi
    done
    
    if [ -z "$CROSS_PREFIX" ]; then
        error "未找到 RISC-V 工具链!
        
请安装以下之一:
  1. MounRiver Studio: https://www.mounriver.com/
  2. xPack RISC-V GCC: https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack
  3. Ubuntu: sudo apt install gcc-riscv64-unknown-elf

或使用 --docker 选项进行 Docker 编译"
    fi
    
    info "使用工具链: ${CROSS_PREFIX}gcc"
    
    # 检查 SDK
    if [ -z "$SDK_PATH" ]; then
        # 尝试自动查找
        for path in "../ch592" "../ch592_sdk" "../../ch592" "/opt/ch592"; do
            if [ -d "$path/EVT" ] || [ -d "$path/SDK" ]; then
                SDK_PATH="$path"
                break
            fi
        done
    fi
    
    if [ -n "$SDK_PATH" ] && [ -d "$SDK_PATH" ]; then
        info "找到 SDK: $SDK_PATH"
        info "运行 SDK 集成脚本..."
        "${PROJECT_DIR}/setup_sdk.sh" "$SDK_PATH"
    else
        warn "未找到 CH592 SDK，使用内置桩文件"
    fi
    
    # 编译
    export CROSS_PREFIX
    "${PROJECT_DIR}/build.sh" "$target"
}

#============================================================================
# 清理
#============================================================================
clean_build() {
    info "清理构建目录..."
    rm -rf "${BUILD_DIR}"
    rm -rf "${OUTPUT_DIR}"/*.{bin,hex,elf,map}
    info "清理完成"
}

#============================================================================
# 主流程
#============================================================================
mkdir -p "${OUTPUT_DIR}"

case $TARGET in
    clean)
        clean_build
        ;;
    all)
        if [ $USE_DOCKER -eq 1 ]; then
            build_with_docker tracker
            build_with_docker receiver
        else
            build_local tracker
            build_local receiver
        fi
        ;;
    tracker|receiver)
        if [ $USE_DOCKER -eq 1 ]; then
            build_with_docker "$TARGET"
        else
            build_local "$TARGET"
        fi
        ;;
esac

# 显示结果
if [ -f "${OUTPUT_DIR}/tracker.bin" ] || [ -f "${OUTPUT_DIR}/receiver.bin" ]; then
    echo ""
    echo "=============================================="
    echo "  编译成功!"
    echo "=============================================="
    echo ""
    echo "输出文件:"
    ls -lh "${OUTPUT_DIR}"/*.{bin,hex} 2>/dev/null || true
    echo ""
    echo "烧录命令:"
    echo "  wchisp flash output/tracker.bin"
    echo "  或使用 WCHISPTool (Windows)"
fi
