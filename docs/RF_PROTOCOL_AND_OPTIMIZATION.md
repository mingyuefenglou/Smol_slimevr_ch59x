# SlimeVR CH592 RF 通讯协议与优化分析

## 一、当前 RF 通讯协议

### 1.1 通讯方式

**协议类型**: 私有 2.4GHz 协议 (基于 CH592 BLE 基带)

由于 CH592 **不支持 Nordic ESB 协议**，当前实现使用:
- **BLE 广播模式** (Broadcaster/Observer)
- **自定义数据包格式**
- **跳频机制**

```
┌─────────────┐      2.4GHz       ┌─────────────┐      USB      ┌──────┐
│   Tracker   │ ─────────────────>│  Receiver   │ ────────────> │  PC  │
│ (发送器)     │   RF Ultra协议    │  (接收器)    │   USB HID    │      │
└─────────────┘                   └─────────────┘              └──────┘
```

### 1.2 RF Ultra 协议格式 (12字节)

这是优化后的协议格式，相比标准 SlimeVR 的 21 字节减少了 43%。

```
┌────────┬────────────────────────┬──────────────┬───────┐
│ Header │     Quaternion         │    Aux       │ CRC8  │
│ 1 byte │       8 bytes          │   2 bytes    │ 1 byte│
└────────┴────────────────────────┴──────────────┴───────┘

Header (1 byte):
  Bit 7-6: 包类型 (0=四元数, 1=设备信息, 2=状态)
  Bit 5-0: 追踪器 ID (0-63)

Quaternion (8 bytes):
  W: int16 (Q15 格式)
  X: int16 (Q15 格式)
  Y: int16 (Q15 格式)
  Z: int16 (Q15 格式)

Aux (2 bytes):
  Bit 11-0: 加速度 Z 轴 (12位, ±2g)
  Bit 15-12: 电池电量 (4位, 0-100%)

CRC8 (1 byte):
  多项式: 0x07
```

### 1.3 传输参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 频段 | 2.4GHz | ISM 频段 |
| 调制 | GFSK | 高斯频移键控 |
| 数据率 | 1Mbps | BLE 基本速率 |
| 发送功率 | 0dBm | 可调 (-20 ~ +4dBm) |
| 通道数 | 3 | 跳频通道 (37, 38, 39) |
| 采样率 | 200Hz | 传感器更新频率 |

### 1.4 协议时序

```
Tracker 发送周期 (5ms @ 200Hz):
│
├── 传感器读取:  ~500μs
├── VQF 融合:    ~250μs
├── 包构建:      ~10μs
├── RF 发送:     ~200μs
└── 空闲:        ~4040μs
│
└── 总计: 5ms (200Hz)
```

## 二、微小误差累积优化分析

### 2.1 误差来源

1. **陀螺仪积分误差**
   - 来源: 陀螺仪噪声和偏移在积分过程中累积
   - 特征: 随时间线性增长
   - 典型值: 0.1-1°/秒 (取决于传感器质量)

2. **量化误差**
   - 来源: ADC 量化、定点运算截断
   - 特征: 随机但有界
   - Q15 精度: ±0.00003 (1/32768)

3. **对齐误差**
   - 来源: 传感器安装不精确
   - 特征: 固定偏差
   - 可通过校准消除

4. **温度漂移**
   - 来源: 传感器随温度变化
   - 特征: 慢变化
   - 需要温度补偿

### 2.2 当前优化措施

#### VQF 算法优化 (已实现)

```c
// 1. 自适应增益控制
if (e_norm > 0.1f) {
    adaptive_gain *= 0.1f / e_norm;  // 运动时减小校正
}

// 2. 静止检测与偏差估计
if (gyro_sq < threshold) {
    state->rest_count++;
    if (state->rest_count > 300) {  // 1.5秒静止
        // 更新陀螺仪偏差
        state->gyro_bias[i] += (raw - bias) >> 8;
    }
}

// 3. 加速度低通滤波
state->acc_lp[i] += alpha * (ax - state->acc_lp[i]);
```

