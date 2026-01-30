# 板子配置系统设置完成

## 完成时间
2026-01-30

## 已完成的工作

### 1. 创建 board 目录结构

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

### 2. 配置文件内容

#### CH591D 配置 (`board/mingyue_slimevr/ch591d/config.h`)
- 芯片类型: CH591
- 引脚配置: 20-pin QFN
- 支持功能: 追踪器
- 包含所有必要的配置项

#### CH592X 配置 (`board/mingyue_slimevr/ch592x/config.h`)
- 芯片类型: CH592
- 引脚配置: 28-pin QFN
- 支持功能: 追踪器/接收器
- 包含所有必要的配置项

### 3. 更新构建系统

#### Makefile 更新
- 添加 `BOARD` 参数支持（默认: ch591d）
- 根据板子自动设置芯片类型
- 添加 `-Iboard` 到包含路径
- 自动定义 `BOARD_CH591D` 或 `BOARD_CH592X` 宏

#### 源代码更新
已更新所有源代码文件，将 `#include "config.h"` 改为 `#include "board.h"`：
- `src/main_tracker.c`
- `src/main_receiver.c`
- `src/rf/*.c`
- `src/sensor/*.c`
- `src/hal/*.c`

### 4. 统一入口文件

`board/board.h` 根据编译时定义的宏自动包含对应板子的配置：
- `BOARD_CH591D` → 包含 `mingyue_slimevr/ch591d/config.h` 和 `mingyue_slimevr/ch591d/pins.h`
- `BOARD_CH592X` → 包含 `mingyue_slimevr/ch592x/config.h` 和 `mingyue_slimevr/ch592x/pins.h`

## 使用方法

### 编译 CH591D 板子

```bash
# Tracker
make TARGET=tracker BOARD=ch591d

# 或使用构建脚本
./build.sh tracker BOARD=ch591d
```

### 编译 CH592X 板子

```bash
# Tracker
make TARGET=tracker BOARD=ch592x

# Receiver
make TARGET=receiver BOARD=ch592x

# 或使用构建脚本
./build.sh all BOARD=ch592x
```

### 在代码中使用

```c
#include "board.h"  // 自动包含对应板子的配置

// 使用配置
if (CHIP_TYPE == CH591) {
    // CH591 特定代码
}

// 使用引脚定义
hal_gpio_set_mode(PIN_LED_PORT, PIN_LED, GPIO_ModeOut_PP);
```

## 配置内容

每个板子的配置文件包含：

1. **芯片选择** - `CHIP_TYPE`
2. **IMU传感器** - `IMU_TYPE`
3. **融合算法** - `FUSION_TYPE`
4. **RF配置** - 功率、通道、数据率
5. **引脚定义** - LED、按键、SPI、I2C、ADC等
6. **电源管理** - 休眠、唤醒配置
7. **功能开关** - 各种可选功能
8. **追踪器配置** - `MAX_TRACKERS` 等

## 向后兼容

- 旧的 `include/config.h` 和 `include/config_pins_ch592x.h` 已保留
- 但建议迁移到新的 board 系统
- 所有新代码应使用 `#include "board.h"`

## 添加新板子

1. 在 `board/` 下创建新目录，如 `board/myboard/`
2. 创建 `config.h` 和 `pins.h`
3. 在 `board/board.h` 中添加条件编译
4. 在 `Makefile` 中添加板子选择逻辑

## 相关文档

- `board/README.md` - 板子配置详细说明
- `docs/BOARD_CONFIG.md` - 板子配置系统说明

