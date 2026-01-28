# =============================================================================
# SlimeVR CH59X 私有 RF 系统 Makefile
# SlimeVR CH59X Private RF System Makefile
#
# 支持编译接收器 (Dongle) 和追踪器 (Tracker) 固件
# Supports building both Receiver (Dongle) and Tracker firmware
#
# 用法 / Usage:
#   make TARGET=tracker    # 编译追踪器 / Build tracker
#   make TARGET=receiver   # 编译接收器 / Build receiver
#   make clean             # 清理 / Clean
#   make all               # 编译全部 / Build all
# =============================================================================

#==============================================================================
# 操作系统检测 / OS Detection
#==============================================================================
ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
    MKDIR = if not exist $(subst /,\,$(1)) (mkdir $(subst /,\,$(1)))
    RM = del /Q /S
    RMDIR = rmdir /Q /S
    CP = copy
    PYTHON = python
    SHELL_REDIRECT = 2>nul || exit 0
else
    DETECTED_OS := $(shell uname -s)
    MKDIR = mkdir -p
    RM = rm -f
    RMDIR = rm -rf
    CP = cp
    PYTHON = python3
    SHELL_REDIRECT = 2>/dev/null || true
endif

#==============================================================================
# 配置区 / Configuration Section
#==============================================================================

# 构建目标: receiver (接收器) 或 tracker (追踪器)
# Build target: receiver or tracker
TARGET ?= tracker

# 根据目标设置项目名称
# Set project name based on target
ifeq ($(TARGET),receiver)
    PROJECT = slimevr_receiver
else
    PROJECT = slimevr_tracker
endif

# 目标芯片: CH592 (默认) 或 CH591
# Target chip: CH592 (default) or CH591
CHIP ?= CH592

# 构建目录 / Build directory
BUILD_DIR = build/$(TARGET)

# 输出目录 / Output directory
OUTPUT_DIR = output

#==============================================================================
# 版本信息 / Version Information
# 
# 版本号来源优先级:
# 1. 命令行指定: make VERSION=x.y.z
# 2. GitHub 自动检测 (如果有网络)
# 3. 默认值: 0.4.2
#
# 参考: SlimeVR-Tracker-nRF-Receiver
#==============================================================================

# 尝试从 GitHub 获取版本 (如果未指定且有网络)
ifndef VERSION
    # 尝试检测 GitHub 版本 (静默失败)
    GITHUB_VERSION := $(shell curl -s --connect-timeout 2 \
        https://api.github.com/repos/SlimeVR/SlimeVR-Tracker-nRF-Receiver/releases/latest 2>/dev/null | \
        grep -oP '"tag_name":\s*"v?\K[^"]+' 2>/dev/null)
    
    # 如果检测到版本则使用，否则使用默认值
    ifeq ($(GITHUB_VERSION),)
        VERSION = 0.4.2
        $(info [Version] Using default: $(VERSION))
    else
        VERSION = $(GITHUB_VERSION)
        $(info [Version] From GitHub: $(VERSION))
    endif
endif

# 解析版本号
VERSION_PARTS := $(subst ., ,$(VERSION))
VERSION_MAJOR := $(word 1,$(VERSION_PARTS))
VERSION_MINOR := $(word 2,$(VERSION_PARTS))
VERSION_PATCH := $(word 3,$(VERSION_PARTS))

# 版本号默认值
VERSION_MAJOR ?= 0
VERSION_MINOR ?= 4
VERSION_PATCH ?= 2

#==============================================================================
# 工具链配置 / Toolchain Configuration
#==============================================================================

CROSS_COMPILE ?= riscv-none-elf-
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)gcc
LD = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump
SIZE = $(CROSS_COMPILE)size

#==============================================================================
# 源文件定义 / Source Files Definition
#==============================================================================

