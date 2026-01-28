#!/usr/bin/env python3
"""
SlimeVR CH59X 长稳测试脚本 v0.6.0
Long-run stability test logger

用途:
- 从Receiver USB HID读取统计数据
- 每10秒记录一行JSONL
- 生成最终健康报告
- 检测异常并告警

依赖:
- pip install hidapi pyserial

用法:
- python longrun_logger.py --duration 3600 --output report.jsonl
"""

import argparse
import json
import time
import sys
import os
from datetime import datetime
from dataclasses import dataclass, asdict
from typing import Optional, List, Dict

# 尝试导入HID库
try:
    import hid
    HID_AVAILABLE = True
except ImportError:
    HID_AVAILABLE = False
    print("Warning: hidapi not installed, using mock mode")

# USB VID/PID
USB_VID = 0x1209  # OpenMoko
USB_PID = 0x5711  # SlimeVR CH59X

# Report IDs
REPORT_ID_DATA = 0x01
REPORT_ID_STATUS = 0x13
REPORT_ID_DIAG = 0x20

@dataclass
class TrackerStats:
    """单个Tracker统计"""
    tracker_id: int
    connected: bool
    rssi: int
    battery_pct: int
    loss_rate_pct: float
    total_packets: int
    lost_packets: int

@dataclass
class ReceiverStats:
    """Receiver统计"""
    uptime_sec: int
    superframe_count: int
    data_received: int
    frame_overrun: int
    usb_tx_count: int
    active_trackers: int

@dataclass
class TestRecord:
    """测试记录"""
    timestamp: str
    elapsed_sec: int
    receiver: ReceiverStats
    trackers: List[TrackerStats]
    alerts: List[str]

