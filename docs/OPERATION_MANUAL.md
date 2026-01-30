# SlimeVR CH592 追踪器操作手册
# SlimeVR CH592 Tracker Operation Manual

**版本 / Version**: 0.7.2  
**日期 / Date**: 2026-01-12  
**适用芯片 / Chips**: CH592, CH591, CH591D
**SlimeVR 兼容版本 / SlimeVR Compatible**: 0.7.x

---

## 目录 / Table of Contents

1. [产品概述](#1-产品概述)
2. [硬件连接](#2-硬件连接)
3. [固件烧录指南](#3-固件烧录指南)
4. [版本更新指南](#4-版本更新指南)
5. [配对方法](#5-配对方法)
6. [日常使用](#6-日常使用)
7. [故障排除](#7-故障排除)
8. [高级配置](#8-高级配置)

---

## 1. 产品概述

### 1.1 系统组成

SlimeVR CH592 系统由以下组件组成:

| 组件 | 数量 | 说明 |
|------|------|------|
| 追踪器 (Tracker) | 5-11 个 | 佩戴在身体各部位 |
| 接收器 (Receiver) | 1 个 | 插入电脑 USB 端口 |
| SlimeVR 软件 | 1 份 | PC 端运行 |

### 1.2 技术规格

| 参数 | 规格 |
|------|------|
| 主控芯片 | WCH CH592 / CH591 |
| 传感器 | ICM-45686 / ICM-42688 / BMI270 |
| 通讯方式 | 2.4GHz 私有协议 |
| 采样率 | 200Hz |
| 延迟 | < 5ms |
| 续航 | 约 10-15 小时 (根据电池容量) |

### 1.3 LED 指示灯说明

| LED 状态 | 含义 |
|----------|------|
| 常亮 | 正常工作 |
| 快闪 (2Hz) | 配对模式 |
| 慢闪 (0.5Hz) | 固件更新模式 |
| 熄灭 | 休眠或关机 |
| 闪烁 3 次 | 配对成功 |

---

## 2. 硬件连接

### 2.1 追踪器引脚定义

#### CH591D (20-pin QFN)

```
CH591D 引脚分配:
┌──────────────────────────────────────┐
│  PA11 - IMU INT1                     │
│  PA10 - CHRG 充电检测                │
│  PA8  - ADC (电池电压检测)           │
│  PA9  - LED 状态指示灯               │
│  PB11 - USB D+                       │
│  PB10 - USB D-                       │
│  PB7  - RST                          │
│  PA12 - IMU SPI CS                   │
│  PA13 - IMU SPI CLK                  │
│  PA14 - IMU SPI MOSI                 │
│  PA15 - IMU SPI MISO                 │
│  PB4  - SW0 用户按键                 │
└──────────────────────────────────────┘
```

#### CH592X (28-pin QFN)

```
CH592X 引脚分配:
┌──────────────────────────────────────┐
│  PA4  - IMU SPI CS                   │
│  PA6  - CLKOUT (外部时钟输出到 IMU)   │
│  PA8  - LED                          │
│  PA12 - IMU SPI CLK                  │
│  PA13 - IMU SPI MOSI                 │
│  PA14 - IMU SPI MISO                 │
│  PA15 - IMU 中断                     │
│  PB4  - SW1 按键                     │
│  PB22 - 充电检测                     │
└──────────────────────────────────────┘
```

### 2.2 接收器引脚定义

```
CH592 接收器引脚:
┌──────────────────────────────────────┐
│  PA8  - LED                          │
│  USB  - 连接电脑                      │
└──────────────────────────────────────┘
```

---

## 3. 固件烧录指南

### 3.1 准备工作

**所需工具:**
- 固件文件 (`tracker.uf2` 或 `tracker.bin`)
- USB 数据线
- (可选) wchisp 工具

### 3.2 方法一: UF2 拖放烧录 (推荐)

这是最简单的烧录方法，无需安装任何软件。

**步骤:**

1. **进入 Bootloader 模式**
   - 按住追踪器上的 **BOOT** 按键
   - 同时将追踪器通过 USB 连接到电脑
   - 松开 BOOT 按键
   - 电脑应识别出一个名为 **NIMISLIME** 的 U 盘

2. **复制固件**
   - 将 `tracker.uf2` 文件拖放到 **NIMISLIME** U盘
   - 等待复制完成 (约 2-3 秒)
   - 设备会自动重启

3. **验证**
   - LED 应亮起表示正常工作
   - U盘消失，设备进入正常模式

**注意:** 如果 U盘名称不是 NIMISLIME，可能是旧版固件，仍可使用同样方法烧录。

### 3.3 方法二: wchisp 命令行烧录

适用于批量烧录或自动化场景。

**安装 wchisp:**

```bash
# Windows (需要 Rust)
cargo install wchisp

# 或下载预编译版本
# https://github.com/ch32-rs/wchisp/releases
```

**烧录命令:**

```bash
# 进入 Bootloader 模式后执行
wchisp flash tracker.bin

# 指定地址烧录
wchisp flash --address 0x00001000 tracker.bin

# 擦除并烧录
wchisp erase --all
wchisp flash tracker.bin
```

### 3.4 方法三: WCHISPTool (Windows GUI)

**下载:**
- 官网: http://www.wch.cn/downloads/WCHISPTool_Setup_exe.html

**步骤:**
1. 安装并打开 WCHISPTool
2. 选择芯片系列: **CH59x**
3. 选择芯片型号: **CH592** 或 **CH591**
4. 点击 "打开文件"，选择 `tracker.hex`
5. 设备进入 Bootloader 模式
6. 点击 "下载"
7. 等待完成

### 3.5 接收器固件烧录

接收器烧录方法与追踪器相同:

```bash
wchisp flash receiver.bin
```

或使用 `receiver.uf2` 拖放烧录。

---

## 4. 版本更新指南

### 4.1 检查当前版本

1. 将接收器插入电脑
2. 打开 SlimeVR 软件
3. 进入 "设置" -> "关于"
4. 查看 "固件版本"

### 4.2 获取新版本

**方法一: 从官方获取**
- 访问 SlimeVR 官网下载最新固件

**方法二: 自行编译**

```bash
# 下载新的 SlimeVR 源码
git clone https://github.com/SlimeVR/SlimeVR-Tracker-ESP.git slimevr-new

# 更新本项目源码
./build_firmware.sh update ./slimevr-new

# 编译
./build_firmware.sh all

# 输出文件在 output/ 目录
# tracker_CH592_v1.0.1.bin
# receiver_CH592_v1.0.1.bin
```

### 4.3 版本号管理

```bash
# 查看当前版本
./build_firmware.sh version
# 输出: v1.0.0

# 增加补丁版本 (bug 修复)
./build_firmware.sh bump patch
# 1.0.0 -> 1.0.1

# 增加次版本 (新功能)
./build_firmware.sh bump minor
# 1.0.1 -> 1.1.0

# 增加主版本 (重大更新)
./build_firmware.sh bump major
# 1.1.0 -> 2.0.0
```

### 4.4 批量更新

如果有多个追踪器需要更新:

```bash
# 编译固件
make both

# 依次将每个追踪器插入电脑并执行
wchisp flash output/tracker.bin
```

---

## 5. 配对方法

### 5.1 首次配对

追踪器和接收器出厂时已预配对，通常无需手动配对。

如果需要重新配对:

**追踪器端:**
1. 开机状态下，**双击** SW1 按键
2. LED 开始快闪表示进入配对模式
3. 等待配对完成 (最长 60 秒)

**接收器端:**
1. 插入电脑 USB
2. LED 快闪表示正在搜索追踪器
3. 找到追踪器后自动配对

**配对成功指示:**
- 追踪器 LED 闪烁 3 次后常亮
- 接收器 LED 常亮

### 5.2 多追踪器配对

1. 将接收器插入电脑
2. 依次将每个追踪器进入配对模式 (双击 SW1)
3. 每个追踪器配对成功后再配对下一个
4. 最多支持 24 个追踪器 (CH592) 或 16 个 (CH591)

### 5.3 配对故障排除

| 问题 | 解决方案 |
|------|----------|
| 配对超时 | 确保追踪器和接收器距离 < 2 米 |
| 配对不稳定 | 检查是否有 2.4GHz 干扰源 |
| 无法进入配对模式 | 检查电池电量 |
| 反复断开连接 | 尝试重新烧录固件 |

---

## 6. 日常使用

### 6.1 开机

- **开机**: 短按 SW1 按键
- LED 亮起表示开机成功

### 6.2 关机 / 休眠

- **休眠**: 长按 SW1 按键 **3 秒**
- LED 熄灭表示进入休眠
- 休眠后可通过按键唤醒

### 6.3 充电

- 使用 USB-C 线连接充电器
- 充电指示灯:
  - 红色: 正在充电
  - 绿色: 充满

### 6.4 校准

**自动校准:**
- 开机后静置 3 秒，自动校准陀螺仪偏差

**手动重置:**
- 同时按住 SW1 + RST 按键 3 秒
- LED 快闪 5 次表示重置成功

### 6.5 连接 SlimeVR 软件

1. 将接收器插入电脑 USB 端口
2. 打开 SlimeVR 软件
3. 软件应自动识别接收器
4. 开启追踪器
5. 在软件中查看追踪器状态

---

## 7. 故障排除

### 7.1 追踪器问题

| 症状 | 可能原因 | 解决方案 |
|------|----------|----------|
| LED 不亮 | 电池没电 | 充电 |
| LED 不亮 | 固件损坏 | 重新烧录固件 |
| 数据漂移 | 需要校准 | 静置 3 秒自动校准 |
| 数据抖动 | 传感器松动 | 检查焊接 |
| 无法配对 | 距离太远 | 靠近接收器 |

### 7.2 接收器问题

| 症状 | 可能原因 | 解决方案 |
|------|----------|----------|
| 电脑无法识别 | USB 接触不良 | 更换 USB 端口 |
| 电脑无法识别 | 驱动问题 | 安装 WCH 驱动 |
| 软件无法连接 | 端口被占用 | 关闭其他软件 |
| 数据丢包 | RF 干扰 | 远离 WiFi 路由器 |

### 7.3 软件问题

| 症状 | 可能原因 | 解决方案 |
|------|----------|----------|
| 无法检测接收器 | 驱动问题 | 重新安装 SlimeVR |
| 追踪器不显示 | 配对问题 | 重新配对 |
| 位置漂移 | 校准问题 | 在软件中重新校准 |

---

## 8. 高级配置

### 8.1 编译配置

修改 `include/optimize.h` 或 Makefile:

```makefile
# CH591 版本 (较少内存)
make CHIP=CH591 TARGET=tracker

# 启用卡尔曼滤波 (更高精度)
# 在 include/ekf_ahrs.h 中取消注释:
# #define USE_EKF_INSTEAD_OF_VQF
```

### 8.2 传感器选择

修改 `Makefile`:

```makefile
# 使用 ICM-45686 (默认)
SENSOR_SRC += src/sensor/imu/icm45686.c

# 使用 BMI270
SENSOR_SRC += src/sensor/imu/bmi270.c

# 使用 ICM-42688
SENSOR_SRC += src/sensor/imu/icm42688.c
```

### 8.3 RF 参数调整

修改 `include/rf_protocol_opt.h`:

```c
// 发射功率 (0dBm 默认, 可选 -20 ~ +4 dBm)
#define RF_TX_POWER     0

// 数据率 (200Hz 默认)
#define RF_DATA_RATE    200
```

### 8.4 休眠配置

修改 `src/main_tracker.c`:

```c
// 长按进入休眠的时间 (毫秒)
#define LONG_PRESS_MS   3000    // 3秒

// 自动休眠超时 (0 = 禁用)
#define AUTO_SLEEP_TIMEOUT_MS   0
// 取消注释启用自动休眠:
// #define AUTO_SLEEP_TIMEOUT_MS   300000  // 5分钟
```

---

## 附录 A: 固件文件说明

| 文件 | 说明 |
|------|------|
| `tracker_CH592_v1.0.0.bin` | 追踪器固件 (二进制) |
| `tracker_CH592_v1.0.0.hex` | 追踪器固件 (Intel HEX) |
| `tracker_CH592_v1.0.0.uf2` | 追踪器固件 (UF2 拖放) |
| `receiver_CH592_v1.0.0.bin` | 接收器固件 (二进制) |
| `receiver_CH592_v1.0.0.hex` | 接收器固件 (Intel HEX) |
| `receiver_CH592_v1.0.0.uf2` | 接收器固件 (UF2 拖放) |

---

## 附录 B: 快捷键汇总

| 操作 | 按键 | 说明 |
|------|------|------|
| 开机 | 短按 SW1 | LED 亮起 |
| 休眠 | 长按 SW1 (3秒) | LED 熄灭 |
| 配对 | 双击 SW1 | LED 快闪 |
| 重置 | SW1 + RST (3秒) | LED 闪 5 次 |
| Bootloader | BOOT + 插入 USB | 出现 NIMISLIME 盘 |

---

## 联系与支持

- GitHub: https://github.com/SlimeVR
- Discord: https://discord.gg/SlimeVR
- 文档: https://docs.slimevr.dev

---

*本手册适用于 SlimeVR CH592 固件 v1.0.0*
