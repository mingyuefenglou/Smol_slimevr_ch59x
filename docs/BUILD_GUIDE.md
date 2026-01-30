# SlimeVR CH59X 编译指南

## 环境要求

### 工具链

- **RISC-V GCC**: xPack RISC-V GCC 12.x 或更高
- **Make**: GNU Make 4.x
- **Python 3**: 用于 UF2 生成和 HEX 合并

### 安装 (Windows)

1. 下载 [xPack RISC-V GCC](https://github.com/xpack-dev-tools/riscv-none-elf-gcc-xpack/releases)
2. 解压到 `C:\xpack-riscv-none-elf-gcc`
3. 添加到 PATH: `C:\xpack-riscv-none-elf-gcc\bin`

### 安装 (Linux)

```bash
# Ubuntu/Debian
sudo apt install gcc-riscv64-unknown-elf make python3

# Arch Linux
sudo pacman -S riscv64-elf-gcc make python
```

## 编译步骤

### 1. 编译 Bootloader

```bash
cd bootloader
make clean
make

# 验证大小 (必须 < 4096 字节)
ls -l output/bootloader_ch592.bin
```

### 2. 编译 Tracker 固件

```bash
cd ..
make TARGET=tracker MCU=CH592 clean
make TARGET=tracker MCU=CH592

# 或使用编译脚本
./build.sh tracker
```

### 3. 编译 Receiver 固件

```bash
make TARGET=receiver MCU=CH592 clean
make TARGET=receiver MCU=CH592

# 或
./build.sh receiver
```

### 4. 合并固件

```bash
python3 tools/merge_hex.py \
    bootloader/output/bootloader_ch592.hex \
    output/tracker_ch592.hex \
    -o output/tracker_combined.hex
```

## 编译选项

### Makefile 变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `TARGET` | tracker | 目标: tracker 或 receiver |
| `MCU` | CH591 | MCU 型号: CH591 或 CH592 |
| `PREFIX` | riscv-none-elf- | 工具链前缀 |
| `DEBUG` | 0 | 调试模式: 0 或 1 |

### 编译示例

```bash
# 调试模式
make TARGET=tracker DEBUG=1

# CH591 版本
make TARGET=tracker MCU=CH591

# 指定工具链
make TARGET=tracker PREFIX=riscv64-unknown-elf-
```

## 输出文件

### Bootloader

| 文件 | 说明 |
|------|------|
| `bootloader_ch592.bin` | 二进制文件 |
| `bootloader_ch592.hex` | Intel HEX |
| `bootloader_ch592.map` | 链接映射 |

### Application

| 文件 | 说明 |
|------|------|
| `tracker_ch592.bin` | 二进制文件 |
| `tracker_ch592.hex` | Intel HEX |
| `tracker_ch592.uf2` | UF2 格式 (需要 Python) |

### 合并

| 文件 | 说明 |
|------|------|
| `tracker_combined.hex` | Bootloader + Application |

## 故障排除

### 错误: 找不到编译器

```
make: riscv-none-elf-gcc: Command not found
```

**解决方案**: 
- 检查 PATH 环境变量
- 使用完整路径: `make PREFIX=/path/to/toolchain/bin/riscv-none-elf-`

### 错误: Bootloader 超过 4KB

```
ERROR: Bootloader exceeds 4KB!
```

**解决方案**:
- 检查代码优化级别 (-Os)
- 移除不必要的功能
- 检查链接脚本

### 错误: 链接失败

```
undefined reference to 'xxx'
```

**解决方案**:
- 检查是否缺少源文件
- 检查函数声明是否正确
- 检查 Makefile 中的源文件列表

## 代码大小参考

| 组件 | 预期大小 |
|------|----------|
| Bootloader | < 2 KB |
| Tracker 固件 | 12-18 KB |
| Receiver 固件 | 9-14 KB |
