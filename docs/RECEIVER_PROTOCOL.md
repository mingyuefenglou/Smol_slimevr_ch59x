# SlimeVR CH592 接收端协议文档

## 概述

本实现兼容 SlimeVR-Tracker-nRF-Receiver 协议，允许 CH592 接收端与 nRF 发射端互操作。

## 数据包格式

### 通用包结构 (16 字节)

```
| Byte 0  | Byte 1  | Bytes 2-15 |
| type    | id      | data       |
```

### 数据包类型

| Type | 名称 | 描述 |
|------|------|------|
| 0 | Info | 设备信息（电池、温度、固件版本等）|
| 1 | Quat+Accel | 完整精度四元数 + 加速度 |
| 2 | Compact | 压缩数据包（最常用）|
| 3 | Status | 服务器状态 |
| 4 | Quat+Mag | 完整精度四元数 + 磁力计 |
| 255 | Register | 设备注册（接收端 → 主机）|

### Type 0: Info 包

```
|b0  |b1 |b2   |b3  |b4    |b5  |b6    |b7    |b8    |b9    |b10-11  |b12   |b13   |b14   |b15 |
|type|id |proto|batt|batt_v|temp|brd_id|mcu_id|imu_id|mag_id|fw_date |major |minor |patch |rssi|
```

### Type 1: 完整精度四元数 + 加速度

```
|b0  |b1 |b2-3|b4-5|b6-7|b8-9|b10-11|b12-13|b14-15|
|type|id |q0  |q1  |q2  |q3  |a0    |a1    |a2    |
```
- 四元数: Q15 定点格式 (-1.0 ~ 0.99997)
- 加速度: 0.01g 单位

### Type 2: 压缩数据包

```
|b0  |b1 |b2  |b3    |b4  |b5-11   |b12-13|b14-15|
|type|id |batt|batt_v|temp|q_buf[7]|a0-a2 |rssi  |
```
- q_buf: 7 字节压缩四元数（见下文）

### Type 255: 注册包

```
|b0  |b1 |b2-7    |b8-15    |
|type|id |addr[6] |reserved |
```

## 扩展包格式

### 20 字节包 (+ CRC32)

```
| 0-15       | 16-19 |
| 基本包     | CRC32 |
```

CRC32 多项式: K=4, P=2, 初值 0x93A409EB

### 21 字节包 (+ CRC32 + 序列号)

```
| 0-15       | 16-19 | 20  |
| 基本包     | CRC32 | seq |
```

## 四元数压缩算法

### 压缩 (4 float → 7 bytes)

1. 找到最大分量索引 (0-3)
2. 对其余 3 个分量进行符号调整
3. 每个分量编码为 18 位定点数
4. 打包: 2 位索引 + 3×18 位 = 56 位 = 7 字节

```c
// 压缩后格式
| bit 55-54 | bit 53-36 | bit 35-18 | bit 17-0 |
| max_idx   | comp_a    | comp_b    | comp_c   |
```

### 解压 (7 bytes → 4 float)

1. 解包获取索引和 3 个分量
2. 将 18 位定点数转为浮点
3. 通过 `sqrt(1 - a² - b² - c²)` 计算最大分量
4. 按索引重建四元数

## 配对协议

### 配对包格式 (8 字节, Pipe 0)

```
| b0       | b1    | b2-7    |
| checksum | stage | addr[6] |
```

### 配对流程

```
Tracker                    Receiver
   |                          |
   |--[stage=0, addr]-------->|  配对请求
   |                          |
   |<--[checksum, id]---------|  分配 ID
   |                          |
   |--[stage=2]-------------->|  确认
   |                          |
```

### Checksum 计算

```c
checksum = crc8_ccitt(0x07, addr, 6);
if (checksum == 0) checksum = 8;  // 避免零值
```

## RF 地址生成

### 配对地址 (固定)

```c
base_addr_0 = {0x62, 0x39, 0x8A, 0xF2}
prefix      = {0xFE, 0xFF, 0x29, 0x27, 0x09, 0x02, 0xB2, 0xD6}
```

### 运行地址 (基于设备 MAC)

```c
// 从设备 MAC 生成
for (i = 0; i < 4; i++) {
    addr_buffer[i] = mac[i];
    addr_buffer[i + 4] = mac[i] + mac[4];
}
for (i = 0; i < 8; i++) {
    addr_buffer[i + 8] = mac[5] + i;
}

// 避免无效地址
for (i = 0; i < 16; i++) {
    if (addr_buffer[i] == 0x00 || addr_buffer[i] == 0x55 || addr_buffer[i] == 0xAA)
        addr_buffer[i] += 8;
}
```

## USB HID 报告

### 报告格式 (64 字节)

```
| 0-15     | 16-31    | 32-47    | 48-63    |
| Packet 0 | Packet 1 | Packet 2 | Packet 3 |
```

每次 USB 传输发送 4 个 16 字节数据包：
- 优先发送数据包（从 FIFO）
- 剩余位置填充注册包（Type 255）

### 轮询间隔

- USB HID 中断端点: 1ms
- 每秒最多 4000 个数据包

## CH592 内存使用

### RAM 分配

| 模块 | 大小 | 说明 |
|------|------|------|
| tracker_info[] | 384B | 24 个追踪器 × 16 字节 |
| tracker_addrs[] | 192B | 24 个追踪器 × 8 字节 |
| pkt_fifo[] | 512B | 32 个包 × 16 字节 |
| sequences[] | 24B | 序列号跟踪 |
| usb_report | 64B | USB 报告缓冲 |
| 其他 | ~100B | 状态变量 |
| **总计** | ~1.3KB | |

### Flash 使用

| 模块 | 大小 |
|------|------|
| 代码 | ~40KB |
| USB 描述符 | ~200B |
| 常量 | ~1KB |
| **总计** | ~42KB |

## 兼容性

本实现与以下设备兼容：
- SlimeVR-Tracker-nRF 发射端
- 任何实现相同协议的 ESB 2Mbps 设备

## 限制

| 项目 | CH592 限制 | nRF52840 |
|------|-----------|----------|
| 最大追踪器数 | 24 | 256 |
| RAM | 26KB | 256KB |
| USB 速度 | Full-Speed | Full-Speed |
