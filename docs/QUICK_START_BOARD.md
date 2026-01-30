# 板子配置快速开始

## 快速使用

### 编译指定板子

```bash
# Generic board (need to configure pins first)
make TARGET=tracker BOARD=generic_board

# CH591D 板子
make TARGET=tracker BOARD=ch591d

# CH592X 板子
make TARGET=tracker BOARD=ch592x
make TARGET=receiver BOARD=ch592x

# 使用构建脚本
./build.sh tracker BOARD=generic_board
```

### 修改板子配置

编辑对应板子的配置文件：

```bash
# Modify generic board configuration (need to configure pins)
vim board/generic_board/config.h

# 修改 CH591D 配置
vim board/mingyue_slimevr/ch591d/config.h

# 修改 CH592X 配置
vim board/mingyue_slimevr/ch592x/config.h
```

### 常用配置修改

#### 修改IMU传感器

在 `board/ch591d/config.h` 或 `board/ch592x/config.h` 中：

```c
#define IMU_TYPE            IMU_BMI270    // 改为 BMI270
```

#### 修改LED引脚

在对应板子的 `config.h` 中：

```c
#define PIN_LED             9       // 修改为其他引脚
#define PIN_LED_PORT        GPIOA
```

#### 修改RF功率

在对应板子的 `config.h` 中：

```c
#define RF_TX_POWER_DBM     0       // 降低功率
```

## 配置文件位置

- **Generic Board**: `board/generic_board/config.h` (need to configure pins)
- **CH591D**: `board/mingyue_slimevr/ch591d/config.h`
- **CH592X**: `board/mingyue_slimevr/ch592x/config.h`
- **统一入口**: `board/board.h` (代码中应包含此文件)

## 注意事项

1. 编译时必须指定 `BOARD` 参数
2. 所有代码使用 `#include "board.h"` 而不是 `config.h`
3. 板子配置在编译时确定，不能运行时切换

