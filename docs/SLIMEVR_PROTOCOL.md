# SlimeVR 协议分析与 CH59X 优化

## 1. SlimeVR nRF Receiver 协议分析

### 1.1 无线协议

SlimeVR nRF 使用 Nordic 的 **ESB (Enhanced ShockBurst)** 协议：

| 特性 | 参数 |
|------|------|
| 频段 | 2.4GHz ISM |
| 比特率 | 2Mbps |
| CRC | 16-bit |
| 地址宽度 | 5 bytes |
| 调制 | GFSK |
| 自动应答 | 可选 |

### 1.2 地址方案

```
Discovery Address (固定，用于配对):
  Base 0: 0x62, 0x39, 0x8A, 0xF2
  Prefix:  0xFE, 0xFF, 0x29, 0x27, 0x09, 0x02, 0xB2, 0xD6

Paired Address (从设备唯一ID生成):
  Base 0: Discovery (保持不变)
  Base 1: DeviceAddr[0:3] + DeviceAddr[4]
  Prefix: Discovery (保持不变)
```

### 1.3 数据包格式

#### 配对包 (8 bytes, pipe 0)
```
[0]     checksum    CRC8-CCITT of bytes [2:7]
[1]     stage       0=request, 1=ack_sent, 2=ack_receiver
[2:7]   mac_addr    Tracker MAC address
```

#### 数据包 (16/20/21 bytes, pipe 1+)

| 长度 | 格式 |
|------|------|
| 16B | 基本数据包 |
| 20B | 基本 + CRC32 |
| 21B | 基本 + CRC32 + Sequence |

**包类型:**

| Type | 名称 | 内容 |
|------|------|------|
| 0 | Info | 设备信息、电池、温度、固件版本 |
| 1 | Quat+Accel | 完整精度四元数(s16) + 加速度(s16) |
| 2 | Compact | 压缩四元数 + 加速度 + 电池 + RSSI |
| 3 | Status | 服务器状态、设备状态 |
| 4 | Quat+Mag | 四元数 + 磁力计 |
| 255 | Receiver | 接收器发送的地址注册包 |

#### CRC32 校验

使用 **CRC32-K/4.2** 多项式:
```c
#define CRC32_POLY  0x93a409eb
crc = crc32_k_4_2_update(0x93a409eb, data, 16);
```

### 1.4 序列号跟踪

```c
// 检测丢包
int next = sequences[tracker_id] + 1;
if(seq != 0 && sequences[tracker_id] != 0 && next != seq) {
    if(next > seq && next < seq + 128) {
        // 旧包，丢弃
    }
}
sequences[tracker_id] = seq;
```

### 1.5 检测阈值

```c
#define DETECTION_THRESHOLD 25  // 每秒收到25个包才认为有效
```

## 2. USB HID 输出

### 2.1 报告格式

```
Report Size: 16 bytes x 4 = 64 bytes (USB Full Speed 最大)
Report Rate: 1ms (1000Hz)
```

### 2.2 批量发送策略

```c
// 每次发送4个16字节报告
struct tracker_report ep_report_buffer[4];

// 填充顺序:
// 1. FIFO中的追踪器数据
// 2. 地址注册包 (每100ms发送一次)
```

## 3. CH59X 优化实现

### 3.1 硬件对比

| 特性 | nRF52840 | CH592 |
|------|----------|-------|
| RAM | 256KB | 26KB |
| Flash | 1MB | 448KB |
| 最大追踪器 | 256 | 24 |
| CPU | Cortex-M4 64MHz | RISC-V 60MHz |
| 无线 | 2.4GHz BLE/ESB | 2.4GHz BLE |

### 3.2 内存优化

**nRF52840 原始:**
```c
uint8_t discovered_trackers[MAX_TRACKERS];  // 256 bytes
uint8_t sequences[255];                      // 255 bytes
uint16_t packets_count[255];                 // 510 bytes
uint64_t stored_tracker_addr[256];           // 2048 bytes
// 总计: ~3KB
```

**CH592 优化:**
```c
typedef struct PACKED {
    u64 addr;       // 8 bytes
    u8  sequence;   // 1 byte
    u8  detect_cnt; // 1 byte
    u8  lost_cnt;   // 1 byte
    u8  flags;      // 1 byte
} tracker_state_t;  // 12 bytes × 24 = 288 bytes

static u8 discovered_count[24];  // 24 bytes
static u8 sequences[24];         // 24 bytes
// 总计: ~336 bytes
```

