# 构建系统快速参考

## 快速开始

### 推荐方式（Linux/macOS）

```bash
# 编译所有固件
./build_complete.sh all

# 仅编译 Tracker
./build_complete.sh tracker

# 仅编译 Receiver  
./build_complete.sh receiver

# 清理构建文件
./build_complete.sh clean
```

### Windows PowerShell

```powershell
# 使用 PowerShell 脚本
.\build.ps1 -Target tracker -Chip CH592

# 或直接使用 Makefile（需要安装 make）
make TARGET=tracker CHIP=CH592
```

### 直接使用 Makefile

```bash
# 编译 Tracker
make TARGET=tracker CHIP=CH592

# 编译 Receiver
make TARGET=receiver CHIP=CH592

# 编译所有
make both CHIP=CH592

# 清理
make clean TARGET=tracker
```

## 目录结构

```
项目根目录/
├── build/              # 构建中间文件（自动生成，已忽略）
│   ├── tracker/       # Tracker 构建文件
│   └── receiver/      # Receiver 构建文件
│
├── output/             # 固件输出目录（自动生成，已忽略）
│   ├── tracker_CH592_v0.6.2.bin
│   ├── tracker_CH592_v0.6.2_combined.hex  # 推荐用于烧录
│   ├── receiver_CH592_v0.6.2.bin
│   └── receiver_CH592_v0.6.2_combined.hex
│
├── bootloader/
│   ├── build/         # Bootloader 构建文件
│   └── output/        # Bootloader 输出
│
├── Makefile           # 主构建配置
├── build_complete.sh  # 完整构建脚本（推荐）
└── build.ps1          # Windows 构建脚本
```

## 输出文件说明

### 文件类型

| 文件类型 | 用途 | 推荐使用场景 |
|---------|------|------------|
| `.bin` | 二进制固件 | 直接烧录工具 |
| `.hex` | Intel HEX 格式 | 大多数烧录工具 |
| `.combined.hex` | 含 Bootloader 的完整 HEX | **推荐用于烧录** |
| `.uf2` | UF2 格式 | USB DFU 更新 |

### 文件命名

```
{target}_{chip}_v{version}.{ext}
```

示例：
- `tracker_CH592_v0.6.2_combined.hex` - Tracker 完整固件
- `receiver_CH592_v0.6.2_combined.hex` - Receiver 完整固件

## 常用选项

### build_complete.sh 选项

```bash
# 指定芯片型号
./build_complete.sh all --mcu=CH591

# 启用调试模式
./build_complete.sh tracker --debug

# 不合并 Bootloader
./build_complete.sh tracker --no-boot

# 并行编译（加快速度）
./build_complete.sh all --parallel

# 详细输出
./build_complete.sh all --verbose
```

### Makefile 选项

```bash
# 指定版本号
make TARGET=tracker CHIP=CH592 VERSION=0.6.2

# 启用调试
make TARGET=tracker CHIP=CH592 DEBUG=1

# 仅编译单个 IMU 驱动（减小固件大小）
make TARGET=tracker CHIP=CH592 IMU=ICM45686
```

## 清理构建文件

```bash
# 清理所有构建文件
./build_complete.sh clean

# 或使用 Makefile
make clean TARGET=tracker
make clean TARGET=receiver
```

## 版本管理

版本号从 `VERSION` 文件自动读取，当前版本: **0.6.2**

## 常见问题

### Q: 找不到工具链？

A: 安装 RISC-V GCC 工具链：
```bash
# Ubuntu/Debian
sudo apt install gcc-riscv64-unknown-elf

# 或下载 xPack
# https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack
```

### Q: build 目录不存在？

A: 构建时会自动创建，无需手动创建。

### Q: 输出文件在哪里？

A: 所有输出文件在 `output/` 目录中。

### Q: 应该使用哪个文件烧录？

A: 推荐使用 `*_combined.hex` 文件，它包含完整的固件（Bootloader + 应用）。

## 更多文档

- [BUILD.md](BUILD.md) - 详细构建说明
- [docs/BUILD_DIRECTORY_STRUCTURE.md](docs/BUILD_DIRECTORY_STRUCTURE.md) - 目录结构说明
- [README.md](README.md) - 项目总体说明

