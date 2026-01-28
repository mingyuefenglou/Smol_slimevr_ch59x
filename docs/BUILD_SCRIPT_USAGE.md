# SlimeVR CH592 固件编译脚本使用指南
# SlimeVR CH592 Firmware Build Script Usage Guide

## 目录

1. [快速开始](#1-快速开始)
2. [编译脚本详解](#2-编译脚本详解)
3. [Makefile 使用](#3-makefile-使用)
4. [版本管理](#4-版本管理)
5. [固件更新流程](#5-固件更新流程)
6. [高级配置](#6-高级配置)
7. [常见问题](#7-常见问题)

---

## 1. 快速开始

### 1.1 环境准备

**必需工具:**
```bash
# RISC-V 工具链 (任选一个)
# 方式 1: xPack RISC-V GCC (推荐)
# 下载: https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases

# 方式 2: MounRiver Studio 工具链
# 下载: http://www.mounriver.com/download

# Python (用于 UF2 转换)
python --version  # 需要 Python 3.6+

# Make 工具
make --version
```

**设置环境变量:**
```bash
# Linux/macOS
export PATH=$PATH:/path/to/riscv-toolchain/bin

# Windows (PowerShell)
$env:PATH += ";C:\riscv-toolchain\bin"
```

### 1.2 第一次编译

```bash
# 进入项目目录
cd slimevr_ch59x

# 编译追踪器固件
make tracker

# 编译接收器固件
make receiver

# 或一次编译两者
make both
```

**输出文件位置:**
```
output/
├── tracker_CH592_v1.0.0.bin   # 追踪器二进制
├── tracker_CH592_v1.0.0.hex   # 追踪器 HEX
├── tracker_CH592_v1.0.0.uf2   # 追踪器 UF2 (拖放烧录)
├── receiver_CH592_v1.0.0.bin  # 接收器二进制
├── receiver_CH592_v1.0.0.hex  # 接收器 HEX
└── receiver_CH592_v1.0.0.uf2  # 接收器 UF2
```

---

## 2. 编译脚本详解

### 2.1 build_firmware.sh 命令

```bash
./build_firmware.sh [命令] [选项]
```

**命令列表:**

| 命令 | 说明 | 示例 |
|------|------|------|
| `tracker` | 编译追踪器 | `./build_firmware.sh tracker` |
| `receiver` | 编译接收器 | `./build_firmware.sh receiver` |
| `all` | 编译全部 | `./build_firmware.sh all` |
| `clean` | 清理构建 | `./build_firmware.sh clean` |
| `version` | 显示版本 | `./build_firmware.sh version` |
| `bump` | 增加版本号 | `./build_firmware.sh bump patch` |
| `update` | 更新源码 | `./build_firmware.sh update ~/new-src` |

**选项:**

| 选项 | 说明 | 示例 |
|------|------|------|
| `--chip` | 目标芯片 | `--chip CH591` |
| `--help` | 显示帮助 | `--help` |

### 2.2 使用示例

```bash
# 基本编译
./build_firmware.sh tracker

# 为 CH591 编译
./build_firmware.sh tracker --chip CH591

# 编译全部 (CH592)
./build_firmware.sh all

# 清理后重新编译
./build_firmware.sh clean
./build_firmware.sh all

# 查看当前版本
./build_firmware.sh version

# 增加版本号
./build_firmware.sh bump patch   # v1.0.0 -> v1.0.1
./build_firmware.sh bump minor   # v1.0.1 -> v1.1.0
./build_firmware.sh bump major   # v1.1.0 -> v2.0.0
```

---

## 3. Makefile 使用

### 3.1 基本命令

```bash
# 编译追踪器
make tracker

# 编译接收器
make receiver

# 编译两者
make both

# 清理
make clean

# 显示帮助
make help

# 显示配置
make info
```

### 3.2 带参数编译

```bash
# 指定芯片
make TARGET=tracker CHIP=CH591

# 指定版本号
make TARGET=tracker VERSION_MAJOR=2 VERSION_MINOR=1 VERSION_PATCH=0

# 编译 CH591 版本
make ch591

# 调试版本 (无优化)
make debug

# 生成反汇编
make disasm
```

### 3.3 输出文件命名规则

```
{target}_{chip}_v{major}.{minor}.{patch}.{ext}

示例:
- tracker_CH592_v1.0.0.bin
- tracker_CH592_v1.0.0.hex
- tracker_CH592_v1.0.0.uf2
- receiver_CH591_v2.1.3.bin
```

---

## 4. 版本管理

### 4.1 版本文件

版本信息存储在 `VERSION` 文件中:

```bash
# 查看版本文件
cat VERSION

# 输出:
# VERSION_MAJOR=1
# VERSION_MINOR=0
# VERSION_PATCH=0
# BUILD_DATE="2026-01-12"
# BUILD_TIME="15:31:59"
```

### 4.2 版本号规则

遵循语义化版本 (SemVer):

| 类型 | 说明 | 示例 |
|------|------|------|
| Major | 重大更新/不兼容 | 1.0.0 -> 2.0.0 |
| Minor | 新功能/兼容 | 1.0.0 -> 1.1.0 |
| Patch | Bug 修复 | 1.0.0 -> 1.0.1 |

### 4.3 自动版本递增

```bash
# 修复 Bug 后
./build_firmware.sh bump patch
./build_firmware.sh all

# 添加新功能后
./build_firmware.sh bump minor
./build_firmware.sh all

# 重大版本更新
./build_firmware.sh bump major
./build_firmware.sh all
```

---

## 5. 固件更新流程

### 5.1 从 SlimeVR 官方更新

当 SlimeVR 官方发布新版本时:

```bash
# 1. 下载新版本源码
git clone https://github.com/SlimeVR/SlimeVR-Tracker-ESP.git slimevr-new

# 2. 运行更新命令
./build_firmware.sh update ./slimevr-new

# 3. 脚本自动:
#    - 备份当前源码
#    - 复制新源码
#    - 递增补丁版本号

# 4. 重新编译
./build_firmware.sh all

# 5. 检查输出
ls -la output/
```

### 5.2 手动更新源码

```bash
# 1. 进入源码目录
cd slimevr_src

# 2. 复制需要的文件
cp -r /path/to/new-source/* .

# 3. 更新版本号
./build_firmware.sh bump minor

# 4. 编译
./build_firmware.sh all
```

### 5.3 批量烧录

```bash
#!/bin/bash
# batch_flash.sh - 批量烧录脚本

FIRMWARE="output/tracker_CH592_v1.0.0.bin"

echo "准备批量烧录..."
echo "固件: $FIRMWARE"
echo ""

while true; do
    echo "请插入追踪器, 按 Enter 继续 (Ctrl+C 退出)"
    read
    
    wchisp flash "$FIRMWARE"
    
    if [ $? -eq 0 ]; then
        echo "✓ 烧录成功!"
    else
        echo "✗ 烧录失败!"
    fi
    echo ""
done
```

---

## 6. 高级配置

### 6.1 修改默认 IMU

编辑 `include/config.h`:

```c
// 可选: ICM45686 (默认), ICM42688, BMI270, LSM6DSV, LSM6DSR
#define IMU_TYPE    IMU_ICM45686

// 改为其他传感器:
#define IMU_TYPE    IMU_BMI270
#define IMU_TYPE    IMU_LSM6DSV
```

### 6.2 修改 RF 功率

编辑 `include/config.h`:

```c
// 发射功率: -20, -16, -12, -8, -4, 0, +3, +4 dBm
#define RF_TX_POWER_DBM     4    // +4dBm (最大)

// 降低功率以节省电量:
#define RF_TX_POWER_DBM     0    // 0dBm
```

### 6.3 使用 EKF 替代 VQF

编辑 `include/config.h`:

```c
// 取消注释以启用 EKF
#define USE_EKF_INSTEAD_OF_VQF
```

或在编译时指定:

```bash
make TARGET=tracker DEFINES="-DUSE_EKF_INSTEAD_OF_VQF"
```

### 6.4 修改采样率

编辑 `include/config.h`:

```c
// 默认 200Hz
#define SENSOR_ODR_HZ       200

// 可改为:
#define SENSOR_ODR_HZ       100     // 省电
#define SENSOR_ODR_HZ       400     // 低延迟
```

---

## 7. 常见问题

### Q1: 找不到工具链

```
错误: riscv-none-elf-gcc: command not found
```

**解决:**
```bash
# 检查工具链路径
which riscv-none-elf-gcc

# 如果没有, 添加到 PATH
export PATH=$PATH:/path/to/riscv-toolchain/bin

# 或指定前缀
make CROSS_COMPILE=/full/path/to/riscv-none-elf-
```

### Q2: UF2 文件生成失败

```
错误: bin2uf2.py not found
```

**解决:**
```bash
# 确保 tools/bin2uf2.py 存在且有执行权限
chmod +x tools/bin2uf2.py

# 确保 Python 可用
python3 --version
```

### Q3: 链接错误

```
错误: undefined reference to 'xxx'
```

**解决:**
```bash
# 清理并重新编译
make clean
make all
```

### Q4: Flash 空间不足

```
错误: section '.text' will not fit in region 'FLASH'
```

**解决:**
- 检查是否为 CH591 (256KB Flash)
- 禁用调试功能
- 使用 `-Oz` 优化

```c
// 在 Makefile 中
OPT = -Oz
```

### Q5: RAM 空间不足

**解决:**
- 减少 MAX_TRACKERS
- 使用 VQF 而非 EKF
- 优化数据结构

---

## 附录: 完整编译示例

```bash
# 完整的编译和发布流程

# 1. 清理
make clean

# 2. 更新版本
./build_firmware.sh bump minor

# 3. 编译 CH592 版本
make CHIP=CH592 TARGET=tracker
make CHIP=CH592 TARGET=receiver

# 4. 编译 CH591 版本
make CHIP=CH591 TARGET=tracker
make CHIP=CH591 TARGET=receiver

# 5. 检查输出
ls -la output/

# 6. 打包发布
VERSION=$(./build_firmware.sh version)
zip -r "slimevr_ch59x_firmware_${VERSION}.zip" output/

echo "发布包: slimevr_ch59x_firmware_${VERSION}.zip"
```
