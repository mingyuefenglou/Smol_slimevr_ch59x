#!/usr/bin/env python3
"""
SlimeVR USB-UDP Bridge for CH592 Receiver
CH592 接收器到 SlimeVR 服务端的桥接程序

Usage:
    python slimevr_bridge.py [--debug]

Requirements:
    pip install hidapi

Author: NiNi
Version: 1.0.0
"""

import argparse
import struct
import socket
import time
import sys
import threading
from typing import Optional, Dict, List

try:
    import hid
except ImportError:
    print("请安装 hidapi: pip install hidapi")
    sys.exit(1)

#==============================================================================
# 配置 / Configuration
#==============================================================================

# USB 设备
USB_VID = 0x1209  # pid.codes VID
USB_PID = 0x5634  # SlimeVR CH592 PID

# SlimeVR 服务端
SLIMEVR_HOST = "127.0.0.1"
SLIMEVR_PORT = 6969

# SlimeVR 协议
PKT_HEARTBEAT = 0
PKT_ROTATION = 1
PKT_GYROSCOPE = 2
PKT_HANDSHAKE = 3
PKT_ACCEL = 4
PKT_PING = 10
PKT_PONG = 11
PKT_BATTERY = 12
PKT_TAP = 13
PKT_RESET_REASON = 14
PKT_SENSOR_INFO = 15
PKT_ROTATION_2 = 16
PKT_ROTATION_DATA = 17
PKT_MAGNETOMETER_ACCURACY = 18
PKT_SIGNAL_STRENGTH = 19
PKT_TEMPERATURE = 20
PKT_FEATURE_FLAGS = 22
PKT_BUNDLE = 100

# 调试
DEBUG = False

#==============================================================================
# 日志 / Logging
#==============================================================================

def log_info(msg: str):
    print(f"[INFO] {msg}")

def log_error(msg: str):
    print(f"[ERROR] {msg}", file=sys.stderr)

def log_debug(msg: str):
    if DEBUG:
        print(f"[DEBUG] {msg}")

#==============================================================================
# 数据包解析 / Packet Parsing
#==============================================================================

def parse_rf_ultra_packet(data: bytes) -> Optional[Dict]:
    """
    解析 RF Ultra 数据包 (12 bytes)
    
    格式:
        [0]     Header: type(2) | tracker_id(6)
        [1-8]   Quaternion: w,x,y,z as int16 (8 bytes)
        [9-10]  Aux: accel_z(12bit) | battery(4bit)
        [11]    CRC8
    """
    if len(data) < 12:
        return None
    
    header = data[0]
    pkt_type = (header >> 6) & 0x03
    tracker_id = header & 0x3F
    
    if pkt_type == 0:  # 四元数数据
        # 解析 Q15 四元数 (-1.0 to +0.99997)
        qw = struct.unpack('<h', bytes(data[1:3]))[0] / 32768.0
        qx = struct.unpack('<h', bytes(data[3:5]))[0] / 32768.0
        qy = struct.unpack('<h', bytes(data[5:7]))[0] / 32768.0
        qz = struct.unpack('<h', bytes(data[7:9]))[0] / 32768.0
        
        # 解析辅助数据
        aux = struct.unpack('<H', bytes(data[9:11]))[0]
        accel_z_raw = aux & 0x0FFF
        if accel_z_raw > 2047:
            accel_z_raw -= 4096
        accel_z = accel_z_raw / 1000.0  # mg -> g
        battery = ((aux >> 12) & 0x0F) * 100 // 15  # 4-bit -> 0-100%
        
        return {
            'type': 'rotation',
            'tracker_id': tracker_id,
            'quaternion': [qw, qx, qy, qz],
            'accel_z': accel_z,
            'battery': battery
        }
    
    elif pkt_type == 1:  # 设备信息
        return {
            'type': 'device_info',
            'tracker_id': tracker_id
        }
    
    elif pkt_type == 2:  # 状态
        return {
            'type': 'status',
            'tracker_id': tracker_id
        }
    
    return None

#==============================================================================
# SlimeVR 协议构建 / SlimeVR Protocol Builder
#==============================================================================

