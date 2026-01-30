# 板子系列整理完成

## 更新完成时间
2026-01-30

## 新的目录结构

板子配置已按系列组织，所有板子现在位于 `mingyue_slimevr` 系列下：

```
board/
├── board.h                      # 板子配置统一入口
├── README.md                    # 板子配置说明
└── mingyue_slimevr/            # 明月 SlimeVR 板子系列
    ├── README.md               # 系列说明
    ├── ch591d/                 # CH591D 板子配置
    │   ├── config.h           # 主配置文件
    │   └── pins.h             # 引脚定义
    └── ch592x/                 # CH592X 板子配置
        ├── config.h           # 主配置文件
        └── pins.h             # 引脚定义
```

## 更新内容

### 1. 目录重组
- 将 `board/ch591d/` 移动到 `board/mingyue_slimevr/ch591d/`
- 将 `board/ch592x/` 移动到 `board/mingyue_slimevr/ch592x/`
- 创建了 `board/mingyue_slimevr/README.md` 系列说明文档

### 2. 代码更新
- 更新了 `board/board.h` 中的包含路径
- 添加了 `BOARD_SERIES` 宏定义
- 所有路径已更新为新的系列结构

### 3. 文档更新
- 更新了 `board/README.md`
- 更新了 `docs/BOARD_CONFIG.md`
- 更新了 `docs/BOARD_SETUP_COMPLETE.md`
- 更新了 `docs/QUICK_START_BOARD.md`

## 使用方法

使用方法保持不变，编译时指定板子：

```bash
# CH591D 板子
make TARGET=tracker BOARD=ch591d

# CH592X 板子
make TARGET=tracker BOARD=ch592x
```

代码中仍然使用：

```c
#include "board.h"  // 自动包含对应板子的配置
```

## 优势

1. **系列化管理**: 同一系列的板子组织在一起，便于管理
2. **易于扩展**: 可以轻松添加新系列（如其他厂商的板子）
3. **清晰结构**: 目录结构更清晰，便于理解板子关系
4. **向后兼容**: 使用方式不变，不影响现有代码

## 添加新板子

### 在同一系列中添加

如果新板子属于 mingyue_slimevr 系列：

```bash
mkdir -p board/mingyue_slimevr/myboard
# 创建 config.h 和 pins.h
# 更新 board/board.h 和 Makefile
```

### 添加新系列

如果要添加新的板子系列（如其他厂商）：

```bash
mkdir -p board/other_series/myboard
# 创建配置文件
# 更新 board/board.h 添加新系列支持
```

## 注意事项

- 所有路径已自动更新，无需修改源代码
- 编译方式保持不变
- 配置文件位置已更新，但功能完全相同

