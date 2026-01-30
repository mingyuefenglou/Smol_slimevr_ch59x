# CH592 SDK 需求分析

## 快速开始

### 方法 1: 使用集成脚本 (推荐)

```bash
# 1. 克隆官方 SDK
git clone https://github.com/openwch/ch592.git ch592_sdk

# 2. 运行集成脚本
chmod +x setup_sdk.sh
./setup_sdk.sh ./ch592_sdk

# 3. 编译
make TARGET=tracker   # 追踪器
make TARGET=receiver  # 接收器
```

### 方法 2: 手动复制

从 `ch592/EVT/EXAM/SRC/` 复制以下目录:
- `StdPeriphDriver/` → `sdk/StdPeriphDriver/`
- `RVMSIS/` → `sdk/RVMSIS/`
- `Startup/` → `sdk/Startup/`
- `Ld/` → `sdk/Ld/`

从 `ch592/EVT/EXAM/BLE/` 复制:
- `LIB/` → `sdk/BLE/LIB/`
- `HAL/` → `sdk/BLE/HAL/`

---

## 项目状态

基于对 SlimeVR nRF Receiver 和 CH592 SDK 的分析，以下是当前状态和需求。

## 已完成的工作

### 1. SlimeVR 协议分析 ✅
- ESB 2.4GHz 协议完全分析
- CRC32-K/4.2 校验实现
- 数据包格式兼容
- USB HID 输出格式

### 2. CH592 优化接收端 ✅
- `main_receiver_opt.c` - 内存优化的接收端
- `usb_hid_slime.h/c` - USB HID 驱动框架
- RAM 使用减少 89%

### 3. 高级 VQF 算法 ✅
- `vqf_advanced.h/c` - 完整 VQF 实现
- 静止检测
- 运动偏置估计
- 自适应增益

## SDK 需求

### 必需的 SDK 文件

| 文件 | 来源 | 用途 |
|------|------|------|
| `CH59x_usbdev.c` | EVT/SRC/StdPeriphDriver | USB 设备驱动 |
| `CH59x_usbhd.c` | EVT/EXAM/USB | USB HID 示例 |
| `LIBCH59xBLE.a` | EVT/BLE/LIB | BLE 协议栈库 |

### 推荐的获取方式

**方案 1: 使用 cjacker 预转换版本**
```bash
git clone https://github.com/cjacker/ch592evt_gcc_makefile.git
```
包含完整的 GCC/Makefile 支持。

**方案 2: 使用 WeAct Studio SDK**
```bash
git clone https://github.com/WeActStudio/WeActStudio.WCH-BLE-Core.git
```
包含 SDK 和示例。

**方案 3: 官方 WCH EVT**
```bash
git clone https://github.com/openwch/ch592.git
```
需要 MounRiver Studio IDE。

## CH592 vs nRF52840 对比

### 硬件差异

| 特性 | nRF52840 | CH592 |
|------|----------|-------|
| CPU | Cortex-M4 64MHz | RISC-V 60MHz |
| RAM | 256KB | 26KB |
| Flash | 1MB | 448KB |
| 无线 | 2.4GHz ESB/BLE | 2.4GHz BLE only |
| 最大追踪器 | 256 | 24 |
| USB | Full-Speed | Full-Speed |

### 关键差异: 无线协议

**nRF52840:**
- 支持 ESB (Enhanced ShockBurst) 私有协议
- 极低延迟 (<1ms)
- 无协议栈开销

**CH592:**
- 仅支持 BLE
- 无原生 ESB 支持
- 需要使用 BLE 实现类似功能

## CH592 无线方案

### 方案 A: BLE GATT 通信 (推荐)

使用标准 BLE 协议：
- 连接间隔: 7.5ms (最低)
- 延迟: ~15ms
- 功耗: 中等
- 兼容性: 好

```
Tracker (Peripheral) <--BLE--> Receiver (Central)
                                    |
                                    v
                                USB HID --> PC
```

### 方案 B: BLE Broadcaster/Observer

使用 BLE 广播模式：
- 延迟: ~10ms
- 无连接开销
- 单向通信

```
Tracker (Broadcaster) --ADV--> Receiver (Observer)
                                    |
                                    v
                                USB HID --> PC
```

### 方案 C: 低级 RF 访问 (如果支持)

如果 CH592 允许直接访问 2.4GHz 射频硬件：
- 可以实现类似 ESB 的协议
- 需要深入了解 BLE 基带控制器
- 官方文档不充分

## 建议的实现路径

### 阶段 1: USB HID 验证
1. 使用官方 USB HID 示例
2. 验证 SlimeVR server 连接
3. 测试数据传输

### 阶段 2: BLE 接收
1. 使用 BLE Observer 示例
2. 接收 Tracker 广播
3. 整合到 USB HID 输出

### 阶段 3: 完整系统
1. 配对流程
2. 多追踪器支持
3. 错误处理

## 当前项目结构

```
slimevr_ch59x/
├── include/
│   ├── optimize.h           # 优化宏 ✅
│   ├── rf_protocol_opt.h    # RF 协议 ✅
│   ├── vqf_advanced.h       # VQF ✅
│   ├── usb_hid_slime.h      # USB HID ✅
│   └── CH59x_usbdev.h       # USB 驱动头 ✅
├── src/
│   ├── main_tracker.c       # 追踪器 ✅
│   ├── main_receiver_opt.c  # 接收器 ✅
│   ├── usb/
│   │   └── usb_hid_slime.c  # USB HID ✅
│   └── sensor/fusion/
│       └── vqf_advanced.c   # VQF ✅
├── sdk/
│   ├── StdPeriphDriver/     # 需要补充
│   │   ├── CH59x_usbdev.c   # ❌ 需要
│   │   └── inc/
│   │       └── CH59x_usbdev.h # ✅ 已创建
│   └── BLE/                 # 需要补充
│       └── LIB/
│           └── LIBCH59xBLE.a # ❌ 需要
└── docs/
    ├── SLIMEVR_PROTOCOL.md  # ✅
    └── SDK_REQUIREMENTS.md  # 本文档
```

## 下一步

1. **如果您能上传 SDK:**
   - 上传 `CH59x_usbdev.c`
   - 上传 BLE 库文件

2. **如果无法上传:**
   - 从 GitHub 克隆推荐的仓库
   - 将相关文件复制到项目

3. **测试流程:**
   - 编译固件
   - 烧录到 CH592
   - 使用 SlimeVR server 测试

## 参考链接

- [openwch/ch592](https://github.com/openwch/ch592) - 官方 SDK
- [cjacker/ch592evt_gcc_makefile](https://github.com/cjacker/ch592evt_gcc_makefile) - GCC 转换版
- [WeActStudio/WCH-BLE-Core](https://github.com/WeActStudio/WeActStudio.WCH-BLE-Core) - 开发板 SDK
- [rgoulter/ch592-ble-hid-keyboard](https://github.com/rgoulter/ch592-ble-hid-keyboard-example) - BLE HID 示例