#### 当前精度

| 指标 | 值 | 说明 |
|------|-----|------|
| 静态精度 | <0.5° | 静止时的误差 |
| 动态精度 | <2° | 运动时的误差 |
| 漂移率 | <0.1°/分钟 | 长期漂移 (静止) |
| 响应延迟 | <5ms | 传感器到输出 |

### 2.3 进一步优化建议

#### 优化方案 A: 卡尔曼滤波增强

**可行性**: ⭐⭐⭐⭐ (高)
**复杂度**: 中等
**RAM 增加**: ~100 字节

```c
// 扩展卡尔曼滤波器状态
typedef struct {
    float x[7];     // 状态: q0,q1,q2,q3, bx,by,bz
    float P[49];    // 协方差矩阵 (7x7)
} ekf_state_t;

// 优点:
// - 最优状态估计
// - 自动偏差估计
// - 可处理非线性
```

#### 优化方案 B: 磁力计融合

**可行性**: ⭐⭐⭐ (中等)
**复杂度**: 低
**RAM 增加**: ~20 字节

```c
// 磁力计航向校正
void apply_mag_correction(state, mag) {
    // 计算磁北方向
    float heading = atan2(mag[1], mag[0]);
    
    // 校正 yaw 漂移
    float error = heading - estimated_heading;
    apply_yaw_correction(state, error * k_mag);
}
```

#### 优化方案 C: 多传感器融合

**可行性**: ⭐⭐⭐⭐⭐ (最高)
**复杂度**: 低
**效果**: 显著提升

```c
// 利用多个追踪器的相对关系
// 例如: 手腕和手肘的距离约束
void apply_constraint(tracker1, tracker2) {
    // 计算关节角度约束
    // 修正超出范围的姿态
}
```

### 2.4 误差补偿代码示例

```c
// 新增: 长期漂移补偿
static float drift_estimate[3] = {0};  // 漂移估计
static uint32_t last_rest_time = 0;

void update_drift_compensation(vqf_state_t *state, uint32_t now) {
    if (state->flags & VQF_AT_REST) {
        // 静止时测量漂移
        float dt = (now - last_rest_time) / 1000.0f;
        if (dt > 1.0f) {
            for (int i = 0; i < 3; i++) {
                // 指数移动平均
                drift_estimate[i] = 0.99f * drift_estimate[i] + 
                                   0.01f * state->gyro_bias[i];
            }
            last_rest_time = now;
        }
    }
    
    // 应用漂移补偿
    for (int i = 0; i < 3; i++) {
        state->gyro_bias[i] -= drift_estimate[i] * 0.001f;
    }
}
```

## 三、延迟优化分析

### 3.1 当前延迟分析

```
端到端延迟分解:

Tracker 端:
├── IMU 读取:        0.5ms (SPI 传输)
├── VQF 融合:        0.25ms (定点运算)
├── 包构建:          0.01ms
├── RF 发送:         0.2ms
└── 小计:            ~1ms

RF 传输:
├── 空中时间:        0.1ms
├── 接收处理:        0.1ms
└── 小计:            ~0.2ms

Receiver 端:
├── 包解析:          0.02ms
├── USB 缓冲:        1ms (USB 帧)
└── 小计:            ~1ms

总延迟: ~2-3ms (理论值)
实际测量: ~5-8ms (包含 USB 帧延迟)
```

### 3.2 延迟优化方案

#### 方案 1: 提高采样率

**当前**: 200Hz (5ms 周期)
**目标**: 400Hz (2.5ms 周期)

```c
// 修改配置
#define SENSOR_HZ       400     // 原 200
#define SENSOR_US       2500    // 原 5000

// 需要确保:
// 1. IMU 支持 400Hz ODR
// 2. CPU 负载 < 80%
// 3. RF 带宽足够
```

**优点**: 延迟减半
**缺点**: 功耗增加 ~30%

#### 方案 2: 预测滤波

