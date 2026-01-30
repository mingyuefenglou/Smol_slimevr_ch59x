# CH591D 硬件引脚定义 v2
# CH591D Hardware Pin Definitions v2

## 概述

本文档定义了 SlimeVR CH591D 追踪器和接收器的完整引脚分配。
已修正: 充电 LED 由 TP4054 控制, MCU 仅检测; 添加时钟同步引脚; 明确 IMU 中断定义。

---

## CH591D 芯片引脚图

```
                    CH591D (QFN28)
                  ┌───────────────┐
           PA9  ─┤1            28├─ PA8   ← LED 状态灯
          PA10  ─┤2            27├─ PA7   ← 用户按键 SW1
          PA11  ─┤3            26├─ PA6   ← IMU 时钟输出 CLKOUT
          PA12  ─┤4            25├─ PA5   ← 充电检测 (TP4054 CHRG)
          PA13  ─┤5            24├─ PA4   ← SPI CS (IMU)
          PA14  ─┤6            23├─ VIO33
          PA15  ─┤7            22├─ VDD33
           GND  ─┤8            21├─ VSS
       USB_DP   ─┤9            20├─ RST
       USB_DM   ─┤10           19├─ PB23  ← I2C SDA (备用)
          XI32  ─┤11           18├─ PB22  ← I2C SCL (备用) / 电池 ADC
          XO32  ─┤12           17├─ PB7   ← IMU INT1 (数据就绪)
          VCC33 ─┤13           16├─ PB6   ← IMU INT2 (运动检测, 可选)
          BAT   ─┤14           15├─ PB4   ← BOOT 按键
                  └───────────────┘
```

---

## 追踪器完整引脚定义

### 核心功能引脚 (不可修改)

| 引脚 | GPIO | 功能 | 方向 | 说明 | 可修改 |
|------|------|------|------|------|--------|
| 4 | **PA12** | SPI_CLK | 输出 | 硬件 SPI 时钟 | ❌ 否 |
| 5 | **PA13** | SPI_MOSI | 输出 | 硬件 SPI 数据输出 | ❌ 否 |
| 6 | **PA14** | SPI_MISO | 输入 | 硬件 SPI 数据输入 | ❌ 否 |
| 15 | **PB4** | BOOT | 输入 | Bootloader 进入 | ❌ 否 |
| 9 | USB_DP | USB D+ | 双向 | USB 数据正 | ❌ 否 |
| 10 | USB_DM | USB D- | 双向 | USB 数据负 | ❌ 否 |

### 可配置功能引脚

| 引脚 | GPIO | 默认功能 | 方向 | 说明 | 可修改 |
|------|------|----------|------|------|--------|
| 28 | PA8 | LED | 输出 | 状态指示灯 | ✅ 是 |
| 27 | PA7 | SW1 | 输入 | 用户按键 (上拉) | ✅ 是 |
| 26 | **PA6** | **CLKOUT** | **输出** | **IMU 外部时钟 32.768kHz** | ✅ 是 |
| 25 | PA5 | CHRG_DET | 输入 | TP4054 CHRG 检测 (仅检测) | ✅ 是 |
| 24 | PA4 | SPI_CS | 输出 | IMU 片选 | ✅ 是 |
| 17 | **PB7** | **IMU_INT1** | **输入** | **IMU 数据就绪中断** | ✅ 是 |
| 16 | PB6 | IMU_INT2 | 输入 | IMU 运动检测中断 (可选) | ✅ 是 |
| 18 | PB22 | VBAT_ADC / I2C_SCL | 输入/输出 | 电池检测或 I2C 时钟 | ✅ 是 |
| 19 | PB23 | I2C_SDA | 输入/输出 | I2C 数据 (备用接口) | ✅ 是 |

---

## IMU 连接详解

### 首选: SPI 连接 (推荐)

