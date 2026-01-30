# SlimeVR 官方软件兼容性分析
# SlimeVR Official Software Compatibility Analysis

## 1. 执行摘要 / Executive Summary

| 项目 | 状态 | 说明 |
|------|------|------|
| **直接兼容** | ⚠️ 需要适配 | 官方软件期望 WiFi/UDP 协议 |
| **USB HID 连接** | ✅ 可实现 | 需要修改接收器协议 |
| **完整功能** | 🔄 进行中 | 需要协议层适配 |

## 2. SlimeVR 官方软件架构

### 2.1 官方系统架构

```
┌────────────────────────────────────────────────────────────┐
│                    SlimeVR 官方系统                         │
├────────────────────────────────────────────────────────────┤
│                                                            │
│   ESP8266/ESP32                    SlimeVR Server          │
│   追踪器                           (Java 应用)              │
│   ┌─────────┐                     ┌─────────────┐          │
│   │  IMU    │   WiFi / UDP        │  服务端     │          │
│   │  ESP    │ ──────────────────> │  6969 端口  │          │
│   │  电池   │   SlimeVR 协议       │             │          │
│   └─────────┘                     └──────┬──────┘          │
│                                          │                 │
│                                          │ OSC/VMC         │
│                                          ▼                 │
│                                   ┌─────────────┐          │
│                                   │  VRChat     │          │
│                                   │  VSeeFace   │          │
│                                   │  SteamVR    │          │
│                                   └─────────────┘          │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### 2.2 官方协议 (UDP)

SlimeVR 使用 UDP 协议在端口 **6969** 通信:

```c
// 官方数据包格式
typedef struct {
    uint32_t packet_type;      // 包类型
    uint64_t packet_id;        // 包序号
    uint8_t  sensor_id;        // 传感器 ID
    uint8_t  data_type;        // 数据类型
    float    quaternion[4];    // 四元数
    float    acceleration[3];  // 加速度
    // ... 其他字段
} slimevr_udp_packet_t;

// 包类型
#define PKT_HEARTBEAT       0
#define PKT_ROTATION        1
#define PKT_GYROSCOPE       2
#define PKT_HANDSHAKE       3
#define PKT_ACCEL           4
// ...
```

## 3. CH592 系统架构 (当前)

```
┌────────────────────────────────────────────────────────────┐
│                    CH592 RF 系统                           │
├────────────────────────────────────────────────────────────┤
│                                                            │
│   CH592                            CH592                   │
│   追踪器                           接收器                   │
│   ┌─────────┐                     ┌─────────────┐          │
│   │  IMU    │   2.4GHz RF         │  USB HID    │──> PC   │
│   │  CH592  │ ──────────────────> │  设备       │          │
│   │  电池   │   RF Ultra 协议      │             │          │
│   └─────────┘                     └─────────────┘          │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

## 4. 兼容性问题分析

### 4.1 协议差异

| 方面 | SlimeVR 官方 | CH592 当前 | 差异 |
|------|-------------|-----------|------|
| 传输方式 | WiFi UDP | 2.4GHz RF + USB HID | 完全不同 |
| 服务端发现 | mDNS | 无 | 需要适配 |
| 数据格式 | SlimeVR UDP 协议 | RF Ultra 协议 | 需要转换 |
| 端口 | UDP 6969 | USB HID | 需要桥接 |

### 4.2 兼容方案

#### 方案 A: USB-UDP 桥接 (推荐)

在接收器端或 PC 端运行桥接程序:

```
CH592           CH592              桥接程序              SlimeVR
追踪器 ──RF──> 接收器 ──USB HID──> (Python/Go) ──UDP──> Server
```

**优点:**
- 不需要修改 SlimeVR 服务端
- 接收器固件改动小
- 可在 PC 端实现全部逻辑

**缺点:**
- 需要额外运行桥接程序

**实现代码示例:**