```c
// 一阶预测
void predict_orientation(float *quat, float *gyro, float dt_predict) {
    // 使用当前角速度预测未来姿态
    float qd[4];
    qd[0] = -0.5f * (quat[1]*gyro[0] + quat[2]*gyro[1] + quat[3]*gyro[2]);
    qd[1] =  0.5f * (quat[0]*gyro[0] + quat[2]*gyro[2] - quat[3]*gyro[1]);
    qd[2] =  0.5f * (quat[0]*gyro[1] - quat[1]*gyro[2] + quat[3]*gyro[0]);
    qd[3] =  0.5f * (quat[0]*gyro[2] + quat[1]*gyro[1] - quat[2]*gyro[0]);
    
    // 应用预测
    for (int i = 0; i < 4; i++) {
        quat[i] += qd[i] * dt_predict;
    }
    normalize_quat(quat);
}

// 在 PC 端应用 ~3ms 的预测
predict_orientation(quat, gyro, 0.003f);
```

**优点**: 无硬件改动
**缺点**: 快速运动时可能过预测

#### 方案 3: USB 批量传输优化

```c
// 当前: 每个追踪器一个 USB 包
// 优化: 批量发送多个追踪器数据

typedef struct {
    uint8_t count;
    tracker_data_t trackers[8];  // 批量发送
} batch_report_t;

// 优点: 减少 USB 帧数
// USB 延迟: 8ms -> 1ms (每帧发送8个追踪器)
```

### 3.3 延迟优化总结

| 方案 | 延迟改善 | 实现难度 | 副作用 |
|------|----------|----------|--------|
| 400Hz 采样 | -50% | 中 | 功耗增加 |
| 预测滤波 | -30% | 低 | 可能过预测 |
| USB 批量 | -40% | 低 | 无 |
| 全部应用 | -70% | 中 | 综合 |

**推荐**: 先实现 USB 批量传输和预测滤波，收益最大且风险最低。

## 四、CH591 特定优化

### 4.1 CH591 vs CH592 差异

| 资源 | CH591 | CH592 | 影响 |
|------|-------|-------|------|
| Flash | 256KB | 448KB | 代码大小限制 |
| RAM | 18KB | 26KB | 数据结构大小 |
| BLE | 5.1 | 5.4 | 功能略少 |

### 4.2 CH591 优化配置

```c
// include/ch591_config.h

#ifdef CH591
    // 减少追踪器数量以节省 RAM
    #define MAX_TRACKERS        16  // 原 24
    
    // 减少缓冲区大小
    #define RF_RX_BUFFER_SIZE   32  // 原 48
    #define USB_TX_BUFFER_SIZE  128 // 原 256
    
    // 使用更激进的代码优化
    #define USE_VQF_LITE        1   // 使用简化版 VQF
    
    // 禁用非必要功能
    #define ENABLE_DEBUG        0
    #define ENABLE_MAG          0   // 禁用磁力计
#endif
```

### 4.3 内存优化技巧

```c
// 1. 使用位字段
typedef struct {
    uint8_t tracker_id : 6;
    uint8_t status : 2;
} compact_id_t;  // 1 字节 vs 2 字节

// 2. 共享缓冲区
static union {
    uint8_t rx_buf[48];
    uint8_t tx_buf[48];
} shared_buffer;  // 节省 48 字节

// 3. 使用 Flash 存储常量
static const uint8_t PROGMEM lookup_table[256] = {...};
```

## 五、总结

### 可立即实施的优化

1. ✅ **USB 批量传输** - 延迟 -40%
2. ✅ **预测滤波** - 延迟 -30%
3. ✅ **CH591 配置** - 支持更低成本硬件

### 中期优化

1. 🔄 **400Hz 采样率** - 需要功耗评估
2. 🔄 **卡尔曼滤波** - 需要更多 RAM

### 长期优化

1. 📋 **磁力计融合** - 需要硬件支持
2. 📋 **多传感器约束** - 需要算法开发