```
CH591D                          IMU (ICM-45686 / BMI270 / LSM6DSV)
──────                          ─────────────────────────────────
PA12 (SPI_CLK)  ─────────────── SCLK
PA13 (SPI_MOSI) ─────────────── SDI (MOSI)
PA14 (SPI_MISO) ─────────────── SDO (MISO)
PA4  (SPI_CS)   ─────────────── CS (片选, active low)
PB7  (IMU_INT1) ─────────────── INT1 (数据就绪)
PB6  (IMU_INT2) ─────────────── INT2 (运动检测, 可选)
PA6  (CLKOUT)   ─────────────── CLKIN (外部时钟, 可选)
```

**SPI 配置:**
- 模式: SPI Mode 3 (CPOL=1, CPHA=1)
- 速率: 最高 10MHz
- 字节序: MSB first

### 备选: I2C 连接

```
CH591D                          IMU
──────                          ───
PB22 (I2C_SCL) ─────────────── SCL (需要 4.7kΩ 上拉)
PB23 (I2C_SDA) ─────────────── SDA (需要 4.7kΩ 上拉)
PB7  (IMU_INT1) ─────────────── INT1
PA6  (CLKOUT)   ─────────────── CLKIN (可选)
```

**I2C 配置:**
- 速率: 400kHz (Fast Mode)
- 地址: ICM-45686 = 0x68/0x69, LSM6DSV = 0x6A/0x6B

**注意:** 使用 I2C 时, PB22 无法用于电池 ADC, 需要使用其他 ADC 引脚 (如 PA4/PA5)。

---

## IMU 中断说明

### INT1 - 数据就绪中断 (主要)

| IMU | INT1 功能 | 触发条件 | 推荐配置 |
|-----|-----------|----------|----------|
| ICM-45686 | DATA_RDY | 新数据可读 | 上升沿触发 |
| ICM-42688 | DATA_RDY | 新数据可读 | 上升沿触发 |
| BMI270 | DRDY | Accel+Gyro 就绪 | 上升沿触发 |
| LSM6DSV | INT1_DRDY | 数据就绪 | 上升沿触发 |
| LSM6DSR | INT1_DRDY_G | 陀螺仪就绪 | 上升沿触发 |

**配置代码示例:**
```c
// ICM-45686 INT1 配置
icm45686_write_reg(INT_CONFIG0, 0x02);  // INT1 = DATA_RDY
icm45686_write_reg(INT_CONFIG1, 0x01);  // Push-pull, active high

// BMI270 INT1 配置
bmi270_write_reg(INT1_IO_CTRL, 0x0A);   // Push-pull, active high
bmi270_write_reg(INT_MAP_DATA, 0x04);   // DRDY -> INT1
```

### INT2 - 运动检测中断 (可选)

| IMU | INT2 功能 | 用途 |
|-----|-----------|------|
| ICM-45686 | WOM (Wake on Motion) | 唤醒休眠 |
| BMI270 | Any-motion | 运动检测 |
| LSM6DSV | Wake-up | 低功耗唤醒 |

INT2 主要用于低功耗模式下的运动唤醒，正常工作时可不连接。

---

## 时钟同步输出 (PA6 CLKOUT)

### 功能说明

PA6 输出 32.768kHz 方波，用于 IMU 外部时钟输入，实现:
1. **多 IMU 同步**: 多个追踪器使用同一时钟源
2. **降低抖动**: 比 IMU 内部 RC 振荡器更稳定
3. **省电**: IMU 可关闭内部振荡器

### 硬件连接

```
CH591D PA6 ────┬──── 100Ω ────┬──── IMU1 CLKIN
               │              │
               │              └──── IMU2 CLKIN (多追踪器同步)
               │
               └──── 1kΩ ────────── 测试点 (可选)
```

### 配置代码

```c
// 在 hal_clkin.c 中
void hal_clkout_init(void)
{
    // 使用 Timer1 PWM 生成 32.768kHz
    // 系统时钟 60MHz / 1831 ≈ 32.768kHz
    TMR1_PWMInit(High_Level, PWM_Times_1);
    TMR1_PWMCycleCfg(1831);
    GPIOA_ModeCfg(GPIO_Pin_6, GPIO_ModeOut_PP_5mA);
}
```

### IMU 外部时钟配置

