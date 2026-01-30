# SlimeVR CH59X 进一步优化建议与实施
# SlimeVR CH59X Further Optimization Suggestions & Implementation

## 概述

本文档分析现有固件的优化空间，并提出具体实施方案。

---

## 1. RF 协议优化分析

### 1.1 现有 RF Ultra v1 协议

| 特性 | 当前状态 | 分析 |
|------|----------|------|
| 包大小 | 12 字节 | 仍有压缩空间 |
| 四元数编码 | 4x int16 (8B) | 可用 smallest-three (6B) |
| CRC | CRC8 (无纠错) | 可加 FEC |
| 静止优化 | 无 | 可用增量编码 |
| 多追踪器 | 独立包 | 可聚合 |

### 1.2 RF Ultra v2 优化 (已实施)

**新增文件**: `src/rf/rf_ultra_v2.c`

| 优化项 | v1 | v2 | 提升 |
|--------|-----|-----|------|
| 完整包大小 | 12B | 10B | 17% |
| 增量包大小 | N/A | 6B | 50% |
| 静止带宽 | 2400 B/s | 300 B/s | 87.5% |
| 纠错能力 | 0 bit | 1 bit | +100% |
| 多追踪器聚合 | 1:1 | 4:1 | 75% |

**v2 核心优化:**

1. **Smallest-Three 四元数压缩**
   - 丢弃绝对值最大的分量
   - 4x16bit → 3x16bit + 2bit index = 6.25B
   - 接收端通过归一化恢复

2. **增量编码**
   - 运动小时只传输变化量
   - 完整包 10B → 增量包 6B
   - 每 10 个增量包发 1 个完整包 (同步)

3. **自适应发送速率**
   - 高运动: 200Hz (全速)
   - 低运动: 100Hz (半速)
   - 静止: 50Hz (四分之一)

4. **智能跳频**
   - 监控各通道 RSSI
   - 自动选择最佳通道
   - 避开 WiFi 干扰

5. **包聚合 (接收器)**
   - 最多聚合 4 个追踪器数据
   - USB 单次传输多追踪器
   - 降低 USB 轮询开销

---

## 2. 功耗优化

### 2.1 当前功耗分析

| 组件 | 电流 | 占比 | 可优化空间 |
|------|------|------|------------|
| MCU 活动 | 8.0 mA | 63% | 可降低时钟 |
| RF TX | 3.0 mA (平均) | 24% | 可降低功率/频率 |
| IMU | 0.65 mA | 5% | 可用低功耗模式 |
| 其他 | 1.0 mA | 8% | 固定 |
| **总计** | **12.65 mA** | 100% | |

### 2.2 优化措施

**A. MCU 时钟动态调整**
```c
// 静止时降低系统时钟
void power_optimize_idle(void) {
    // 60MHz → 32MHz (省 ~3mA)
    SetSysClock(CLK_SOURCE_PLL_32MHz);
}

// 活动时恢复
void power_optimize_active(void) {
    SetSysClock(CLK_SOURCE_PLL_60MHz);
}
```

**B. RF 自适应功率**
```c
// 根据 RSSI 调整发射功率
void rf_adaptive_power(int8_t rssi) {
    if (rssi > -50) {
        // 信号强，降低功率
        rf_set_tx_power(RF_TX_POWER_0DBM);  // 省 ~2mA
    } else if (rssi < -70) {
        // 信号弱，最大功率
        rf_set_tx_power(RF_TX_POWER_4DBM);
    }
}
```

**C. IMU 低功耗模式**
```c
// 静止时 IMU 降频
void imu_low_power_mode(void) {
    // 200Hz → 50Hz (省 ~0.3mA)
    imu_set_odr(50);
}
```

### 2.3 优化后功耗预估

| 模式 | 电流 | 150mAh 续航 |
|------|------|-------------|
| 原始 | 12.65 mA | 11.9 小时 |
| 优化 (活动) | 10.5 mA | 14.3 小时 |
| 优化 (静止) | 5.0 mA | 30.0 小时 |
| 混合 (70%活动) | 8.9 mA | 16.9 小时 |

---

## 3. 精度优化

### 3.1 陀螺仪噪音 (已实施)

**文件**: `src/sensor/gyro_noise_filter.c`

| 滤波层 | 方法 | 效果 |
|--------|------|------|
| 硬件 | IMU 内部 LPF | 预滤波 |
| 第1层 | 偏差校正 | 消除静态偏差 |
| 第2层 | 中值滤波 | 去尖峰 |
| 第3层 | 移动平均 | 平滑 |
| 第4层 | 卡尔曼 | 自适应噪声 |
| 第5层 | ZUPT | 零速更新 |
| 第6层 | 死区 | 微小噪声 |

### 3.2 传感器融合优化建议

**A. VQF 参数调优**
```c
// 根据使用场景调整
#define VQF_TAU_ACC_FAST    1.0f    // 快速响应 (舞蹈)
#define VQF_TAU_ACC_NORMAL  3.0f    // 普通 (VR 游戏)
#define VQF_TAU_ACC_STABLE  5.0f    // 稳定 (站立)
```