class SlimeVRProtocol:
    """SlimeVR UDP 协议构建器"""
    
    def __init__(self):
        self.packet_id = 0
        self.mac_base = bytes([0x01, 0x02, 0x03, 0x04, 0x05, 0x00])
    
    def build_handshake(self, tracker_id: int) -> bytes:
        """构建握手包"""
        pkt = bytearray()
        
        # Packet type
        pkt.extend(struct.pack('<I', PKT_HANDSHAKE))
        
        # Packet ID
        pkt.extend(struct.pack('<Q', self.packet_id))
        self.packet_id += 1
        
        # Board type (custom)
        pkt.extend(struct.pack('<I', 100))  # Custom board
        
        # IMU type
        pkt.extend(struct.pack('<I', 3))  # IMU_ICM42688
        
        # MCU type
        pkt.extend(struct.pack('<I', 100))  # Custom MCU
        
        # IMU info (3 ints)
        pkt.extend(struct.pack('<III', 0, 0, 0))
        
        # Firmware version
        pkt.extend(struct.pack('<I', 1))  # Major
        pkt.extend(struct.pack('<I', 0))  # Minor
        pkt.extend(struct.pack('<I', 0))  # Patch
        
        # Firmware build
        pkt.extend(struct.pack('<I', 1))
        
        # MAC address
        mac = bytearray(self.mac_base)
        mac[5] = tracker_id
        pkt.extend(mac)
        
        return bytes(pkt)
    
    def build_rotation(self, tracker_id: int, quat: List[float]) -> bytes:
        """构建旋转数据包"""
        pkt = bytearray()
        
        # Packet type
        pkt.extend(struct.pack('<I', PKT_ROTATION_DATA))
        
        # Packet ID
        pkt.extend(struct.pack('<Q', self.packet_id))
        self.packet_id += 1
        
        # Sensor ID
        pkt.append(tracker_id)
        
        # Data type (1 = normal rotation)
        pkt.append(1)
        
        # Quaternion (w, x, y, z as float)
        for q in quat:
            pkt.extend(struct.pack('<f', q))
        
        # Accuracy indicator
        pkt.append(1)  # High accuracy
        
        return bytes(pkt)
    
    def build_battery(self, tracker_id: int, battery_pct: int) -> bytes:
        """构建电池状态包"""
        pkt = bytearray()
        
        # Packet type
        pkt.extend(struct.pack('<I', PKT_BATTERY))
        
        # Packet ID
        pkt.extend(struct.pack('<Q', self.packet_id))
        self.packet_id += 1
        
        # Battery voltage (估算)
        voltage = 3.3 + (battery_pct / 100.0) * 0.9  # 3.3V - 4.2V
        pkt.extend(struct.pack('<f', voltage))
        
        # Battery percentage
        pkt.extend(struct.pack('<f', battery_pct / 100.0))
        
        return bytes(pkt)
    
    def build_heartbeat(self) -> bytes:
        """构建心跳包"""
        pkt = bytearray()
        pkt.extend(struct.pack('<I', PKT_HEARTBEAT))
        pkt.extend(struct.pack('<Q', self.packet_id))
        self.packet_id += 1
        return bytes(pkt)

#==============================================================================
# 桥接主类 / Bridge Main Class
#==============================================================================