# HAL 源文件 / HAL sources (完整列表)
HAL_SRC = \
    src/hal/hal_i2c.c \
    src/hal/hal_spi.c \
    src/hal/hal_gpio.c \
    src/hal/hal_timer.c \
    src/hal/hal_storage.c \
    src/hal/hal_watchdog.c \
    src/hal/hal_power.c \
    src/hal/hal_dma.c \
    src/hal/hal_charging.c \
    src/hal/event_logger.c \
    src/hal/diagnostics.c \
    src/hal/retained_state.c \
    src/hal/power_optimizer.c

# RF 协议源文件 / RF protocol sources
RF_SRC = src/rf/rf_hw.c src/rf/rf_common.c
RF_OPT_SRC = \
    src/rf/rf_ultra.c \
    src/rf/channel_manager.c \
    src/rf/rf_recovery.c \
    src/rf/rf_slot_optimizer.c \
    src/rf/rf_timing_opt.c

# 传感器源文件 (仅追踪器) / Sensor sources (tracker only)
# IMU 条件编译: make IMU=ICM45686 编译单一驱动
# IMU conditional compilation: make IMU=ICM45686 to build single driver
IMU ?= ALL

ifeq ($(IMU),ALL)
    SENSOR_SRC = \
        src/sensor/imu/bmi270.c \
        src/sensor/imu/bmi323.c \
        src/sensor/imu/icm42688.c \
        src/sensor/imu/icm45686.c \
        src/sensor/imu/lsm6dso.c \
        src/sensor/imu/lsm6dsr.c \
        src/sensor/imu/lsm6dsv.c \
        src/sensor/imu/iim42652.c \
        src/sensor/imu/sc7i22.c
else ifeq ($(IMU),ICM45686)
    SENSOR_SRC = src/sensor/imu/icm45686.c
    DEFINES += -DUSE_IMU_ICM45686_ONLY
else ifeq ($(IMU),ICM42688)
    SENSOR_SRC = src/sensor/imu/icm42688.c
    DEFINES += -DUSE_IMU_ICM42688_ONLY
else ifeq ($(IMU),BMI270)
    SENSOR_SRC = src/sensor/imu/bmi270.c
    DEFINES += -DUSE_IMU_BMI270_ONLY
else ifeq ($(IMU),BMI323)
    SENSOR_SRC = src/sensor/imu/bmi323.c
    DEFINES += -DUSE_IMU_BMI323_ONLY
else ifeq ($(IMU),LSM6DSO)
    SENSOR_SRC = src/sensor/imu/lsm6dso.c
    DEFINES += -DUSE_IMU_LSM6DSO_ONLY
else ifeq ($(IMU),LSM6DSR)
    SENSOR_SRC = src/sensor/imu/lsm6dsr.c
    DEFINES += -DUSE_IMU_LSM6DSR_ONLY
else ifeq ($(IMU),LSM6DSV)
    SENSOR_SRC = src/sensor/imu/lsm6dsv.c
    DEFINES += -DUSE_IMU_LSM6DSV_ONLY
else ifeq ($(IMU),IIM42652)
    SENSOR_SRC = src/sensor/imu/iim42652.c
    DEFINES += -DUSE_IMU_IIM42652_ONLY
else ifeq ($(IMU),SC7I22)
    SENSOR_SRC = src/sensor/imu/sc7i22.c
    DEFINES += -DUSE_IMU_SC7I22_ONLY
endif

SENSOR_SRC += src/sensor/fusion/vqf_ultra.c
SENSOR_SRC += src/sensor/fusion/vqf_advanced.c
SENSOR_SRC += src/sensor/fusion/vqf_opt.c
SENSOR_SRC += src/sensor/fusion/vqf_simple.c
SENSOR_SRC += src/sensor/fusion/ekf_ahrs.c
SENSOR_SRC += src/sensor/imu_interface.c
SENSOR_SRC += src/sensor/gyro_noise_filter.c
SENSOR_SRC += src/sensor/auto_calibration.c
SENSOR_SRC += src/sensor/temp_compensation.c
SENSOR_SRC += src/sensor/sensor_optimized.c
SENSOR_SRC += src/sensor/mag_interface.c

