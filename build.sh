#!/bin/bash
#============================================================================
# SlimeVR CH59X 主构建脚本
# 
# 注意: 此脚本已废弃，请直接使用 make 命令
# 推荐使用: make TARGET=tracker BOARD=ch591d
#
# 用法: ./build.sh [target] [board]
#============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 检查 build_complete.sh 是否存在
if [ -f "${SCRIPT_DIR}/scripts/build/build_complete.sh" ]; then
    echo "注意: 使用 build_complete.sh (高级功能)"
    exec "${SCRIPT_DIR}/scripts/build/build_complete.sh" "$@"
else
    echo "=============================================="
    echo "SlimeVR CH59X 构建脚本"
    echo "=============================================="
    echo ""
    echo "注意: build_complete.sh 未找到，使用 make 命令"
    echo ""
    echo "推荐用法:"
    echo "  make TARGET=tracker BOARD=ch591d"
    echo "  make TARGET=tracker BOARD=ch592x"
    echo "  make TARGET=receiver BOARD=ch592x"
    echo ""
    echo "支持的板子:"
    echo "  - ch591d: CH591D 板子"
    echo "  - ch592x: CH592X 板子"
    echo "  - generic_board: 通用板子（需配置引脚）"
    echo ""
    
    # 简单的参数转换
    TARGET="${1:-tracker}"
    BOARD="${2:-ch591d}"
    
    echo "编译: TARGET=$TARGET BOARD=$BOARD"
    echo ""
    
    make TARGET="$TARGET" BOARD="$BOARD"
fi
