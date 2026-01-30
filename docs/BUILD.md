# 构建系统说明

## 目录结构

```
Smol_slimevr_ch59x/
├── build/                    # 构建中间文件（自动生成，已忽略）
│   ├── tracker/             # Tracker 构建文件
│   │   ├── src/            # 源文件编译产物
│   │   ├── sdk/            # SDK 编译产物
│   │   ├── *.elf           # ELF 文件
│   │   └── *.map           # 链接映射文件
│   └── receiver/           # Receiver 构建文件
│       ├── src/
│       ├── sdk/
│       ├── *.elf
│       └── *.map
│
├── output/                   # 固件输出目录（已忽略）
│   ├── tracker_CH592_v0.6.2.bin
│   ├── tracker_CH592_v0.6.2.hex
│   ├── tracker_CH592_v0.6.2_combined.hex  # 含 Bootloader
│   ├── tracker_CH592_v0.6.2.uf2
│   ├── receiver_CH592_v0.6.2.bin
│   ├── receiver_CH592_v0.6.2.hex
│   ├── receiver_CH592_v0.6.2_combined.hex
│   └── receiver_CH592_v0.6.2.uf2
│
├── bootloader/
│   ├── build/               # Bootloader 构建文件（已忽略）
│   └── output/              # Bootloader 输出（已忽略）
│       ├── bootloader_ch592.bin
│       └── bootloader_ch592.hex
│
├── Makefile                 # 主构建配置
├── build.sh                 # 主构建脚本（推荐使用）
├── scripts/
│   └── build/              # 构建脚本目录
│       ├── build_complete.sh  # 完整构建脚本（功能最全）
│       ├── build_complete.sh
│       └── build_complete.ps1
└── build.ps1               # Windows PowerShell 构建脚本
```

## 快速开始

### Linux/macOS/Windows

```bash
# 编译 Tracker (默认 CH591D 板子)
make TARGET=tracker

# 编译 Receiver (默认 CH591D 板子)
make TARGET=receiver

# 指定板子类型
make TARGET=tracker BOARD=ch591d    # CH591D 板子
make TARGET=tracker BOARD=ch592x   # CH592X 板子
make TARGET=tracker BOARD=generic_board  # 通用板子（需配置引脚）

# 清理所有构建文件
make clean
```

### Windows

```powershell
# 使用 PowerShell 脚本
.\build.ps1 -Target tracker -Chip CH592

# 或直接使用 Makefile
make TARGET=tracker CHIP=CH592
```

## 构建选项

### Makefile 选项

```bash
# 基本用法
make TARGET=tracker BOARD=ch591d
# 或直接使用
./scripts/build/build_complete.sh [target] [options]

# 目标选项
all         # 编译所有（默认）
tracker     # 仅编译 Tracker
receiver    # 仅编译 Receiver
bootloader  # 仅编译 Bootloader
clean       # 清理所有构建文件

# 常用选项
--mcu=CH592        # 指定 MCU 型号（CH591/CH592，默认 CH592）
--debug            # 启用调试模式
--no-boot          # 不编译/合并 Bootloader
--parallel         # 并行编译（同时编译 tracker 和 receiver）
--verify           # 编译后验证产物
-v, --verbose      # 详细输出
```

### Makefile 选项

```bash
# 基本用法
make TARGET=tracker CHIP=CH592 VERSION=0.6.2

# 清理
make clean TARGET=tracker

# 编译所有
make both CHIP=CH592
```

## 输出文件说明

### 文件类型

- **.bin** - 二进制固件文件（用于直接烧录）
- **.hex** - Intel HEX 格式（用于烧录工具）
- **.combined.hex** - 合并了 Bootloader 的完整 HEX 文件（推荐用于烧录）
- **.uf2** - UF2 格式（用于 USB DFU 更新）

### 文件命名规则

```
{target}_{chip}_v{version}.{ext}
```

示例：
- `tracker_CH592_v0.6.2.bin`
- `receiver_CH592_v0.6.2_combined.hex`

## 构建流程

1. **编译 Bootloader** (4KB)
   - 位置: `bootloader/output/`
   - 限制: 最大 4KB

2. **编译应用固件**
   - Tracker: `output/tracker_*.bin`
   - Receiver: `output/receiver_*.bin`

3. **合并 HEX 文件**（可选）
   - 将 Bootloader 和应用固件合并
   - 生成 `*_combined.hex` 文件

4. **生成 UF2 文件**（可选）
   - 用于 USB DFU 更新
   - 生成 `*.uf2` 文件

## 清理构建文件

```bash
# 清理所有构建文件
./build.sh clean

# 或使用 Makefile
make clean TARGET=tracker
make clean TARGET=receiver
```

## 常见问题

### 1. 找不到工具链

确保已安装 RISC-V GCC 工具链：
- Ubuntu/Debian: `sudo apt install gcc-riscv64-unknown-elf`
- 或下载 xPack: https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack

### 2. 构建目录不存在

构建目录会在首次编译时自动创建，无需手动创建。

### 3. 输出文件在哪里？

所有输出文件在 `output/` 目录中。

### 4. 如何查看构建日志？

使用 `--verbose` 选项查看详细输出：
```bash
./build.sh all --verbose
```

## 版本管理

版本号从 `VERSION` 文件读取，或通过 `--version` 参数指定。

当前版本: 0.6.2

