# 板子配置系统说明

## 概述

项目使用 `board/` 目录来管理不同板子的配置文件。编译时通过 `BOARD` 参数选择对应的板子配置。

## 目录结构

```
board/
├── board.h                      # 板子配置统一入口（自动选择板子）
├── README.md                    # 板子配置说明
├── generic_board/              # Generic board configuration (pins undefined)
│   ├── config.h               # Main configuration file (need to configure pins)
│   ├── pins.h                 # Pin definitions (need to configure mappings)
│   └── README.md             # Generic configuration instructions
└── mingyue_slimevr/            # 明月 SlimeVR 板子系列
    ├── README.md               # 系列说明
    ├── ch591d/                 # CH591D 板子配置
    │   ├── config.h           # 主配置文件（芯片、IMU、RF、引脚等）
    │   └── pins.h              # 引脚定义（ADC、PWM、UART映射等）
    └── ch592x/                 # CH592X 板子配置
        ├── config.h           # 主配置文件
        └── pins.h             # 引脚定义
```

## 使用方法

### 编译时指定板子

```bash
# Compile generic board (need to configure pins first)
make TARGET=tracker BOARD=generic_board

# 编译 CH591D 板子的 Tracker
make TARGET=tracker BOARD=ch591d

# 编译 CH592X 板子的 Tracker
make TARGET=tracker BOARD=ch592x

# 编译 CH592X 板子的 Receiver
make TARGET=receiver BOARD=ch592x

# 使用 make 命令（推荐）
make TARGET=tracker BOARD=generic_board
```

### 默认板子

如果不指定 `BOARD`，默认使用 `ch591d`。

## 配置文件说明

### board.h - 统一入口

`board/board.h` 是板子配置的统一入口，根据 `BOARD_CH591D` 或 `BOARD_CH592X` 宏自动包含对应板子的配置：

```c
#include "board.h"  // 自动包含对应板子的配置
```

### config.h - 主配置

每个板子的 `config.h` 包含：

- **芯片类型** (`CHIP_TYPE`)
- **IMU传感器选择** (`IMU_TYPE`)
- **融合算法** (`FUSION_TYPE`)
- **RF配置** (功率、通道、数据率)
- **引脚定义** (LED、按键、SPI、I2C等)
- **电源管理** (休眠、唤醒)
- **功能开关** (各种可选功能)
- **追踪器配置** (`MAX_TRACKERS`)

### pins.h - 引脚定义

每个板子的 `pins.h` 包含：

- **ADC通道映射**
- **PWM通道映射**
- **UART通道映射**
- **定时器映射**

## 支持的板子

### CH591D
- **芯片**: CH591
- **封装**: 20-pin QFN
- **用途**: 追踪器
- **配置文件**: `board/mingyue_slimevr/ch591d/config.h`
- **特点**: 成本低，内存较小

### CH592X
- **芯片**: CH592
- **封装**: 28-pin QFN
- **用途**: 追踪器/接收器
- **配置文件**: `board/mingyue_slimevr/ch592x/config.h`
- **特点**: 功能完整，内存更大

## 代码中使用配置

所有源代码只需包含 `board.h`：

```c
#include "board.h"

void init_gpio(void) {
    // 使用板子配置的引脚定义
    hal_gpio_set_mode(PIN_LED_PORT, PIN_LED, GPIO_ModeOut_PP);
}

void check_battery(void) {
    // 使用板子配置的电池检测引脚
    uint16_t adc = hal_adc_read(PIN_VBAT_ADC_CHANNEL);
}
```

## 添加新板子

### 在同一系列中添加（推荐）

如果新板子属于 mingyue_slimevr 系列：

1. **创建板子目录**
   ```bash
   mkdir -p board/mingyue_slimevr/myboard
   ```

2. **创建配置文件**
   - `board/mingyue_slimevr/myboard/config.h` - 主配置
   - `board/mingyue_slimevr/myboard/pins.h` - 引脚定义

3. **更新 board.h**
   在 `board/board.h` 中添加：
   ```c
   #elif defined(BOARD_MYBOARD)
       #include "mingyue_slimevr/myboard/config.h"
       #include "mingyue_slimevr/myboard/pins.h"
       #define BOARD_NAME "MYBOARD"
       #define BOARD_SERIES "mingyue_slimevr"
   ```

4. **更新 Makefile**
   在 `Makefile` 中添加：
   ```makefile
   else ifeq ($(BOARD),myboard)
       CHIP := CH592
       DEFINES += -DBOARD_MYBOARD
   ```

### 添加新系列

如果要添加新的板子系列（如其他厂商的板子）：

1. 在 `board/` 下创建新系列目录
2. 在新系列目录下创建板子配置
3. 在 `board/board.h` 中添加新系列的条件编译

## 配置示例

### 修改IMU传感器

编辑对应板子的 `config.h`：
```c
#define IMU_TYPE            IMU_BMI270    // 改为 BMI270
```

### 修改引脚

编辑对应板子的 `config.h` 或 `pins.h`：
```c
#define PIN_LED             9       // 修改LED引脚
#define PIN_LED_PORT        GPIOA
```

### 修改RF功率

编辑对应板子的 `config.h`：
```c
#define RF_TX_POWER_DBM     0       // 降低功率到 0dBm
```

## 注意事项

1. **编译时确定**: 板子配置在编译时确定，不能运行时切换
2. **统一入口**: 所有代码应使用 `#include "board.h"`，不要直接包含 `config.h`
3. **引脚一致性**: 确保引脚定义与实际硬件原理图一致
4. **向后兼容**: 旧的 `include/config.h` 已保留但建议迁移到 board 系统

## 迁移指南

如果之前使用 `include/config.h`，需要：

1. 将所有 `#include "config.h"` 改为 `#include "board.h"`
2. 编译时添加 `BOARD=ch591d` 或 `BOARD=ch592x` 参数
3. 根据实际板子选择对应的配置