# SDK 驱动源文件 / SDK driver sources
SDK_SRC = \
    sdk/StdPeriphDriver/CH59x_gpio.c \
    sdk/StdPeriphDriver/CH59x_clk.c \
    sdk/StdPeriphDriver/CH59x_timer.c \
    sdk/StdPeriphDriver/CH59x_spi.c \
    sdk/StdPeriphDriver/CH59x_i2c.c \
    sdk/StdPeriphDriver/CH59x_flash.c \
    sdk/StdPeriphDriver/CH59x_usbdev.c \
    sdk/StdPeriphDriver/CH59x_pwr.c \
    sdk/StdPeriphDriver/CH59x_adc.c

RVMSIS_SRC = $(wildcard sdk/RVMSIS/*.c)

# 目标特定源文件 / Target specific sources
ifeq ($(TARGET),receiver)
    # Receiver: RF接收 + USB HID + Bootloader (MSC DFU) + SlimeVR协议
    APP_SRC = src/main_receiver.c \
              src/rf/rf_receiver.c \
              src/rf/rf_protocol_enhanced.c \
              src/usb/usb_hid_slime.c \
              src/usb/usb_bootloader.c \
              src/usb/usb_msc.c \
              src/usb/slime_packet.c \
              src/usb/usb_debug.c
    DEFINES += -DBUILD_RECEIVER
else
    # Tracker: RF发送 + 传感器 + Bootloader (MSC DFU) + 调试
    APP_SRC = src/main_tracker.c \
              src/rf/rf_transmitter.c \
              src/usb/usb_bootloader.c \
              src/usb/usb_msc.c \
              src/usb/usb_hid_slime.c \
              src/usb/usb_debug.c \
              $(SENSOR_SRC)
    DEFINES += -DBUILD_TRACKER
endif

STARTUP_SRC = sdk/Startup/startup_CH592.S
SOURCES = $(APP_SRC) $(HAL_SRC) $(RF_SRC) $(RF_OPT_SRC) $(SDK_SRC) $(RVMSIS_SRC)
ASM_SOURCES = $(STARTUP_SRC)

#==============================================================================
# 编译器标志 / Compiler Flags
#==============================================================================

INCLUDES = -Iinclude -Isrc -Isdk/StdPeriphDriver/inc -Isdk/RVMSIS -Isdk/BLE/LIB -Isdk/BLE/HAL/include

CPU_FLAGS = -march=rv32imac_zicsr_zifencei -mabi=ilp32 -msmall-data-limit=8
OPT = -Os
SIZE_OPT = -ffunction-sections -fdata-sections -fno-common -fno-exceptions -fshort-enums
WARNINGS = -Wall -Wextra -Wno-unused-parameter -Wno-unused-function

# 链接时优化 (LTO) - 代码大小减少5-10%
# Link-Time Optimization - reduces code size by 5-10%
LTO_FLAGS ?= -flto

# 依赖文件生成 - 增量编译优化
# Dependency file generation - incremental compilation optimization
DEP_FLAGS = -MMD -MP

DEFINES += -D$(CHIP) -DCH59X -DVERSION_MAJOR=$(VERSION_MAJOR) -DVERSION_MINOR=$(VERSION_MINOR) -DVERSION_PATCH=$(VERSION_PATCH)

# CH591 优化 (较小内存) / CH591 optimization (less memory)
ifeq ($(CHIP),CH591)
    DEFINES += -DMAX_TRACKERS=16 -DFLASH_SIZE=256K -DRAM_SIZE=18K
else
    DEFINES += -DMAX_TRACKERS=24 -DFLASH_SIZE=448K -DRAM_SIZE=26K
endif

CFLAGS = $(CPU_FLAGS) $(OPT) $(SIZE_OPT) $(WARNINGS) $(DEFINES) $(INCLUDES) $(LTO_FLAGS) $(DEP_FLAGS)
ASFLAGS = $(CPU_FLAGS) $(DEFINES) $(INCLUDES)
# 添加 -lm 链接数学库 (sin, cos, atan2等)
LDFLAGS = $(CPU_FLAGS) -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map -Tsdk/Ld/Link.ld -nostartfiles --specs=nano.specs --specs=nosys.specs $(LTO_FLAGS) -lm -u _printf_float

#==============================================================================
# 目标文件 / Object Files
#==============================================================================

OBJECTS = $(SOURCES:%.c=$(BUILD_DIR)/%.o)
ASM_OBJECTS = $(ASM_SOURCES:%.S=$(BUILD_DIR)/%.o)
DEPS = $(OBJECTS:.o=.d)

ELF = $(BUILD_DIR)/$(PROJECT).elf
BIN = $(OUTPUT_DIR)/$(TARGET)_$(CHIP)_v$(VERSION).bin
HEX = $(OUTPUT_DIR)/$(TARGET)_$(CHIP)_v$(VERSION).hex
UF2 = $(OUTPUT_DIR)/$(TARGET)_$(CHIP)_v$(VERSION).uf2

# 包含依赖文件 (增量编译优化)
# Include dependency files (incremental compilation optimization)
-include $(DEPS)

#==============================================================================
# 构建规则 / Build Rules
#==============================================================================

.PHONY: all clean tracker receiver both ch591 info flash help

all: $(BIN) $(HEX) $(UF2)

$(ELF): $(OBJECTS) $(ASM_OBJECTS)
	@echo "[LD] 链接 / Linking $(PROJECT)..."
ifeq ($(DETECTED_OS),Windows)
	@$(call MKDIR,$(dir $@))
else
	@mkdir -p $(dir $@)
endif
	$(CC) $(LDFLAGS) $^ -o $@ -lm
	$(SIZE) $@

$(BIN): $(ELF)
	@echo "[BIN] 生成 / Generating $@..."
ifeq ($(DETECTED_OS),Windows)
	@$(call MKDIR,$(dir $@))
else
	@mkdir -p $(dir $@)
endif
	$(OBJCOPY) -O binary $< $@
ifeq ($(DETECTED_OS),Windows)
	@$(CP) $@ $(OUTPUT_DIR)\$(TARGET).bin $(SHELL_REDIRECT)
else
	@ln -sf $(notdir $@) $(OUTPUT_DIR)/$(TARGET).bin $(SHELL_REDIRECT)
endif

$(HEX): $(ELF)
	@echo "[HEX] 生成 / Generating $@..."
ifeq ($(DETECTED_OS),Windows)
	@$(call MKDIR,$(dir $@))
else
	@mkdir -p $(dir $@)
endif
	$(OBJCOPY) -O ihex $< $@
ifeq ($(DETECTED_OS),Windows)
	@$(CP) $@ $(OUTPUT_DIR)\$(TARGET).hex $(SHELL_REDIRECT)
else
	@ln -sf $(notdir $@) $(OUTPUT_DIR)/$(TARGET).hex $(SHELL_REDIRECT)
endif

$(UF2): $(BIN)
	@echo "[UF2] 生成 / Generating $@..."
ifeq ($(DETECTED_OS),Windows)
	@if exist tools\bin2uf2.py $(PYTHON) tools\bin2uf2.py $< $@ 0x4b50e11a 0x00001000
	@if exist $@ $(CP) $@ $(OUTPUT_DIR)\$(TARGET).uf2 $(SHELL_REDIRECT)
else
	@if [ -f tools/bin2uf2.py ]; then $(PYTHON) tools/bin2uf2.py $< $@ 0x4b50e11a 0x00001000; fi
	@ln -sf $(notdir $@) $(OUTPUT_DIR)/$(TARGET).uf2 $(SHELL_REDIRECT)
endif

$(BUILD_DIR)/%.o: %.c
	@echo "[CC] $<"
ifeq ($(DETECTED_OS),Windows)
	@$(call MKDIR,$(dir $@))
else
	@mkdir -p $(dir $@)
endif
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@echo "[AS] $<"
ifeq ($(DETECTED_OS),Windows)
	@$(call MKDIR,$(dir $@))
else
	@mkdir -p $(dir $@)
endif
	$(AS) $(ASFLAGS) -c $< -o $@

clean:
	@echo "[CLEAN] 清理构建文件 / Cleaning build files..."
ifeq ($(DETECTED_OS),Windows)
	@if exist build $(RMDIR) build
else
	@rm -rf build
endif

cleanall: clean
	@echo "[CLEAN] 清理输出文件 / Cleaning output files..."
	rm -f $(OUTPUT_DIR)/*.bin $(OUTPUT_DIR)/*.hex $(OUTPUT_DIR)/*.elf $(OUTPUT_DIR)/*.map $(OUTPUT_DIR)/*.uf2

tracker:
	$(MAKE) TARGET=tracker

receiver:
	$(MAKE) TARGET=receiver

both:
	$(MAKE) TARGET=tracker
	$(MAKE) TARGET=receiver

ch591:
	$(MAKE) CHIP=CH591 TARGET=tracker
	$(MAKE) CHIP=CH591 TARGET=receiver

info:
	@echo "目标/Target: $(TARGET), 芯片/Chip: $(CHIP), 版本/Version: v$(VERSION)"

flash: $(BIN)
	wchisp flash $<

help:
	@echo "make tracker/receiver/both/clean/ch591/flash/info"

# EKF 算法 (可选) / EKF algorithm (optional)
# 取消注释以使用卡尔曼滤波 / Uncomment to use Kalman filter
# SENSOR_SRC += src/sensor/fusion/ekf_ahrs.c
# DEFINES += -DUSE_EKF_INSTEAD_OF_VQF

# LSM6DSV/LSM6DSR 驱动 (可选)
# LSM6DSV/LSM6DSR drivers (optional)
# 取消注释以启用 / Uncomment to enable:
# SENSOR_SRC += src/sensor/imu/lsm6dsv.c
# SENSOR_SRC += src/sensor/imu/lsm6dsr.c

# 充电检测驱动 / Charging detection driver
HAL_SRC += src/hal/hal_charging.c

# 陀螺仪噪音滤波器 / Gyro noise filter
SENSOR_SRC += src/sensor/gyro_noise_filter.c

# IMU 统一接口层 / IMU unified interface
SENSOR_SRC += src/sensor/imu_interface.c

# RF Ultra v2 / RF Ultra v2 protocol
RF_SRC += src/rf/rf_ultra_v2.c

# RF 发射器 v2 优化版 / RF transmitter v2 optimized
RF_SRC += src/rf/rf_transmitter_v2.c

# RF 时隙优化器 / RF slot optimizer
RF_SRC += src/rf/rf_slot_optimizer.c

# RF 时序优化 / RF timing optimization
RF_SRC += src/rf/rf_timing_opt.c

#==============================================================================
# 优化模块 / Optimization Modules
#==============================================================================

# DMA 优化 / DMA optimization
HAL_SRC += src/hal/hal_dma.c

# 电源优化器 / Power optimizer
HAL_SRC += src/hal/power_optimizer.c

# 传感器优化 / Sensor optimization
SENSOR_SRC += src/sensor/sensor_optimized.c

# 传感器 DMA / Sensor DMA
SENSOR_SRC += src/sensor/sensor_dma.c

# 自动校准 / Auto calibration
SENSOR_SRC += src/sensor/auto_calibration.c

# 温度补偿 / Temperature compensation
SENSOR_SRC += src/sensor/temp_compensation.c

# 磁力计接口 / Magnetometer interface
SENSOR_SRC += src/sensor/mag_interface.c

#==============================================================================
# USB 模块 / USB Modules
#==============================================================================

# USB 调试接口 / USB debug interface
USB_SRC = src/usb/usb_debug.c

# USB 协议 / USB protocol
USB_SRC += src/usb/usb_protocol.c

# OTA 更新 (可选) / OTA update (optional)
# 使用 make OTA=1 启用
ifdef OTA
    USB_SRC += src/usb/ota_update.c
    DEFINES += -DOTA_ENABLED=1
endif

# 添加 USB 源文件到应用
ifeq ($(TARGET),receiver)
    APP_SRC += $(USB_SRC)
else
    APP_SRC += $(USB_SRC)
endif
