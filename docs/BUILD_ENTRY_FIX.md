# 构建入口修复说明

## 问题描述

发现构建入口存在不一致：
- README 建议使用 `./build.sh ...`
- `build.sh` 依赖 `scripts/build/build_complete.sh`
- 该文件可能在某些环境中缺失或不可用
- 应改用 `make TARGET=tracker/receiver` 的标准路径

## 修复内容

### 1. 更新 build.sh

`build.sh` 现在会：
- 优先尝试使用 `build_complete.sh`（如果存在）
- 如果不存在，自动回退到 `make` 命令
- 显示清晰的错误信息和推荐用法

### 2. 更新所有文档

所有文档中的构建命令已更新为使用 `make` 命令：

**旧方式（已废弃）:**
```bash
./build.sh tracker
./build.sh receiver
./build.sh all
```

**新方式（推荐）:**
```bash
make TARGET=tracker BOARD=ch591d
make TARGET=receiver BOARD=ch592x
```

### 3. 标准构建命令

#### 基本用法

```bash
# 编译 Tracker (默认 CH591D 板子)
make TARGET=tracker

# 编译 Receiver (默认 CH591D 板子)
make TARGET=receiver
```

#### 指定板子类型

```bash
# CH591D 板子
make TARGET=tracker BOARD=ch591d

# CH592X 板子
make TARGET=tracker BOARD=ch592x
make TARGET=receiver BOARD=ch592x

# 通用板子（需配置引脚）
make TARGET=tracker BOARD=generic_board
```

#### 清理构建文件

```bash
make clean
```

## 更新的文档

以下文档已更新：
- `README.md` - 主 README 文件
- `docs/BUILD.md` - 构建系统说明
- `docs/BUILD_GUIDE.md` - 编译指南
- `docs/BOARD_CONFIG.md` - 板子配置说明
- `docs/QUICK_START_BOARD.md` - 快速开始指南
- 其他相关文档

## 向后兼容

`build.sh` 仍然可用，但会：
1. 优先尝试使用 `build_complete.sh`（如果存在）
2. 如果不存在，自动使用 `make` 命令
3. 显示清晰的提示信息

## 推荐用法

**推荐**: 直接使用 `make` 命令，更简单、更可靠：

```bash
make TARGET=tracker BOARD=ch591d
```

**备选**: 如果 `build_complete.sh` 存在且需要高级功能，可以使用：

```bash
./build.sh tracker
```

## 注意事项

1. `make` 命令是标准构建方式，所有环境都支持
2. `build.sh` 是便利脚本，依赖 `build_complete.sh`
3. 如果 `build_complete.sh` 缺失，`build.sh` 会自动回退到 `make`
4. 建议在文档和脚本中使用 `make` 命令作为主要构建方式

