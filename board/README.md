# 板子配置说明

## 目录结构

```
board/
├── board.h                      # 板子配置统一入口（自动选择板子）
├── README.md                    # 板子配置说明
├── generic_board/              # Generic board configuration (pins undefined)
│   ├── config.h               # Main configuration file (need to configure pins)
│   ├── pins.h                 # Pin definitions (need to configure mappings)
│   └── README.md              # Generic configuration instructions
├── generic_receiver/           # Generic receiver board configuration (pins undefined)
│   ├── config.h               # Main configuration file (need to configure pins)
│   ├── pins.h                 # Pin definitions (need to configure mappings)
│   └── README.md              # Generic receiver configuration instructions
└── mingyue_slimevr/            # 明月 SlimeVR 板子系列
    ├── README.md               # 系列说明
    ├── ch591d/                 # CH591D 板子配置
    │   ├── config.h           # 主配置文件
    │   └── pins.h             # 引脚定义
    └── ch592x/                 # CH592X 板子配置
        ├── config.h           # 主配置文件
        └── pins.h             # 引脚定义
```

## 使用方法

### 编译时指定板子

```bash
# Compile generic receiver (need to configure pins first)
make TARGET=receiver BOARD=generic_receiver

# Compile generic board (need to configure pins first)
make TARGET=tracker BOARD=generic_board

# 编译 CH591D 板子
make TARGET=tracker BOARD=ch591d

# 编译 CH592X 板子
make TARGET=tracker BOARD=ch592x

# 或使用构建脚本
./build.sh tracker BOARD=generic_board
```

### 在代码中使用配置

所有源代码只需包含 `board.h`，它会自动包含对应板子的配置：

```c
#include "board.h"  // 自动包含对应板子的配置

// 使用配置
if (CHIP_TYPE == CH591) {
    // CH591 特定代码
}
```

## 支持的板子

### Generic Receiver
- **Chip**: CH592 (can be changed to CH591)
- **Package**: Custom
- **Usage**: Receiver only
- **Config File**: `board/generic_receiver/config.h`
- **Features**: Pins undefined, need to configure according to actual hardware

### Generic Board
- **Chip**: CH592 (can be changed to CH591)
- **Package**: Custom
- **Usage**: Tracker
- **Config File**: `board/generic_board/config.h`
- **Features**: Pins undefined, need to configure according to actual hardware

### CH591D
- **芯片**: CH591
- **封装**: 20-pin QFN
- **用途**: 追踪器
- **配置文件**: `board/mingyue_slimevr/ch591d/config.h`

### CH592X
- **芯片**: CH592
- **封装**: 28-pin QFN
- **用途**: 追踪器/接收器
- **配置文件**: `board/mingyue_slimevr/ch592x/config.h`

## 配置内容

每个板子的配置文件包含：

1. **芯片类型** (`CHIP_TYPE`)
2. **IMU传感器选择** (`IMU_TYPE`)
3. **融合算法** (`FUSION_TYPE`)
4. **RF配置** (功率、通道、数据率等)
5. **引脚定义** (LED、按键、SPI、I2C等)
6. **电源管理** (休眠、唤醒等)
7. **功能开关** (各种可选功能)

## 添加新板子

### 在同一系列中添加

如果新板子属于现有系列（如 mingyue_slimevr），在对应系列目录下添加：

1. 在 `board/mingyue_slimevr/` 下创建新文件夹，如 `board/mingyue_slimevr/myboard/`
2. 创建 `config.h` 和 `pins.h`
3. 在 `board/board.h` 中添加新板子的条件编译
4. 在 `Makefile` 中添加板子选择逻辑

### 添加新系列

如果要添加新的板子系列：

1. 在 `board/` 下创建新系列目录，如 `board/new_series/`
2. 在新系列目录下创建板子配置
3. 在 `board/board.h` 中添加新系列的条件编译

## 注意事项

- 所有源代码应使用 `#include "board.h"` 而不是直接包含 `config.h`
- 板子配置在编译时确定，不能运行时切换
- 引脚定义应与实际硬件原理图一致

