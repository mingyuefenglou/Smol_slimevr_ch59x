# SlimeVR CH59X 工具使用指南

## 概述

本文档描述项目中各工具脚本的使用方法。

---

## 1. 编译工具

### build_full.sh - 完整编译脚本

**用法:**
```bash
# 完整编译 (Bootloader + Tracker + Receiver)
./build_full.sh

# 仅编译Tracker
./build_full.sh -t tracker

# 仅编译Receiver  
./build_full.sh -t receiver

# 指定芯片型号
./build_full.sh -c CH591
./build_full.sh -c CH592

# 启用OTA
./build_full.sh -o
```

**输出:**
- `output/tracker_CH59X_vX.Y.Z_combined.hex`
- `output/receiver_CH59X_vX.Y.Z_combined.hex`
- `output/*.uf2` (UF2格式，用于拖放更新)

### Makefile - 底层编译

**用法:**
```bash
# 编译Tracker
make TARGET=tracker CHIP=CH592

# 编译Receiver
make TARGET=receiver CHIP=CH592

# 启用LTO优化
make LTO_FLAGS=-flto

# 清理
make clean

# 指定IMU类型
make IMU=ICM45686
```

---

## 2. 烧录工具

### WCH-Link 命令行烧录

```bash
# 烧录合并后的HEX文件
wchisp flash output/tracker_CH592_v0.6.1_combined.hex

# 读取芯片ID
wchisp info

# 擦除Flash
wchisp erase
```

### USB DFU更新 (UF2)

1. 按住BOOT按键开机
2. 电脑识别为U盘 "SLIMEVR"
3. 拖放 `.uf2` 文件到U盘
4. 自动更新并重启

---

## 3. 调试工具

### tools/slimevr_bridge.py - USB调试桥

**功能:**
- 连接SlimeVR Receiver
- 显示实时Tracker数据
- 导出诊断信息

**用法:**
```bash
# 基本使用
python tools/slimevr_bridge.py

# 指定VID/PID
python tools/slimevr_bridge.py --vid 0x1209 --pid 0x5711

# 导出日志
python tools/slimevr_bridge.py --log output.jsonl
```

### tools/longrun_logger.py - 长稳测试脚本

**功能:**
- 从Receiver读取统计数据
- 每10秒记录一条JSONL
- 自动检测异常并告警
- 生成最终健康报告

**用法:**
```bash
# 1小时测试
python tools/longrun_logger.py --duration 3600 --output test.jsonl

# 4小时封版测试
python tools/longrun_logger.py --duration 14400 --output longrun.jsonl

# 详细输出
python tools/longrun_logger.py --duration 3600 --verbose
```

**输出文件:**
- `test.jsonl` - JSONL格式原始数据
- `test_report.json` - 最终健康报告

**报告示例:**
```json
{
  "test_info": {
    "duration_sec": 3600,
    "sample_count": 360
  },
  "summary": {
    "total_alerts": 0,
    "pass": true
  }
}
```

---

## 4. 固件工具

### tools/bin2uf2.py - UF2转换

**功能:**
- 将BIN文件转换为UF2格式

**用法:**
```bash
python tools/bin2uf2.py input.bin output.uf2 --base 0x1000
```

### tools/merge_hex.py - HEX合并

**功能:**
- 合并Bootloader和App的HEX文件

**用法:**
```bash
python tools/merge_hex.py bootloader.hex app.hex combined.hex
```

---

## 5. 诊断工具

### USB HID诊断命令

通过USB HID发送诊断命令:

| Report ID | 命令 | 说明 |
|-----------|------|------|
| 0x20 | 0x01 | 请求统计数据 |
| 0x20 | 0x02 | 请求事件日志 |
| 0x20 | 0x03 | 请求崩溃快照 |
| 0x20 | 0x10 | 重置计数器 |
| 0x20 | 0x11 | 清除事件日志 |

### 诊断报告导出

```python
import hid

device = hid.device()
device.open(0x1209, 0x5711)

# 请求诊断报告
device.write([0x20, 0x01])

# 读取响应
data = device.read(64)
```

---

## 6. 测试清单

### 必跑测试 (每次发布前)

```bash
# 1. 逻辑分析仪时序确认
# - 确认SYNC周期 = 5000us
# - 确认slot不越界

# 2. 10 Tracker长稳测试 (60分钟)
python tools/longrun_logger.py --duration 3600

# 3. 干扰环境测试 (30分钟)
# - 在WiFi密集环境运行

# 4. 睡眠唤醒压力测试 (50次)
# - 静止→睡眠→抬手唤醒→重复
```

### 封版测试 (4小时)

```bash
python tools/longrun_logger.py --duration 14400 --output release_test.jsonl
```

通过标准:
- 无掉线
- 无假死
- 丢包率 < 1%
- 告警数 = 0

---

## 7. 常见问题

### Q: 编译失败找不到SDK

```bash
# 设置SDK路径
export CH59X_SDK=/path/to/sdk

# 或运行setup脚本
./setup_sdk.sh
```

### Q: USB设备无法识别

1. 检查VID/PID是否正确 (0x1209:0x5711)
2. 检查USB线缆
3. 尝试重新烧录Bootloader

### Q: Tracker无法配对

1. 长按Receiver按钮3秒进入配对模式
2. 短按Tracker按钮开始配对
3. LED闪烁表示配对成功

---

## 版本历史

| 版本 | 日期 | 说明 |
|------|------|------|
| v0.6.1 | 2026-01-24 | 添加CCA支持 |
| v0.6.0 | 2026-01-24 | 产品化完整版 |
| v0.5.0 | 2026-01-24 | Retained state |
| v0.4.22 | 2026-01-24 | AllInOne优化 |
