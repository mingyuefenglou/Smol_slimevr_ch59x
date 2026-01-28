# SlimeVR CH59X v0.5.0 产品化发布说明

## 概述

v0.5.0 是 SlimeVR CH59X 固件的产品化整合版本，完成了从 v0.4.22 到 v0.5.0 的全部里程碑：

| 版本 | 主要内容 | 状态 |
|------|---------|------|
| v0.4.22 | AllInOne优化指南实施 (P0/P1/P2共10项修复) | ✅ |
| v0.4.23 | 丢包统计 + 静止检测优化 | ✅ |
| v0.4.24 | WOM深睡眠唤醒闭环 | ✅ |
| v0.4.25 | Receiver packet翻译层集成 | ✅ |
| v0.5.0 | retained state + 诊断系统 + 文档化 | ✅ |

---

## 核心参数定版

### 10 Tracker @200Hz 时隙预算

```
5ms 超帧 (200Hz)
├── Sync Slot:     250us  (Receiver beacon)
├── Hop/Guard:     250us  (跳频保护)
├── Tracker Slots: 4000us (10 × 400us)
│   ├── Align:     30us
│   ├── TX:        160us
│   ├── RX/ACK:    110us
│   └── Protect:   100us
└── Tail Guard:    500us  (帧尾保护)
```

### 关键配置

| 参数 | 值 | 说明 |
|------|------|------|
| MAX_TRACKERS | 10 | 最大tracker数 |
| RF_SUPERFRAME_US | 5000 | 固定200Hz |
| RF_DATA_SLOT_US | 400 | 单tracker时隙 |
| STATIONARY_TX_DIVIDER | 4 | 静止时50Hz |
| AUTO_SLEEP_TIMEOUT_MS | 60000 | 60秒自动睡眠 |

---

## 新增功能

### 1. Retained State (唤醒快速恢复)

**文件**: `include/retained_state.h`, `src/hal/retained_state.c`

**功能**:
- 保存四元数和陀螺仪偏置到Flash
- 唤醒后2秒内姿态收敛
- 60秒有效期，超时后忽略

**API**:
```c
retained_init();                         // 初始化
retained_save(quat, gyro_bias);          // 保存状态
retained_restore(quat, gyro_bias);       // 恢复状态
retained_is_valid();                     // 检查有效性
```

### 2. WOM深睡眠唤醒

**文件**: `src/main_tracker.c`, `src/sensor/imu_interface.c`

**模式**:
- **Deep Sleep**: 已配对+静止60秒 → Shutdown模式 → WOM唤醒
- **Light Sleep**: 未配对/丢失同步 → Halt模式 → 按键唤醒

**IMU支持**:
- ICM-45686/42688: WOM中断
- BMI270: Motion detect
- LSM6DSV/DSR: Wake-up中断

### 3. Packet语义对齐 (nRF兼容)

**文件**: `include/slime_packet.h`, `src/usb/slime_packet.c`

| Packet | 内容 | 频率 |
|--------|------|------|
| packet0 | 版本/板型/IMU/磁力计/电压/温度 | 1Hz |
| packet1 | 全精度quat+accel (float) | 200Hz |
| packet2 | 压缩quat+accel+batt/temp/rssi | 200Hz |
| packet3 | 状态flags (tracker_status) | 5Hz |

### 4. 诊断统计系统

**文件**: `include/diagnostics.h`, `src/hal/diagnostics.c`

**统计项**:
- Per-tracker: 丢包率/CRC错误/超时/重传
- RSSI: 最小/最大/平均
- 帧统计: 超帧数/数据包数

---

## 验收测试清单

### 稳定性测试
- [ ] 10 trackers连续运行30分钟无掉线
- [ ] 干扰环境下不假死（2ms超时自愈）
- [ ] 帧间隔稳定5000us（±50us抖动）

### 功耗测试
- [ ] 运动时功耗 < 15mA (200Hz发送)
- [ ] 静止时功耗 < 5mA (50Hz发送)
- [ ] 深睡眠功耗 < 10uA

### 唤醒测试
- [ ] WOM唤醒响应 < 500ms
- [ ] 唤醒后姿态收敛 < 2s
- [ ] 未配对时按键唤醒正常

### 兼容性测试
- [ ] SlimeVR Server正确显示电量
- [ ] SlimeVR Server正确显示状态
- [ ] 诊断报告可正确解析

---

## 文件清单

### 新增文件 (v0.4.22-v0.5.0)

```
include/
├── retained_state.h     # v0.5.0 状态保存
├── diagnostics.h        # v0.5.0 诊断统计
└── slime_packet.h       # v0.4.22 packet翻译

src/
├── hal/
│   ├── retained_state.c # v0.5.0 状态保存实现
│   └── diagnostics.c    # v0.5.0 诊断统计实现
└── usb/
    └── slime_packet.c   # v0.4.22 packet翻译实现
```

### 修改文件

```
include/
├── config.h            # MAX_TRACKERS=10, AUTO_SLEEP=60s
├── rf_protocol.h       # 扩展flags, tracker_info_t统计
├── imu_interface.h     # WOM API
└── vqf_ultra.h         # set_quat API

src/
├── main_tracker.c      # WOM睡眠+retained state
├── main_receiver.c     # packet翻译层集成
├── rf/rf_transmitter.c # 静止降速+低功耗等待
├── rf/rf_receiver.c    # 固定5000us超帧
├── rf/rf_hw.c          # 超时保护
├── sensor/imu_interface.c  # WOM实现
└── sensor/fusion/vqf_ultra.c # set_quat实现
```

---

## 已知限制

1. **Flash存储**: retained state使用4KB Flash区域
2. **RAM占用**: 诊断统计约增加200字节
3. **WOM精度**: 不同IMU的WOM阈值精度有差异
4. **兼容性**: packet语义需要SlimeVR Server 0.12+

---

## 下一步计划

- v0.5.1: 长稳测试问题修复
- v0.5.2: OTA固件更新优化
- v0.6.0: BLE模式支持

---

## 累计修复统计

| 版本范围 | Bug修复数 | 新增功能 |
|---------|----------|---------|
| v0.4.14-v0.4.21 | 39 | - |
| v0.4.22 | 10 | packet翻译层 |
| v0.4.23 | 3 | 丢包统计 |
| v0.4.24 | 5 | WOM睡眠 |
| v0.4.25 | 2 | HID packet |
| v0.5.0 | 4 | retained+诊断 |
| **总计** | **63** | **5** |