**节省: 89%**

### 3.3 FIFO 优化

**nRF52840:**
```c
struct tracker_report reports[MAX_TRACKERS];  // 4KB
```

**CH592:**
```c
#define REPORT_FIFO_SIZE 32
static hid_report_t report_fifo[32];  // 512 bytes
```

### 3.4 CH592 2.4GHz 无线

CH592 使用自有的 2.4GHz 协议，需要自定义实现类似 ESB 的功能：

```c
// 配置 2.4GHz 发射机
void rf_init(void) {
    // 设置频率
    RF_SetChannel(40);  // 2.440GHz
    
    // 设置发射功率
    RF_SetTxPower(TX_POWER_0_DBM);
    
    // 设置地址
    RF_SetAddress(base_addr, 5);
    
    // 设置 CRC
    RF_SetCRCMode(CRC_16);
    
    // 接收模式
    RF_SetRxMode();
}
```

## 4. VQF 传感器融合优化

### 4.1 算法对比

| 算法 | 精度 | 漂移 | RAM | CPU |
|------|------|------|-----|-----|
| Mahony | ★★ | 高 | 40B | 低 |
| Madgwick | ★★★ | 中 | 60B | 中 |
| VQF Basic | ★★★★ | 低 | 100B | 中 |
| VQF Advanced | ★★★★★ | 极低 | 180B | 高 |

### 4.2 VQF Advanced 特性

1. **自适应加速度校正**
   - 检测运动时降低加速度权重
   - 避免快速运动时的误差

2. **静止检测**
   - 检测设备静止状态
   - 静止时快速校准陀螺仪偏置

3. **运动偏置估计**
   - 运动期间持续估计陀螺仪偏置
   - 基于加速度误差推断

4. **磁力计干扰拒绝** (可选)
   - 检测磁场异常
   - 自动忽略干扰期间的磁力计数据

### 4.3 内存配置

```c
// 最小配置 (无磁力计)
#define VQF_USE_MAGNETOMETER    0
#define VQF_USE_REST_DETECTION  1
#define VQF_USE_MOTION_BIAS     1
// RAM: ~140 bytes

// 完整配置
#define VQF_USE_MAGNETOMETER    1
#define VQF_USE_REST_DETECTION  1
#define VQF_USE_MOTION_BIAS     1
// RAM: ~180 bytes
```

## 5. 是否需要 CH59X SDK?

### 当前情况

项目已包含基本的 SDK 框架:
- `sdk/StdPeriphDriver/` - GPIO, I2C, SPI, Timer
- `sdk/BLE/` - BLE 协议栈头文件
- `sdk/Startup/` - 启动代码

### 需要补充的 SDK 组件

如果要实现完整的 **2.4GHz 私有协议**，需要:

1. **RF 驱动** (`CH59x_rf.h/c`)
   - 2.4GHz 发射/接收函数
   - 频道切换
   - 功率控制

2. **USB 设备驱动** (`CH59x_usb.h/c`)
   - USB HID 设备类
   - 端点配置
   - 中断处理

3. **电源管理** (`CH59x_pwr.h/c`)
   - 低功耗模式
   - 唤醒配置

### 建议

**如果可以分批上传 SDK:**

```
优先级 1: RF 相关 (必须)
  - CH59x_rf.h
  - CH59x_rf.c
  
优先级 2: USB 相关 (必须)
  - CH59x_usb.h
  - CH59x_usb.c
  - CH59x_usbhid.c
  
优先级 3: 其他 (可选)
  - CH59x_pwr.h/c
  - CH59x_adc.c
```

**如果无法上传:**

我可以基于芯片数据手册和已有头文件推断实现，但可能需要您在实际硬件上测试和调整。

## 6. 完整项目结构

```
slimevr_ch59x/
├── include/
│   ├── optimize.h           # 优化宏
│   ├── rf_protocol_opt.h    # 优化的RF协议
│   ├── vqf_advanced.h       # 高级VQF
│   └── ...
├── src/
│   ├── main_tracker.c       # 追踪器 (优化版)
│   ├── main_receiver_opt.c  # 接收器 (SlimeVR兼容)
│   └── sensor/fusion/
│       ├── vqf_opt.c        # 基础VQF
│       └── vqf_advanced.c   # 高级VQF
└── docs/
    ├── MEMORY_OPTIMIZATION.md
    ├── HARDWARE_DESIGN.md
    └── SLIMEVR_PROTOCOL.md  # 本文档
```
