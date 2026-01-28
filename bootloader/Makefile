#============================================================================
# SlimeVR CH59X Bootloader Makefile
#============================================================================

# 目标
TARGET = bootloader_ch592

# 工具链
PREFIX ?= riscv-none-elf-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump
SIZE = $(PREFIX)size

# 目录
SRC_DIR = src
BUILD_DIR = build
OUTPUT_DIR = output

# 源文件
C_SOURCES = $(wildcard $(SRC_DIR)/*.c)
ASM_SOURCES = $(wildcard $(SRC_DIR)/*.S)

# 目标文件
OBJECTS = $(addprefix $(BUILD_DIR)/, $(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/, $(notdir $(ASM_SOURCES:.S=.o)))

# 链接脚本
LDSCRIPT = Link_boot.ld

# 编译标志 - 优化大小
# 添加 zicsr 和 zifencei 扩展以支持 CSR 指令和 fence.i
CFLAGS = -march=rv32imac_zicsr_zifencei -mabi=ilp32
CFLAGS += -Os -ffunction-sections -fdata-sections -fno-common
CFLAGS += -Wall -Wno-unused-function
CFLAGS += -nostdlib -ffreestanding

# 汇编标志
ASFLAGS = -march=rv32imac_zicsr_zifencei -mabi=ilp32

# 链接标志
LDFLAGS = -T $(LDSCRIPT) -nostdlib
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-Map=$(BUILD_DIR)/$(TARGET).map

#============================================================================
# 构建规则
#============================================================================

all: $(OUTPUT_DIR)/$(TARGET).bin $(OUTPUT_DIR)/$(TARGET).hex

# 创建目录
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

# 编译 C 文件
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 编译汇编文件
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.S | $(BUILD_DIR)
	$(AS) $(ASFLAGS) -c $< -o $@

# 链接
$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
	$(SIZE) $@

# 生成 BIN
$(OUTPUT_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf | $(OUTPUT_DIR)
	$(OBJCOPY) -O binary $< $@
	@echo "Bootloader size: $$(stat -c%s $@) bytes (max 4096)"
	@if [ $$(stat -c%s $@) -gt 4096 ]; then \
		echo "ERROR: Bootloader exceeds 4KB!"; \
		exit 1; \
	fi

# 生成 HEX
$(OUTPUT_DIR)/$(TARGET).hex: $(BUILD_DIR)/$(TARGET).elf | $(OUTPUT_DIR)
	$(OBJCOPY) -O ihex $< $@

# 反汇编
disasm: $(BUILD_DIR)/$(TARGET).elf
	$(OBJDUMP) -d -S $< > $(BUILD_DIR)/$(TARGET).lst

# 清理
clean:
	rm -rf $(BUILD_DIR) $(OUTPUT_DIR)

# 信息
info:
	@echo "Bootloader for CH592"
	@echo "  Flash: 0x00000000 - 0x00000FFF (4KB)"
	@echo "  App:   0x00001000 onwards"

.PHONY: all clean disasm info
