# SlimeVR CH59X 使用指南

## 目录

1. [快速开始](#快速开始)
2. [编译固件](#编译固件)
3. [烧录固件](#烧录固件)
4. [配对操作](#配对操作)
5. [日常使用](#日常使用)
6. [故障排除](#故障排除)

---

## 快速开始

### 硬件准备

- CH592/CH591 开发板或自制 PCB
- 支持的 IMU: ICM42688, ICM45686, BMI270, BMI323, LSM6DSO/DSR/DSV
- WCH-LinkE 编程器
- USB 数据线

### 软件准备

```bash
# 安装 RISC-V 工具链
# Windows: 下载 xPack RISC-V GCC
# Linux:
sudo apt install gcc-riscv64-unknown-elf

# 克隆/解压项目
unzip slimevr_ch59x_v0.4.12.zip
cd slimevr_ch59x
```

---

## 编译固件

### 1. 编译 Bootloader

```bash
cd bootloader
make clean
make
```

输出文件:
- `output/bootloader_ch592.bin`
- `output/bootloader_ch592.hex`

### 2. 编译 Tracker 固件

```bash
cd ..
make TARGET=tracker clean
make TARGET=tracker
```

输出文件:
- `output/tracker_ch592.bin`
- `output/tracker_ch592.hex`

### 3. 编译 Receiver 固件

```bash
make TARGET=receiver clean
make TARGET=receiver
```

输出文件:
- `output/receiver_ch592.bin`
- `output/receiver_ch592.hex`

### 4. 合并 HEX 文件

```bash
python3 tools/merge_hex.py \
    bootloader/output/bootloader_ch592.hex \
    output/tracker_ch592.hex \
    -o output/tracker_combined.hex
```

---

## 烧录固件

### 方法 1: 使用 WCH-LinkE

```bash
# 连接 WCH-LinkE 到目标板
# SWD 接口: SWDIO, SWCLK, GND, 3.3V

# 烧录合并后的固件
wch-link -w output/tracker_combined.hex

# 或分别烧录
wch-link -w bootloader/output/bootloader_ch592.hex
wch-link -w output/tracker_ch592.hex
```

### 方法 2: 使用 WCHISPTool (Windows)

1. 打开 WCHISPTool
2. 选择芯片型号: CH592F
3. 加载 HEX 文件
4. 点击"下载"

### 方法 3: USB DFU 模式 (需要 Bootloader)

1. 按住 BOOT 按键 (PB4)
2. 连接 USB
3. 释放按键
4. 将 UF2 文件拖放到出现的 USB 驱动器

---

## 配对操作

### Receiver 端

1. 连接 Receiver 到电脑 USB
2. **长按按键 3 秒** 进入配对模式
3. LED 快速闪烁表示配对模式
4. 等待 Tracker 连接
5. 配对成功后 LED 恢复正常

### Tracker 端

1. 确保 Tracker 已开机
2. **长按按键 3 秒** 进入配对模式
3. LED 快速闪烁
4. 自动搜索 Receiver
5. 配对成功后 LED 恢复正常

### 配对状态

| Tracker LED | 状态 |
|-------------|------|
| 慢闪 (1Hz) | 正常运行 |
| 快闪 (5Hz) | 配对模式 |
| 常亮 | 充电中 |
| 双闪 | 搜索同步中 |
| 三闪 | 错误 |

---

## 日常使用

### 开机

1. 短按 Tracker 按键
2. LED 闪烁 3 次表示启动
3. 自动搜索 Receiver 并同步

### 关机

- Tracker: 长按按键 5 秒
- 或等待电池耗尽自动关机

### 校准

1. 将 Tracker 放置在平稳表面
2. **双击按键** 开始校准
3. LED 持续亮起
4. 保持静止 5 秒
5. LED 熄灭表示完成

### 重置配对

1. **三次快速按键** 进入重置模式
2. 所有配对信息将被清除
3. 需要重新配对

---

## 故障排除

### 问题: Tracker 无法连接

**检查清单:**
1. Receiver 是否已开启并在配对模式?
2. Tracker 和 Receiver 是否已配对?
3. 距离是否太远? (建议 < 10m)
4. 是否有 2.4GHz 干扰?

**解决方案:**
- 重新配对
- 靠近 Receiver
- 远离 WiFi 路由器

### 问题: 姿态漂移

**可能原因:**
1. 未校准
2. 温度变化
3. 磁场干扰

**解决方案:**
- 执行校准流程
- 等待 IMU 预热 (约 5 分钟)
- 远离金属物体

### 问题: LED 不亮

**检查清单:**
1. 电池是否有电?
2. 充电是否正常?
3. LED 是否损坏?

**解决方案:**
- 充电
- 检查充电器
- 更换 LED

### 问题: USB 无法识别

**检查清单:**
1. USB 线是否数据线?
2. 驱动是否安装?
3. 固件是否正确?

**解决方案:**
- 更换 USB 线
- 安装 WCH USB 驱动
- 重新烧录固件

---

## 高级设置

### 修改 RF 参数

编辑 `include/config.h`:

```c
#define RF_TX_POWER         RF_TX_POWER_4DBM  // 发射功率
#define RF_DATA_RATE        RF_RATE_2MBPS     // 数据速率
#define MAX_TRACKERS        16                 // 最大 Tracker 数
```

### 修改 IMU 采样率

编辑 `include/config.h`:

```c
#define IMU_SAMPLE_RATE     200   // Hz
#define IMU_FILTER_CUTOFF   50    // Hz
```

### 启用调试输出

编辑 `Makefile`:

```makefile
CFLAGS += -DDEBUG_ENABLED
```

---

## 获取帮助

- GitHub Issues: [项目地址]
- Discord: SlimeVR 官方服务器
- 文档: `docs/` 目录
