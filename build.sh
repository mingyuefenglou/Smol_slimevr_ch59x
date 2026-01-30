#!/bin/bash
#============================================================================
# SlimeVR CH59X 主构建脚本
# 这是推荐的构建入口，内部调用 build_complete.sh
#
# 用法: ./build.sh [target] [options]
#============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# 如果 build_complete.sh 存在，使用它
if [ -f "${SCRIPT_DIR}/scripts/build/build_complete.sh" ]; then
    exec "${SCRIPT_DIR}/scripts/build/build_complete.sh" "$@"
fi

TARGET="${1:-tracker}"
case "${TARGET}" in
    tracker|receiver)
        exec make TARGET="${TARGET}"
        ;;
    all)
        exec make all
        ;;
    clean)
        exec make clean
        ;;
    *)
        echo "错误: 未找到 build_complete.sh，且未知目标: ${TARGET}"
        echo "可用目标: tracker, receiver, all, clean"
        echo "或直接使用: make TARGET=tracker"
        exit 1
        ;;
esac
