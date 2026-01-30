# 项目文件整理说明

## 整理完成

### 1. 目录结构整理

#### 构建脚本目录
- **scripts/build/** - 所有构建脚本
  - `build_complete.sh` - 完整构建脚本（推荐，Linux/macOS）
  - `build_complete.ps1` - Windows完整构建脚本

#### 测试脚本目录
- **scripts/test/** - 测试和检查脚本
  - `test_compile.sh` - 编译测试
  - `full_syntax_check.sh` - 语法检查
  - `compile.sh` - 编译脚本

#### 文档目录
- **docs/** - 所有项目文档
  - `BUILD.md` - 构建系统说明
  - `README_BUILD.md` - 构建快速参考
  - `BUILD_DIRECTORY_STRUCTURE.md` - 目录结构说明
  - `FIRMWARE_ORGANIZATION.md` - 固件整理说明
  - `FIRMWARE_FEATURES.md` - 固件功能说明
  - `FIRMWARE_STATUS_REPORT.md` - 固件状态报告
  - `BUILD_ERROR_LOG.md` - 构建错误日志
  - 其他技术文档...

#### 根目录保留文件
- `build.sh` - 主构建入口（调用 scripts/build/build_complete.sh）
- `build.ps1` - Windows构建入口
- `Makefile` - 主构建配置
- `README.md` - 项目说明
- `CHANGELOG.md` - 更新日志
- `VERSION` - 版本号文件

### 2. 文件清理

#### 已移动的文件
- 所有构建脚本 → `scripts/build/`
- 所有测试脚本 → `scripts/test/`
- 所有文档文件 → `docs/`
- 日志文件 → `docs/`

#### 已删除的内容
- 所有文档中的emoji符号已删除

### 3. 使用方式

#### 构建固件
```bash
# 使用主入口脚本（推荐）
./build.sh all

# 或直接使用完整脚本
./scripts/build/build_complete.sh all

# Windows
.\build.ps1 -Target all -Chip CH592
```

#### 查看文档
所有文档在 `docs/` 目录中，包括：
- 构建指南
- 功能说明
- 协议文档
- 硬件设计
- 等等

### 4. 目录结构

```
项目根目录/
├── scripts/              # 脚本目录
│   ├── build/          # 构建脚本
│   └── test/           # 测试脚本
├── docs/                # 文档目录
├── src/                 # 源代码
├── include/             # 头文件
├── sdk/                 # SDK文件
├── tools/               # 工具脚本
├── bootloader/          # Bootloader
├── build/               # 构建输出（已忽略）
├── output/              # 固件输出（已忽略）
├── build.sh             # 主构建入口
├── build.ps1            # Windows构建入口
└── Makefile             # 构建配置
```

## 注意事项

1. **构建脚本路径**: 根目录的 `build.sh` 会自动调用 `scripts/build/build_complete.sh`
2. **文档位置**: 所有文档已整理到 `docs/` 目录
3. **日志文件**: 构建日志在 `docs/build_output.log`
4. **清理**: 使用 `./build.sh clean` 清理构建文件

## 后续维护

- 新增构建脚本应放在 `scripts/build/` 目录
- 新增测试脚本应放在 `scripts/test/` 目录
- 新增文档应放在 `docs/` 目录
- 避免在文档中使用emoji符号