**B. 磁力计融合 (可选)**
- 添加磁力计可消除偏航漂移
- 需要磁干扰检测
- 室内效果可能不佳

**C. 运动自适应**
```c
// 高运动时信任陀螺仪
// 静止时信任加速度计
float adapt_beta(float motion_level) {
    if (motion_level > 0.8f) return 0.02f;  // 低修正
    if (motion_level < 0.2f) return 0.2f;   // 高修正
    return 0.1f;  // 默认
}
```

---

## 4. 延迟优化

### 4.1 当前延迟分析

| 阶段 | 延迟 | 说明 |
|------|------|------|
| IMU 采样 | 5 ms | ODR=200Hz |
| 融合算法 | 0.4 ms | VQF |
| RF TX | 0.5 ms | 12B @ 2Mbps |
| RF 空中 | 0.05 ms | 约 50us |
| USB HID | 1 ms | 轮询间隔 |
| **总端到端** | **~7 ms** | |

### 4.2 优化措施

**A. 提高 ODR**
```c
// 200Hz → 400Hz (延迟 5ms → 2.5ms)
imu_set_odr(400);
```

**B. 缩短 USB 轮询**
```c
// 1ms → 0.5ms (需要 USB 高速支持)
#define USB_HID_INTERVAL    1  // 最小 1ms (USB FS)
```

**C. 中断驱动**
```c
// 使用 IMU 数据就绪中断
void IMU_INT1_IRQHandler(void) {
    // 立即读取并处理
    imu_read_and_process();
}
```

---

## 5. 代码大小优化 (CH591D 适用)

### 5.1 当前代码大小估算

| 模块 | 大小 | 说明 |
|------|------|------|
| 系统启动 | ~4 KB | 启动代码 + 向量表 |
| HAL | ~8 KB | 硬件抽象层 |
| RF | ~12 KB | RF 协议 |
| IMU | ~10 KB | 5种 IMU 驱动 |
| 融合 | ~6 KB | VQF + EKF |
| USB | ~8 KB | USB HID |
| **总计** | **~48 KB** | CH591D 最大 256KB |

### 5.2 优化措施

**A. 选择性编译 IMU 驱动**
```makefile
# 只编译需要的 IMU
ifeq ($(IMU),ICM45686)
    SENSOR_SRC += src/sensor/imu/icm45686.c
else ifeq ($(IMU),BMI270)
    SENSOR_SRC += src/sensor/imu/bmi270.c
endif
```

**B. 禁用调试代码**
```c
#ifndef DEBUG_ENABLE
    #define DEBUG_PRINTF(...)   // 空
#else
    #define DEBUG_PRINTF(...)   printf(__VA_ARGS__)
#endif
```

**C. 优化级别**
```makefile
# 使用 -Os 优化大小
OPT = -Os
```

---

## 6. 实施优先级

| 优先级 | 优化项 | 预期收益 | 工作量 |
|--------|--------|----------|--------|
| 🔴 高 | RF v2 (smallest-three) | 带宽 -40% | 中 |
| 🔴 高 | 增量编码 | 静止带宽 -80% | 中 |
| 🟠 中 | 自适应功率 | 功耗 -20% | 低 |
| 🟠 中 | MCU 动态时钟 | 功耗 -25% | 低 |
| 🟢 低 | 提高 ODR | 延迟 -2.5ms | 低 |
| 🟢 低 | 磁力计融合 | 漂移消除 | 高 |

---

## 7. 测试验证

### 7.1 RF v2 测试用例

```c
// 测试 smallest-three 压缩
void test_smallest_three(void) {
    q15_t quat_in[4] = {32767, 0, 0, 0};  // 单位四元数
    smallest_three_t st;
    q15_t quat_out[4];
    
    quat_compress_smallest_three(quat_in, &st);
    quat_decompress_smallest_three(&st, quat_out);
    
    // 验证误差 < 0.01
    for (int i = 0; i < 4; i++) {
        assert(abs(quat_in[i] - quat_out[i]) < 328);  // 1% 误差
    }
}
```

### 7.2 功耗测试方法

```
测试设备: 电流表 (uA 级精度)
测试条件:
1. 静止 60 秒, 记录平均电流
2. 轻微运动 60 秒, 记录平均电流
3. 剧烈运动 60 秒, 记录平均电流
4. 计算加权平均
```

---

## 8. 总结

### 已实施优化:
- ✅ RF Ultra v2 (smallest-three, 增量编码, 聚合)
- ✅ 陀螺仪多级噪音滤波
- ✅ 统一 IMU 接口 (SPI/I2C)
- ✅ 简化充电检测

### 待实施优化:
- ⏳ MCU 动态时钟
- ⏳ RF 自适应功率
- ⏳ IMU 低功耗模式切换
- ⏳ 磁力计融合 (可选)

### 预期总体提升:
| 指标 | 优化前 | 优化后 | 提升 |
|------|--------|--------|------|
| 续航 | 12 小时 | 17+ 小时 | 40%+ |
| 静态噪音 | 0.02°/s | 0.005°/s | 75% |
| RF 带宽 | 2400 B/s | 1000 B/s | 58% |
| 端到端延迟 | 7 ms | 5 ms | 29% |

---

*最后更新: 2026-01-12*