class LongRunLogger:
    def __init__(self, duration_sec: int, output_file: str, verbose: bool = False):
        self.duration_sec = duration_sec
        self.output_file = output_file
        self.verbose = verbose
        self.device = None
        self.records: List[TestRecord] = []
        self.start_time = None
        self.alerts_count = 0
        
    def connect(self) -> bool:
        """连接USB设备"""
        if not HID_AVAILABLE:
            print("[MOCK] Simulating device connection")
            return True
            
        try:
            self.device = hid.device()
            self.device.open(USB_VID, USB_PID)
            self.device.set_nonblocking(True)
            print(f"Connected to {self.device.get_manufacturer_string()} {self.device.get_product_string()}")
            return True
        except Exception as e:
            print(f"Failed to connect: {e}")
            return False
    
    def disconnect(self):
        """断开连接"""
        if self.device:
            self.device.close()
            self.device = None
    
    def read_stats(self) -> Optional[Dict]:
        """读取统计数据"""
        if not HID_AVAILABLE:
            # Mock数据
            return {
                'uptime_sec': int(time.time() - self.start_time) if self.start_time else 0,
                'superframe_count': 0,
                'data_received': 0,
                'frame_overrun': 0,
                'usb_tx_count': 0,
                'trackers': []
            }
        
        try:
            # 发送诊断请求
            self.device.write([REPORT_ID_DIAG, 0x01])  # 请求统计
            
            # 读取响应
            data = self.device.read(64, timeout_ms=1000)
            if not data:
                return None
            
            # 解析响应 (根据实际协议)
            return self._parse_diag_response(data)
            
        except Exception as e:
            if self.verbose:
                print(f"Read error: {e}")
            return None
    
    def _parse_diag_response(self, data: bytes) -> Dict:
        """解析诊断响应"""
        # 简化解析，实际需要根据协议
        return {
            'uptime_sec': int.from_bytes(data[4:8], 'little') if len(data) > 8 else 0,
            'superframe_count': int.from_bytes(data[8:12], 'little') if len(data) > 12 else 0,
            'data_received': int.from_bytes(data[12:16], 'little') if len(data) > 16 else 0,
            'frame_overrun': int.from_bytes(data[16:20], 'little') if len(data) > 20 else 0,
            'usb_tx_count': int.from_bytes(data[20:24], 'little') if len(data) > 24 else 0,
            'trackers': []
        }
    
    def check_alerts(self, stats: Dict) -> List[str]:
        """检查异常"""
        alerts = []
        
        # 帧超时过多
        if stats.get('frame_overrun', 0) > 100:
            alerts.append(f"HIGH_FRAME_OVERRUN: {stats['frame_overrun']}")
        
        # Tracker掉线
        for tr in stats.get('trackers', []):
            if not tr.get('connected', True):
                alerts.append(f"TRACKER_OFFLINE: {tr.get('tracker_id', '?')}")
            if tr.get('loss_rate_pct', 0) > 10:
                alerts.append(f"HIGH_LOSS_RATE: tracker={tr.get('tracker_id')}, loss={tr.get('loss_rate_pct')}%")
        
        return alerts
    
    def record_sample(self) -> TestRecord:
        """记录一个样本"""
        stats = self.read_stats() or {}
        
        elapsed = int(time.time() - self.start_time) if self.start_time else 0
        
        receiver = ReceiverStats(
            uptime_sec=stats.get('uptime_sec', 0),
            superframe_count=stats.get('superframe_count', 0),
            data_received=stats.get('data_received', 0),
            frame_overrun=stats.get('frame_overrun', 0),
            usb_tx_count=stats.get('usb_tx_count', 0),
            active_trackers=len(stats.get('trackers', []))
        )
        
        trackers = [
            TrackerStats(
                tracker_id=tr.get('tracker_id', i),
                connected=tr.get('connected', True),
                rssi=tr.get('rssi', -50),
                battery_pct=tr.get('battery_pct', 100),
                loss_rate_pct=tr.get('loss_rate_pct', 0),
                total_packets=tr.get('total_packets', 0),
                lost_packets=tr.get('lost_packets', 0)
            )
            for i, tr in enumerate(stats.get('trackers', []))
        ]
        
        alerts = self.check_alerts(stats)
        self.alerts_count += len(alerts)
        
        record = TestRecord(
            timestamp=datetime.now().isoformat(),
            elapsed_sec=elapsed,
            receiver=receiver,
            trackers=trackers,
            alerts=alerts
        )
        
        return record
    
    def run(self):
        """运行测试"""
        print(f"Starting long-run test for {self.duration_sec} seconds")
        print(f"Output: {self.output_file}")
        print("-" * 50)
        
        if not self.connect():
            print("Failed to connect, exiting")
            return False
        
        self.start_time = time.time()
        
        try:
            with open(self.output_file, 'w') as f:
                while (time.time() - self.start_time) < self.duration_sec:
                    record = self.record_sample()
                    self.records.append(record)
                    
                    # 写入JSONL
                    f.write(json.dumps(asdict(record), ensure_ascii=False) + '\n')
                    f.flush()
                    
                    # 打印进度
                    elapsed = int(time.time() - self.start_time)
                    progress = (elapsed / self.duration_sec) * 100
                    status = f"[{elapsed:5d}s / {self.duration_sec}s] ({progress:5.1f}%)"
                    
                    if record.alerts:
                        status += f" ALERTS: {record.alerts}"
                    
                    print(f"\r{status}", end='', flush=True)
                    
                    # 等待下一个采样周期
                    time.sleep(10)
                    
        except KeyboardInterrupt:
            print("\n\nTest interrupted by user")
        finally:
            self.disconnect()
        
        print("\n" + "-" * 50)
        self.generate_report()
        
        return self.alerts_count == 0
    
    def generate_report(self):
        """生成最终报告"""
        if not self.records:
            print("No records to analyze")
            return
        
        duration = self.records[-1].elapsed_sec if self.records else 0
        
        report = {
            'test_info': {
                'start_time': self.records[0].timestamp,
                'end_time': self.records[-1].timestamp,
                'duration_sec': duration,
                'sample_count': len(self.records),
            },
            'summary': {
                'total_alerts': self.alerts_count,
                'pass': self.alerts_count == 0,
            },
            'receiver_stats': {
                'final_uptime': self.records[-1].receiver.uptime_sec,
                'final_superframes': self.records[-1].receiver.superframe_count,
                'final_frame_overrun': self.records[-1].receiver.frame_overrun,
            }
        }
        
        # 写入报告
        report_file = self.output_file.replace('.jsonl', '_report.json')
        with open(report_file, 'w') as f:
            json.dump(report, f, indent=2, ensure_ascii=False)
        
        print(f"\nTest {'PASSED' if report['summary']['pass'] else 'FAILED'}")
        print(f"Duration: {duration}s, Samples: {len(self.records)}, Alerts: {self.alerts_count}")
        print(f"Report saved to: {report_file}")

def main():
    parser = argparse.ArgumentParser(description='SlimeVR CH59X Long-run Test Logger')
    parser.add_argument('--duration', type=int, default=3600, help='Test duration in seconds (default: 3600)')
    parser.add_argument('--output', type=str, default='longrun_test.jsonl', help='Output file (JSONL format)')
    parser.add_argument('--verbose', '-v', action='store_true', help='Verbose output')
    
    args = parser.parse_args()
    
    logger = LongRunLogger(args.duration, args.output, args.verbose)
    success = logger.run()
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