class SlimeVRBridge:
    """USB HID 到 SlimeVR UDP 桥接"""
    
    def __init__(self):
        self.hid_device = None
        self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.protocol = SlimeVRProtocol()
        self.running = False
        self.connected_trackers = set()
        self.last_battery_time = {}
        self.server_addr = (SLIMEVR_HOST, SLIMEVR_PORT)
    
    def find_device(self) -> bool:
        """查找 USB HID 设备"""
        log_info("搜索 SlimeVR CH592 接收器...")
        
        for device in hid.enumerate(USB_VID, USB_PID):
            log_info(f"找到设备: {device['product_string']}")
            log_debug(f"  路径: {device['path']}")
            log_debug(f"  VID/PID: {hex(device['vendor_id'])}/{hex(device['product_id'])}")
            return True
        
        # 也尝试搜索其他可能的设备
        log_info("搜索其他 HID 设备...")
        for device in hid.enumerate():
            if 'slime' in device.get('product_string', '').lower() or \
               'ch592' in device.get('product_string', '').lower():
                log_info(f"找到可能的设备: {device['product_string']}")
                return True
        
        return False
    
    def connect(self) -> bool:
        """连接 USB HID 设备"""
        try:
            self.hid_device = hid.device()
            self.hid_device.open(USB_VID, USB_PID)
            self.hid_device.set_nonblocking(True)
            
            manufacturer = self.hid_device.get_manufacturer_string()
            product = self.hid_device.get_product_string()
            serial = self.hid_device.get_serial_number_string()
            
            log_info(f"已连接: {manufacturer} {product}")
            log_debug(f"序列号: {serial}")
            
            return True
            
        except Exception as e:
            log_error(f"连接失败: {e}")
            return False
    
    def disconnect(self):
        """断开连接"""
        if self.hid_device:
            try:
                self.hid_device.close()
            except:
                pass
            self.hid_device = None
        log_info("已断开连接")
    
    def send_to_slimevr(self, data: bytes):
        """发送数据到 SlimeVR 服务端"""
        try:
            self.udp_socket.sendto(data, self.server_addr)
            log_debug(f"发送 {len(data)} 字节到 SlimeVR")
        except Exception as e:
            log_error(f"UDP 发送失败: {e}")
    
    def handle_tracker_data(self, data: Dict):
        """处理追踪器数据"""
        tracker_id = data['tracker_id']
        
        # 新追踪器发送握手
        if tracker_id not in self.connected_trackers:
            log_info(f"新追踪器连接: #{tracker_id}")
            handshake = self.protocol.build_handshake(tracker_id)
            self.send_to_slimevr(handshake)
            self.connected_trackers.add(tracker_id)
            self.last_battery_time[tracker_id] = 0
        
        if data['type'] == 'rotation':
            # 发送旋转数据
            rotation = self.protocol.build_rotation(
                tracker_id, 
                data['quaternion']
            )
            self.send_to_slimevr(rotation)
            
            log_debug(f"追踪器 #{tracker_id}: q={data['quaternion']}")
            
            # 定期发送电池状态 (每 5 秒)
            now = time.time()
            if now - self.last_battery_time.get(tracker_id, 0) > 5:
                battery = self.protocol.build_battery(
                    tracker_id,
                    data['battery']
                )
                self.send_to_slimevr(battery)
                self.last_battery_time[tracker_id] = now
    
    def heartbeat_thread(self):
        """心跳线程"""
        while self.running:
            heartbeat = self.protocol.build_heartbeat()
            self.send_to_slimevr(heartbeat)
            time.sleep(1)  # 每秒一次
    
    def run(self):
        """主循环"""
        if not self.connect():
            log_error("无法连接设备，退出")
            return
        
        self.running = True
        
        # 启动心跳线程
        heartbeat = threading.Thread(target=self.heartbeat_thread, daemon=True)
        heartbeat.start()
        
        log_info(f"桥接运行中... (服务端: {SLIMEVR_HOST}:{SLIMEVR_PORT})")
        log_info("按 Ctrl+C 停止")
        
        try:
            while self.running:
                # 读取 USB HID 数据
                try:
                    data = self.hid_device.read(64, timeout_ms=10)
                except Exception as e:
                    log_error(f"读取错误: {e}")
                    time.sleep(0.1)
                    continue
                
                if data:
                    parsed = parse_rf_ultra_packet(bytes(data))
                    if parsed:
                        self.handle_tracker_data(parsed)
                
                # 不要占用太多 CPU
                time.sleep(0.001)
                
        except KeyboardInterrupt:
            log_info("\n正在停止...")
        finally:
            self.running = False
            self.disconnect()
        
        log_info(f"会话统计: {len(self.connected_trackers)} 个追踪器连接过")

#==============================================================================
# 主程序 / Main
#==============================================================================

def main():
    global DEBUG
    
    parser = argparse.ArgumentParser(
        description='SlimeVR USB-UDP Bridge for CH592',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
    python slimevr_bridge.py              # 正常运行
    python slimevr_bridge.py --debug      # 调试模式
    python slimevr_bridge.py --list       # 列出 HID 设备
        """
    )
    
    parser.add_argument('--debug', '-d', action='store_true',
                        help='启用调试输出')
    parser.add_argument('--list', '-l', action='store_true',
                        help='列出所有 HID 设备')
    parser.add_argument('--host', default=SLIMEVR_HOST,
                        help=f'SlimeVR 服务端地址 (默认: {SLIMEVR_HOST})')
    parser.add_argument('--port', type=int, default=SLIMEVR_PORT,
                        help=f'SlimeVR 服务端端口 (默认: {SLIMEVR_PORT})')
    
    args = parser.parse_args()
    
    DEBUG = args.debug
    
    if args.list:
        log_info("列出所有 HID 设备:")
        for device in hid.enumerate():
            print(f"  VID={hex(device['vendor_id'])} PID={hex(device['product_id'])}")
            print(f"    产品: {device.get('product_string', 'N/A')}")
            print(f"    制造商: {device.get('manufacturer_string', 'N/A')}")
            print()
        return
    
    # 创建并运行桥接
    bridge = SlimeVRBridge()
    bridge.server_addr = (args.host, args.port)
    
    if not bridge.find_device():
        log_error("未找到 SlimeVR CH592 接收器!")
        log_info("请确保:")
        log_info("  1. 接收器已插入 USB 端口")
        log_info("  2. 接收器固件正确")
        log_info(f"  3. VID/PID 为 {hex(USB_VID)}/{hex(USB_PID)}")
        log_info("")
        log_info("运行 --list 查看所有 HID 设备")
        sys.exit(1)
    
    bridge.run()

if __name__ == '__main__':
    main()
