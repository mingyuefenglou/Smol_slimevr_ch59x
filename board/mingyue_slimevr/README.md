# 明月 SlimeVR 板子系列

## 系列说明

本目录包含明月 SlimeVR 系列的板子配置，支持 CH591D 和 CH592X 两种板子。

## 支持的板子

### CH591D
- **芯片**: CH591
- **封装**: 20-pin QFN
- **用途**: 追踪器
- **配置文件**: `ch591d/config.h`
- **特点**: 成本低，内存较小（18KB RAM, 256KB Flash）

### CH592X
- **芯片**: CH592
- **封装**: 28-pin QFN
- **用途**: 追踪器/接收器
- **配置文件**: `ch592x/config.h`
- **特点**: 功能完整，内存更大（26KB RAM, 448KB Flash）

## 使用方法

### 编译指定板子

```bash
# CH591D 板子
make TARGET=tracker BOARD=ch591d

# CH592X 板子
make TARGET=tracker BOARD=ch592x
make TARGET=receiver BOARD=ch592x
```

### 修改配置

编辑对应板子的配置文件：

```bash
# 修改 CH591D 配置
vim ch591d/config.h

# 修改 CH592X 配置
vim ch592x/config.h
```

## 配置内容

每个板子的配置文件包含：

1. **芯片类型** - `CHIP_TYPE`
2. **IMU传感器** - `IMU_TYPE`
3. **融合算法** - `FUSION_TYPE`
4. **RF配置** - 功率、通道、数据率
5. **引脚定义** - LED、按键、SPI、I2C、ADC等
6. **电源管理** - 休眠、唤醒配置
7. **功能开关** - 各种可选功能

## 引脚差异

### CH591D (20-pin)
- 引脚较少，功能精简
- 无 I2C 硬件接口（PB22/PB23）
- 电池检测使用 PA8/AIN12

### CH592X (28-pin)
- 引脚更多，功能完整
- 支持 I2C 硬件接口（PB22/PB23）
- 电池检测使用 PA7/AIN11
- 支持更多外设

## 注意事项

1. 确保引脚定义与实际硬件原理图一致
2. CH591D 和 CH592X 的引脚定义不同，不能混用
3. 编译时必须指定正确的 `BOARD` 参数