```python
#!/usr/bin/env python3
"""
SlimeVR USB-UDP Bridge
USB HID 到 SlimeVR UDP 协议桥接
"""

import hid
import socket
import struct
import time

# USB HID 设备
USB_VID = 0x1209
USB_PID = 0x5634

# SlimeVR 服务端
SLIMEVR_HOST = "127.0.0.1"
SLIMEVR_PORT = 6969

class SlimeVRBridge:
    def __init__(self):
        self.hid_device = None
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.packet_id = 0
        
    def connect_usb(self):
        """连接 USB HID 设备"""
        try:
            self.hid_device = hid.device()
            self.hid_device.open(USB_VID, USB_PID)
            self.hid_device.set_nonblocking(True)
            print(f"已连接: {self.hid_device.get_manufacturer_string()}")
            return True
        except Exception as e:
            print(f"USB 连接失败: {e}")
            return False
    
    def parse_rf_packet(self, data):
        """解析 RF Ultra 数据包"""
        if len(data) < 12:
            return None
        
        # RF Ultra 格式:
        # [header:1][quat:8][aux:2][crc:1]
        header = data[0]
        pkt_type = (header >> 6) & 0x03
        tracker_id = header & 0x3F
        
        if pkt_type == 0:  # 四元数数据
            # 解析 Q15 四元数
            qw = struct.unpack('<h', bytes(data[1:3]))[0] / 32768.0
            qx = struct.unpack('<h', bytes(data[3:5]))[0] / 32768.0
            qy = struct.unpack('<h', bytes(data[5:7]))[0] / 32768.0
            qz = struct.unpack('<h', bytes(data[7:9]))[0] / 32768.0
            
            # 解析辅助数据
            aux = struct.unpack('<H', bytes(data[9:11]))[0]
            accel_z = (aux & 0x0FFF) - 2048  # 12-bit signed
            battery = (aux >> 12) * 100 // 15  # 4-bit, 0-100%
            
            return {
                'tracker_id': tracker_id,
                'quaternion': [qw, qx, qy, qz],
                'accel_z': accel_z,
                'battery': battery
            }
        return None
    
    def build_slimevr_packet(self, data):
        """构建 SlimeVR UDP 数据包"""
        # 简化的旋转数据包
        # 参考: https://github.com/SlimeVR/SlimeVR-Tracker-ESP
        
        pkt = bytearray()
        
        # 包头
        pkt.extend(struct.pack('<I', 1))  # packet_type = ROTATION
        pkt.extend(struct.pack('<Q', self.packet_id))  # packet_id
        self.packet_id += 1
        
        # 传感器 ID
        pkt.append(data['tracker_id'])
        
        # 数据类型
        pkt.append(1)  # ROTATION_DATA
        
        # 四元数 (float)
        for q in data['quaternion']:
            pkt.extend(struct.pack('<f', q))
        
        # 精度指示
        pkt.append(1)  # accuracy
        
        return bytes(pkt)
    
    def send_to_slimevr(self, packet):
        """发送到 SlimeVR 服务端"""
        try:
            self.udp_socket.sendto(packet, (SLIMEVR_HOST, SLIMEVR_PORT))
        except Exception as e:
            print(f"UDP 发送失败: {e}")
    
    def run(self):
        """主循环"""
        if not self.connect_usb():
            return
        
        print("桥接运行中... 按 Ctrl+C 停止")
        
        try:
            while True:
                # 读取 USB HID 数据
                data = self.hid_device.read(64, timeout_ms=10)
                
                if data:
                    parsed = self.parse_rf_packet(data)
                    if parsed:
                        packet = self.build_slimevr_packet(parsed)
                        self.send_to_slimevr(packet)
                
                time.sleep(0.001)  # 1ms
                
        except KeyboardInterrupt:
            print("\n停止桥接")
        finally:
            if self.hid_device:
                self.hid_device.close()

if __name__ == '__main__':
    bridge = SlimeVRBridge()
    bridge.run()
```

#### 方案 B: 修改接收器为虚拟串口

将接收器改为 USB CDC (虚拟串口)，更容易与现有工具集成:

```c
// 接收器输出 JSON 格式
// {"id":0,"q":[0.707,0.0,0.707,0.0],"a":0.98,"b":85}
```

#### 方案 C: 修改 SlimeVR 服务端

Fork SlimeVR 服务端，添加 USB HID 支持:

```java
// SlimeVR Server 修改
public class USBHIDTracker implements TrackerSource {
    @Override
    public void start() {
        // 使用 hid4java 库连接 USB 设备
        HidDevice device = services.openDevice(VID, PID);
        // ...
    }
}
```

## 5. 推荐方案

### 5.1 短期方案 (立即可用)

使用 **方案 A: USB-UDP 桥接程序**

1. 安装 Python hidapi 库:
   ```bash
   pip install hidapi
   ```

2. 运行桥接程序:
   ```bash
   python slimevr_bridge.py
   ```

3. 启动 SlimeVR 软件

4. 软件应识别追踪器

### 5.2 中期方案 (推荐)

1. 将桥接程序打包为独立可执行文件
2. 随固件一起发布
3. 或提交 PR 到 SlimeVR 官方仓库

### 5.3 长期方案

1. 与 SlimeVR 团队合作
2. 将 USB HID 支持合并到官方软件
3. 或开发独立的 "SlimeVR CH592 Edition"

## 6. USB 描述符兼容性

当前 USB 描述符需要调整以确保兼容性:

```c
// 推荐的 VID/PID 配置
#define USB_VID     0x1209  // pid.codes (开源硬件)
#define USB_PID     0x5634  // 申请专用 PID

// 或使用 SlimeVR 官方 VID/PID (需授权)
// #define USB_VID     0x1915  // Nordic (如果使用 nRF)
// #define USB_PID     0x520F  // SlimeVR
```

## 7. 测试步骤

### 7.1 验证 USB 连接

```bash
# Linux
lsusb | grep 1209

# Windows (PowerShell)
Get-PnpDevice | Where-Object { $_.InstanceId -like "*VID_1209*" }
```

### 7.2 验证桥接

```bash
# 运行桥接程序并观察输出
python slimevr_bridge.py --debug

# 应看到类似:
# [INFO] USB 已连接
# [DEBUG] 收到追踪器 0: q=[0.707, 0.0, 0.707, 0.0]
# [DEBUG] 发送到 SlimeVR: 28 bytes
```

### 7.3 验证 SlimeVR

1. 打开 SlimeVR 软件
2. 进入 "设置" -> "追踪器"
3. 应显示已连接的追踪器

## 8. 结论

| 问题 | 答案 |
|------|------|
| 能否直接连接 SlimeVR? | ❌ 不能直接连接 (协议不同) |
| 能否通过桥接连接? | ✅ 可以 (推荐方案) |
| 开发工作量 | 中等 (1-2 周) |
| 长期可维护性 | 良好 (可合并到官方) |

## 9. 后续步骤

1. ✅ 完成 USB HID 基础实现 (已完成)
2. 🔄 开发桥接程序
3. 📝 提交 PR 到 SlimeVR 官方
4. 🚀 发布独立工具