**ICM-45686:**
```c
// 启用外部时钟
icm45686_write_reg(REG_INTF_CONFIG1, 0x91);  // RTC_MODE=1, CLKIN=1
```

**LSM6DSV:**
```c
// 外部时钟输入
lsm6dsv_write_reg(CTRL3_C, 0x04);  // External clock mode
```

---

## 充电检测 (PA5)

### 硬件连接

```
          TP4054                CH591D
          ──────                ──────
USB 5V ──→ VIN
          │
          ├── CHRG ──────────── PA5 (输入, 内部上拉)
          │    │
          │    └── LED ──R── GND  (TP4054 直接控制)
          │
BAT+ ────← BAT
```

**重要:** 充电 LED 由 TP4054 CHRG 引脚直接控制，CH591D 仅读取状态。

### TP4054 CHRG 引脚状态

| 状态 | CHRG 电平 | PA5 读数 |
|------|-----------|----------|
| 充电中 | 低 (拉低 LED) | 0 |
| 充电完成 | 高阻 | 1 (上拉) |
| 未连接电源 | 高阻 | 1 (上拉) |
| 故障 | 低 (闪烁) | 0/1 |

### 检测代码

```c
// 初始化
GPIOA_ModeCfg(GPIO_Pin_5, GPIO_ModeIN_PU);  // 输入, 内部上拉

// 读取状态
bool is_charging(void) {
    return (GPIOA_ReadPortPin(GPIO_Pin_5) == 0);
}
```

---

## config.h 引脚定义

```c
/*============================================================================
 * 引脚定义 / Pin Definitions
 * 
 * 根据实际硬件修改以下定义
 *============================================================================*/

// LED 状态灯
#define PIN_LED             8       // PA8

// 用户按键
#define PIN_SW1             7       // PA7

// IMU SPI 接口 (不可修改)
#define PIN_SPI_CS          4       // PA4
#define PIN_SPI_CLK         12      // PA12 (硬件固定)
#define PIN_SPI_MOSI        13      // PA13 (硬件固定)
#define PIN_SPI_MISO        14      // PA14 (硬件固定)

// IMU 中断
#define PIN_IMU_INT1        7       // PB7 - 数据就绪 (主要)
#define PIN_IMU_INT2        6       // PB6 - 运动检测 (可选)

// IMU 时钟同步输出
#define PIN_CLKOUT          6       // PA6 - 32.768kHz 输出到 IMU CLKIN

// 充电检测 (仅检测, LED 由 TP4054 控制)
#define PIN_CHRG_DET        5       // PA5 - 连接 TP4054 CHRG

// 电池 ADC (与 I2C_SCL 复用)
#define PIN_VBAT_ADC        22      // PB22

// I2C 备用接口 (用于 IMU 或扩展)
#define PIN_I2C_SCL         22      // PB22
#define PIN_I2C_SDA         23      // PB23

// BOOT 按键 (不可修改)
#define PIN_BOOT            4       // PB4
```

---

## 引脚修改总结

| 引脚 | 功能 | 可修改 | 修改方法 |
|------|------|--------|----------|
| PA8 | LED | ✅ | 修改 PIN_LED |
| PA7 | SW1 | ✅ | 修改 PIN_SW1 |
| PA6 | CLKOUT | ✅ | 修改 PIN_CLKOUT (需要 PWM 功能) |
| PA5 | CHRG_DET | ✅ | 修改 PIN_CHRG_DET |
| PA4 | SPI_CS | ✅ | 修改 PIN_SPI_CS |
| PA12-14 | SPI | ❌ | 硬件 SPI 固定 |
| PB7 | IMU_INT1 | ✅ | 修改 PIN_IMU_INT1 |
| PB6 | IMU_INT2 | ✅ | 修改 PIN_IMU_INT2 |
| PB22 | VBAT/I2C | ✅ | 需选择功能 |
| PB23 | I2C_SDA | ✅ | I2C 模式时使用 |
| PB4 | BOOT | ❌ | Bootloader 固定 |

---

*最后更新: 2026-01-12 v2*
