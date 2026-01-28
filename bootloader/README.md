# SlimeVR CH59X Bootloader

## 概述

独立的 4KB Bootloader，位于 Flash 起始地址 (0x0000)。

## 功能

1. **启动检测**
   - 检查 BOOT 按键 (PB4)
   - 检查应用程序有效性

2. **启动流程**
   ```
   复位
     ↓
   检查 BOOT 按键
     ↓ (未按下)          ↓ (按下)
   检查应用有效性      进入 DFU 模式
     ↓ (有效)
   跳转到应用 (0x1000)
   ```

3. **DFU 模式**
   - LED 闪烁指示
   - USB MSC 模式 (待实现)

## 内存布局

```
0x00000000 ┌─────────────────┐
           │   Bootloader    │  4KB
           │   (本程序)       │
0x00001000 ├─────────────────┤
           │   Application   │  444KB
           │   (主固件)       │
0x00070000 └─────────────────┘
```

## 编译

```bash
cd bootloader
make
```

输出文件:
- `output/bootloader_ch592.bin` - 二进制文件
- `output/bootloader_ch592.hex` - Intel HEX 文件

## 烧录

### 使用 WCH-LinkE

```bash
# 先烧录 Bootloader
wch-link -w bootloader_ch592.hex -a 0x0000

# 再烧录应用程序
wch-link -w tracker_ch592.hex -a 0x1000
```

### 合并 HEX 文件

```bash
# 使用 srec_cat 合并
srec_cat bootloader_ch592.hex -Intel tracker_ch592.hex -Intel -o combined.hex -Intel
```

## 大小限制

Bootloader 必须小于 4096 字节 (4KB)。当前实现约 1-2KB。

## 版本历史

- v1.0.0: 初始版本，基础启动功能
