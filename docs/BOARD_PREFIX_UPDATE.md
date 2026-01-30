# 板子配置前缀更新说明

## 更新日期
2026-01-30

## 更新原因

为避免与后续板子配置冲突，将 `mingyue_slimevr` 系列下的板子配置添加 `mingyue_` 前缀。

## 更新内容

### 1. 目录重命名

- `board/mingyue_slimevr/ch591d/` → `board/mingyue_slimevr/mingyue_ch591d/`
- `board/mingyue_slimevr/ch592x/` → `board/mingyue_slimevr/mingyue_ch592x/`

### 2. 代码更新

#### board.h
- 添加 `BOARD_MINGYUE_CH591D` 和 `BOARD_MINGYUE_CH592X` 支持
- 保留 `BOARD_CH591D` 和 `BOARD_CH592X` 向后兼容
- 更新包含路径为带前缀的版本

#### Makefile
- 添加 `BOARD=mingyue_ch591d` 和 `BOARD=mingyue_ch592x` 支持
- 保留 `BOARD=ch591d` 和 `BOARD=ch592x` 向后兼容（自动转换）
- 默认板子改为 `mingyue_ch591d`

### 3. 文档更新

所有文档中的板子名称和路径已更新：
- `board/README.md`
- `board/mingyue_slimevr/README.md`
- `docs/BOARD_CONFIG.md`
- `docs/QUICK_START_BOARD.md`
- `docs/BOARD_SETUP_COMPLETE.md`
- `docs/BUILD_GUIDE.md`
- `docs/BUILD.md`
- `docs/BUILD_SCRIPT_USAGE.md`
- `README.md`

## 新的目录结构

```
board/
├── board.h
├── generic_board/              # Generic board (Tracker)
├── generic_receiver/            # Generic receiver
└── mingyue_slimevr/            # Mingyue SlimeVR board series
    ├── mingyue_ch591d/         # Mingyue CH591D (带前缀)
    │   ├── config.h
    │   └── pins.h
    └── mingyue_ch592x/         # Mingyue CH592X (带前缀)
        ├── config.h
        └── pins.h
```

## 使用方法

### 推荐用法（新）

```bash
# Mingyue CH591D
make TARGET=tracker BOARD=mingyue_ch591d

# Mingyue CH592X
make TARGET=tracker BOARD=mingyue_ch592x
make TARGET=receiver BOARD=mingyue_ch592x
```

### 向后兼容（旧命令仍然可用）

```bash
# 旧命令会自动转换为新命令
make TARGET=tracker BOARD=ch591d  # → mingyue_ch591d
make TARGET=tracker BOARD=ch592x  # → mingyue_ch592x
```

## 优势

1. **避免冲突**: 带前缀的命名避免与其他厂商的板子配置冲突
2. **向后兼容**: 旧命令仍然可用，自动转换
3. **清晰标识**: 明确标识板子属于哪个系列
4. **易于扩展**: 后续可以添加其他厂商的板子（如 `other_ch591d`）

## 支持的板子

### 当前支持的板子

| 板子名称 | 编译命令 | 状态 |
|---------|---------|------|
| Generic Receiver | `BOARD=generic_receiver` | ✅ |
| Generic Board | `BOARD=generic_board` | ✅ |
| Mingyue CH591D | `BOARD=mingyue_ch591d` | ✅ |
| Mingyue CH592X | `BOARD=mingyue_ch592x` | ✅ |
| CH591D (兼容) | `BOARD=ch591d` | ✅ 自动转换 |
| CH592X (兼容) | `BOARD=ch592x` | ✅ 自动转换 |

## 添加新板子

### 在 mingyue_slimevr 系列中添加

新板子必须使用 `mingyue_` 前缀：

```bash
# 创建新板子目录
mkdir -p board/mingyue_slimevr/mingyue_myboard

# 创建配置文件
# board/mingyue_slimevr/mingyue_myboard/config.h
# board/mingyue_slimevr/mingyue_myboard/pins.h

# 更新 board.h 和 Makefile
```

### 添加其他厂商的板子

其他厂商的板子可以使用自己的前缀：

```bash
# 例如：其他厂商的 CH591D 板子
mkdir -p board/other_series/other_ch591d

# 在 board.h 中添加：
# #elif defined(BOARD_OTHER_CH591D)
#     #include "other_series/other_ch591d/config.h"
```

## 验证

使用测试脚本验证配置：

```bash
# 测试新命令
bash scripts/test/check_board_config.sh mingyue_ch591d
bash scripts/test/check_board_config.sh mingyue_ch592x

# 测试向后兼容
bash scripts/test/check_board_config.sh ch591d
bash scripts/test/check_board_config.sh ch592x
```

## 注意事项

1. **推荐使用新命令**: 虽然旧命令仍然可用，但推荐使用带前缀的新命令
2. **前缀规则**: mingyue_slimevr 系列下的所有板子都应使用 `mingyue_` 前缀
3. **其他系列**: 其他厂商的板子应使用自己的前缀，避免冲突

