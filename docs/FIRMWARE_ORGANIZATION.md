# 固件项目整理说明

## 整理完成内容

### 1. 创建了 `.gitignore` 文件

已创建 `.gitignore` 文件，自动忽略以下内容：
- `build/` 目录（构建中间文件）
- `output/` 目录（固件输出文件）
- `bootloader/build/` 和 `bootloader/output/` 目录
- 所有 `.o`, `.elf`, `.map`, `.bin`, `.hex`, `.uf2` 文件
- 临时文件、日志文件、IDE 配置文件

### 2. 确保目录结构清晰

#### build/ 目录
- **用途**: 存储编译过程中的中间文件
- **结构**:
  ```
  build/
  ├── tracker/        # Tracker 构建文件
  │   ├── src/       # 源文件编译产物
  │   ├── sdk/       # SDK 编译产物
  │   ├── *.elf      # ELF 文件
  │   └── *.map      # 链接映射文件
  └── receiver/      # Receiver 构建文件
      ├── src/
      ├── sdk/
      ├── *.elf
      └── *.map
  ```
- **特点**: 自动创建，已加入 `.gitignore`

#### output/ 目录
- **用途**: 存储最终的可烧录固件文件
- **文件类型**:
  - `.bin` - 二进制格式
  - `.hex` - Intel HEX 格式
  - `.combined.hex` - 含 Bootloader 的完整 HEX（推荐）
  - `.uf2` - UF2 格式（USB DFU）
- **命名规则**: `{target}_{chip}_v{version}.{ext}`
- **特点**: 自动创建，已加入 `.gitignore`

### 3. 构建脚本说明

#### 推荐使用的脚本

1. **build_complete.sh** (Linux/macOS)
   - 功能最完整的构建脚本
   - 支持并行编译、验证、详细输出等
   - 用法: `./build_complete.sh [target] [options]`

2. **build.ps1** (Windows PowerShell)
   - Windows 平台的构建脚本
   - 用法: `.\build.ps1 -Target tracker -Chip CH592`

3. **Makefile**
   - 底层构建配置
   - 用法: `make TARGET=tracker CHIP=CH592`

#### 其他构建脚本

项目根目录下的 `build.sh` 是主构建入口，它会自动调用 `scripts/build/build_complete.sh`。

**建议**: 优先使用 `build.sh` 或 `scripts/build/build_complete.sh`，它功能最完整且维护良好。

### 4. 文档说明

已创建以下文档帮助理解和使用构建系统：

1. **BUILD.md** - 详细的构建系统使用说明
   - 目录结构说明
   - 快速开始指南
   - 构建选项说明
   - 常见问题解答

2. **README_BUILD.md** - 快速参考文档
   - 快速开始命令
   - 常用选项
   - 文件类型说明

3. **docs/BUILD_DIRECTORY_STRUCTURE.md** - 目录结构详细说明
   - 完整的目录树
   - 每个目录的用途
   - 文件类型说明

## 使用建议

### 首次构建

```bash
# Linux/macOS
./build_complete.sh all

# Windows
.\build.ps1 -Target all -Chip CH592
```

### 日常开发

```bash
# 仅编译 Tracker
./build_complete.sh tracker

# 清理后重新编译
./build_complete.sh tracker --clean

# 查看详细输出
./build_complete.sh tracker --verbose
```

### 查找输出文件

所有输出文件在 `output/` 目录中：
- 推荐使用 `*_combined.hex` 文件进行烧录
- `.uf2` 文件用于 USB DFU 更新

## 验证整理结果

### 检查目录是否存在

```bash
# Linux/macOS
test -d build && echo "build/ 存在" || echo "build/ 不存在"
test -d output && echo "output/ 存在" || echo "output/ 不存在"
test -f .gitignore && echo ".gitignore 存在" || echo ".gitignore 不存在"

# Windows PowerShell
Test-Path build
Test-Path output
Test-Path .gitignore
```

### 检查 .gitignore 是否生效

```bash
# 查看被忽略的文件
git status --ignored
```

## 注意事项

1. **build/ 和 output/ 目录已加入 .gitignore**
   - 这些目录不会被版本控制
   - 构建时会自动创建
   - 清理命令会删除这些目录

2. **版本号管理**
   - 版本号从 `VERSION` 文件读取
   - 当前版本: 0.6.2
   - 输出文件名包含版本号

3. **芯片型号**
   - 支持 CH591 和 CH592
   - 默认使用 CH592
   - 可通过 `--mcu=CH591` 或 `CHIP=CH591` 指定

4. **构建脚本选择**
   - 推荐使用 `build_complete.sh`（功能最全）
   - 或直接使用 `Makefile`（最底层）

## 下一步

1. 运行一次完整构建验证系统正常
2. 查看输出文件确认格式正确
3. 根据需要调整构建选项

---

**整理完成时间**: 2026-01-30
**整理内容**: 目录结构、构建脚本、文档说明

