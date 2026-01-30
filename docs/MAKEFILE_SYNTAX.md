# Makefile 语法说明

## Makefile 使用什么语言？

Makefile 使用 **Makefile 语法**（也称为 Make 语法），这是一种专门用于构建系统的领域特定语言（DSL）。

## 基本语法元素

### 1. 变量定义

```makefile
# 简单变量
TARGET = tracker
CHIP = CH591
BOARD = mingyue_ch591d

# 条件赋值（如果变量未定义才赋值）
CROSS_COMPILE ?= riscv-none-elf-

# 变量引用
CC = $(CROSS_COMPILE)gcc
```

### 2. 条件判断

```makefile
# ifeq / else / endif
ifeq ($(TARGET),receiver)
    PROJECT = slimevr_receiver
else
    PROJECT = slimevr_tracker
endif

# ifdef / ifndef（检查变量是否定义）
ifndef VERSION
    VERSION = 0.4.2
endif
```

### 3. 函数调用

```makefile
# shell 函数：执行 shell 命令
DETECTED_OS := $(shell uname -s)

# subst 函数：字符串替换
VERSION_PARTS := $(subst ., ,$(VERSION))

# word 函数：获取单词
VERSION_MAJOR := $(word 1,$(VERSION_PARTS))

# call 函数：调用自定义函数
$(call MKDIR,$(dir $@))
```

### 4. 目标（Target）和规则（Rule）

```makefile
# 基本格式：
# target: prerequisites
#     command

all: $(BIN) $(HEX) $(UF2)

$(ELF): $(OBJECTS) $(ASM_OBJECTS)
	@echo "[LD] 链接 / Linking $(PROJECT)..."
	$(CC) $(LDFLAGS) $^ -o $@ -lm
	$(SIZE) $@
```

**说明：**
- `target`: 目标名称
- `prerequisites`: 依赖项（用空格分隔）
- `command`: 执行的命令（必须以 Tab 开头，不能用空格）
- `@`: 不显示命令本身，只显示输出
- `$^`: 所有依赖项
- `$@`: 目标文件名
- `$<`: 第一个依赖项

### 5. 模式规则（Pattern Rules）

```makefile
# 编译所有 .c 文件为 .o 文件
$(BUILD_DIR)/%.o: %.c
	@echo "[CC] $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# 编译所有 .S 文件为 .o 文件
$(BUILD_DIR)/%.o: %.S
	@echo "[AS] $<"
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@
```

**说明：**
- `%`: 通配符，匹配任意字符串
- `%.c`: 匹配所有 .c 文件
- `$<`: 输入文件（%.c 对应的文件）

### 6. 伪目标（Phony Targets）

```makefile
.PHONY: all clean tracker receiver help

clean:
	@echo "[CLEAN] 清理构建文件..."
	rm -rf build
```

**说明：**
- `.PHONY`: 声明这不是实际文件，而是命令
- 即使存在同名文件，也会执行命令

### 7. 包含文件（Include）

```makefile
# 包含依赖文件（用于增量编译）
-include $(DEPS)
```

**说明：**
- `-include`: 如果文件不存在也不报错
- 用于自动生成的依赖文件

## 当前 Makefile 的关键部分

### 1. 变量定义和配置

```makefile
# 构建目标
TARGET ?= tracker

# 板子选择
BOARD ?= mingyue_ch591d

# 工具链
CROSS_COMPILE ?= riscv-none-elf-
CC = $(CROSS_COMPILE)gcc
```

### 2. 源文件列表

```makefile
# HAL 源文件
HAL_SRC = \
    src/hal/hal_i2c.c \
    src/hal/hal_spi.c \
    ...

# 根据目标选择源文件
ifeq ($(TARGET),receiver)
    APP_SRC = src/main_receiver.c ...
else
    APP_SRC = src/main_tracker.c ...
endif
```

### 3. 编译标志

```makefile
# CPU 架构
CPU_FLAGS = -march=rv32imac_zicsr_zifencei -mabi=ilp32

# 优化选项
OPT = -Os
SIZE_OPT = -ffunction-sections -fdata-sections

# 编译标志组合
CFLAGS = $(CPU_FLAGS) $(OPT) $(SIZE_OPT) $(WARNINGS) $(DEFINES) $(INCLUDES)
```

