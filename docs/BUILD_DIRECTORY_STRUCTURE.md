# 构建目录结构说明

## 概述

本文档说明固件项目的构建目录结构，帮助理解构建产物的组织方式。

## 目录树

```
Smol_slimevr_ch59x/
│
├── build/                          # 构建中间文件目录（已加入 .gitignore）
│   ├── tracker/                    # Tracker 固件构建目录
│   │   ├── src/                    # 源文件编译产物
│   │   │   ├── main_tracker.o
│   │   │   ├── hal/
│   │   │   ├── rf/
│   │   │   └── sensor/
│   │   ├── sdk/                    # SDK 编译产物
│   │   │   ├── StdPeriphDriver/
│   │   │   └── RVMSIS/
│   │   ├── slimevr_tracker.elf     # ELF 可执行文件
│   │   └── slimevr_tracker.map     # 链接映射文件
│   │
│   └── receiver/                   # Receiver 固件构建目录
│       ├── src/
│       │   ├── main_receiver.o
│       │   ├── hal/
│       │   ├── rf/
│       │   └── usb/
│       ├── sdk/
│       ├── slimevr_receiver.elf
│       └── slimevr_receiver.map
│
├── output/                         # 固件输出目录（已加入 .gitignore）
│   ├── tracker_CH592_v0.6.2.bin   # Tracker 二进制文件
│   ├── tracker_CH592_v0.6.2.hex   # Tracker HEX 文件
│   ├── tracker_CH592_v0.6.2_combined.hex  # 含 Bootloader 的完整 HEX
│   ├── tracker_CH592_v0.6.2.uf2   # Tracker UF2 文件（USB DFU）
│   ├── receiver_CH592_v0.6.2.bin
│   ├── receiver_CH592_v0.6.2.hex
│   ├── receiver_CH592_v0.6.2_combined.hex
│   └── receiver_CH592_v0.6.2.uf2
│
└── bootloader/
    ├── build/                      # Bootloader 构建目录（已加入 .gitignore）
    │   ├── bootloader_main.o
    │   ├── startup_boot.o
    │   ├── bootloader_ch592.elf
    │   └── bootloader_ch592.map
    │
    └── output/                      # Bootloader 输出目录（已加入 .gitignore）
        ├── bootloader_ch592.bin    # Bootloader 二进制（最大 4KB）
        └── bootloader_ch592.hex    # Bootloader HEX 文件
```

## 目录说明

### build/ 目录

**用途**: 存储编译过程中的中间文件（.o, .d, .elf, .map）

**特点**:
- 自动创建，无需手动创建
- 已加入 `.gitignore`，不会被版本控制
- 清理命令会删除此目录

**子目录结构**:
- `build/tracker/` - Tracker 固件的所有构建文件
- `build/receiver/` - Receiver 固件的所有构建文件
- 每个子目录下按源文件路径组织（src/, sdk/）

### output/ 目录

**用途**: 存储最终的可烧录固件文件

**文件类型**:
- `.bin` - 二进制格式，用于直接烧录
- `.hex` - Intel HEX 格式，用于烧录工具
- `.combined.hex` - 合并了 Bootloader 的完整 HEX（推荐使用）
- `.uf2` - UF2 格式，用于 USB DFU 更新

**命名规则**:
```
{target}_{chip}_v{version}.{ext}
```

示例:
- `tracker_CH592_v0.6.2.bin`
- `receiver_CH592_v0.6.2_combined.hex`

### bootloader/build/ 和 bootloader/output/

**用途**: Bootloader 的构建和输出目录

**限制**:
- Bootloader 大小限制: 4KB
- 编译时会自动检查大小

## 文件说明

### ELF 文件 (.elf)

- **用途**: 包含调试信息的可执行文件
- **位置**: `build/{target}/{project}.elf`
- **用途**: 用于调试、查看内存使用情况

### MAP 文件 (.map)

- **用途**: 链接映射文件，显示符号地址和内存布局
- **位置**: `build/{target}/{project}.map`
- **用途**: 分析内存使用、查找符号地址

### BIN 文件 (.bin)

- **用途**: 纯二进制固件
- **位置**: `output/{target}_{chip}_v{version}.bin`
- **用途**: 直接烧录到 Flash

### HEX 文件 (.hex)

- **用途**: Intel HEX 格式固件
- **位置**: `output/{target}_{chip}_v{version}.hex`
- **用途**: 大多数烧录工具支持此格式

### Combined HEX 文件 (.combined.hex)

- **用途**: 合并了 Bootloader 和应用固件的完整 HEX
- **位置**: `output/{target}_{chip}_v{version}_combined.hex`
- **用途**: **推荐用于烧录**，包含完整的固件

### UF2 文件 (.uf2)

- **用途**: USB DFU 更新格式
- **位置**: `output/{target}_{chip}_v{version}.uf2`
- **用途**: 拖放到 USB 设备进行更新

## 清理构建文件

### 清理所有构建文件

```bash
# 使用构建脚本
./build_complete.sh clean

# 或使用 Makefile
make clean TARGET=tracker
make clean TARGET=receiver
```

### 手动清理

```bash
# 清理构建目录
rm -rf build/
rm -rf bootloader/build/

# 清理输出文件
rm -f output/*.bin output/*.hex output/*.uf2
```

## 注意事项

1. **不要提交构建文件**: `build/` 和 `output/` 目录已加入 `.gitignore`
2. **自动创建**: 构建时会自动创建所需目录
3. **版本号**: 输出文件名包含版本号，从 `VERSION` 文件读取
4. **芯片型号**: 输出文件名包含芯片型号（CH591/CH592）

## 相关文档

- [BUILD.md](../BUILD.md) - 构建系统使用说明
- [README.md](../README.md) - 项目总体说明