### 4. 构建规则

```makefile
# 链接 ELF 文件
$(ELF): $(OBJECTS) $(ASM_OBJECTS)
	$(CC) $(LDFLAGS) $^ -o $@ -lm

# 生成 BIN 文件
$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

# 生成 HEX 文件
$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@
```

## 常用命令

### 编译

```bash
# 编译 Tracker
make TARGET=tracker BOARD=mingyue_ch591d

# 编译 Receiver
make TARGET=receiver BOARD=mingyue_ch592x

# 编译所有
make all
```

### 清理

```bash
# 清理构建文件
make clean

# 清理所有（包括输出文件）
make cleanall
```

### 信息

```bash
# 显示构建信息
make info

# 显示帮助
make help
```

## 特殊变量

| 变量 | 说明 |
|------|------|
| `$@` | 目标文件名 |
| `$<` | 第一个依赖项 |
| `$^` | 所有依赖项（去重） |
| `$+` | 所有依赖项（不去重） |
| `$?` | 比目标新的依赖项 |
| `$*` | 模式匹配的部分 |
| `$(@D)` | 目标文件的目录部分 |
| `$(@F)` | 目标文件的文件名部分 |

## 示例：理解编译流程

当你运行 `make TARGET=tracker BOARD=mingyue_ch591d` 时：

1. **变量设置**
   ```makefile
   TARGET = tracker
   BOARD = mingyue_ch591d
   CHIP = CH591  # 根据 BOARD 自动设置
   ```

2. **源文件收集**
   ```makefile
   SOURCES = $(APP_SRC) $(HAL_SRC) $(RF_SRC) ...
   OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)/%.o)
   ```

3. **编译每个 .c 文件**
   ```makefile
   build/tracker/src/main_tracker.o: src/main_tracker.c
       riscv-none-elf-gcc -c src/main_tracker.c -o build/tracker/src/main_tracker.o
   ```

4. **链接所有 .o 文件**
   ```makefile
   build/tracker/slimevr_tracker.elf: $(OBJECTS)
       riscv-none-elf-gcc $(LDFLAGS) $(OBJECTS) -o build/tracker/slimevr_tracker.elf
   ```

5. **生成最终文件**
   ```makefile
   output/tracker_CH591_v0.4.2.bin: build/tracker/slimevr_tracker.elf
       riscv-none-elf-objcopy -O binary $< $@
   ```

## 调试技巧

### 1. 显示变量值

```makefile
debug:
	@echo "TARGET = $(TARGET)"
	@echo "BOARD = $(BOARD)"
	@echo "CHIP = $(CHIP)"
	@echo "SOURCES = $(SOURCES)"
```

运行：`make debug TARGET=tracker BOARD=mingyue_ch591d`

### 2. 显示执行的命令

```bash
# 不显示命令（默认）
make TARGET=tracker

# 显示所有命令
make TARGET=tracker -n  # 只显示，不执行
make TARGET=tracker V=1  # 如果 Makefile 支持
```

### 3. 详细输出

```makefile
# 在 Makefile 中
ifeq ($(V),1)
    Q :=
else
    Q := @
endif

$(BUILD_DIR)/%.o: %.c
	$(Q)echo "[CC] $<"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
```

## 常见问题

### 1. Tab vs 空格

**错误：**
```makefile
target:
    command  # 使用空格缩进（错误！）
```

**正确：**
```makefile
target:
	command  # 使用 Tab 缩进（正确！）
```

### 2. 变量展开时机

```makefile
# 立即展开（:=）
VAR := $(shell date)

# 延迟展开（=）
VAR = $(shell date)  # 每次使用时才执行
```

### 3. 条件判断语法

```makefile
# 正确
ifeq ($(VAR),value)
    ...
endif

# 错误
if ($(VAR) == value)  # Makefile 不支持这种语法
    ...
endif
```

## 参考资源

- [GNU Make 官方文档](https://www.gnu.org/software/make/manual/)
- [Makefile 教程](https://makefiletutorial.com/)
- [CMake vs Make](https://cmake.org/cmake/help/latest/manual/cmake.1.html)

