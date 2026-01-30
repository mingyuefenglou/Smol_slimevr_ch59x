/**
 * @file config.h
 * @brief SlimeVR CH592 主配置文件
 * 
 * 所有可配置参数集中在此文件
 * All configurable parameters are centralized here
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

/*============================================================================
 * 芯片选择 / Chip Selection
 *============================================================================*/

#ifndef CHIP_TYPE
#define CHIP_TYPE           CH592       // CH592 或 CH591
#endif

/*============================================================================
 * IMU 传感器选择 / IMU Sensor Selection
 * 
 * 可选: ICM45686 (默认), ICM42688, BMI270, LSM6DSV, LSM6DSR
 * Options: ICM45686 (default), ICM42688, BMI270, LSM6DSV, LSM6DSR
 *============================================================================*/

#ifndef IMU_TYPE
#define IMU_TYPE            IMU_ICM45686    // 默认使用 ICM-45686
#endif

// IMU 类型定义 (完整支持列表)
#define IMU_UNKNOWN         0
#define IMU_MPU6050         1       // 经典 6轴
#define IMU_BMI160          2       // Bosch 6轴
#define IMU_BMI270          3       // Bosch 6轴 (推荐)
#define IMU_ICM42688        4       // TDK 6轴
#define IMU_ICM45686        5       // TDK 6轴 (推荐)
#define IMU_LSM6DSV         6       // ST 6轴 (推荐, 媲美BNO085)
#define IMU_LSM6DSR         7       // ST 6轴
#define IMU_LSM6DSO         8       // ST 6轴 (常用)
#define IMU_ICM20948        9       // TDK 9轴
#define IMU_MPU9250         10      // TDK 9轴 (经典)
#define IMU_BMI323          11      // Bosch 6轴 (新款)
#define IMU_ICM42605        12      // TDK 6轴
#define IMU_QMI8658         13      // 国产 6轴
#define IMU_IIM42652        14      // TDK 工业级 6轴 (-40°C~+105°C)
#define IMU_SC7I22          15      // 芯海 国产 6轴 (低成本)
#define IMU_MAX_TYPE        16      // 类型数量

/*============================================================================
 * 传感器融合算法选择 / Sensor Fusion Algorithm Selection
 * 
 * v0.6.2: 支持多种融合算法选择
 *============================================================================*/

// 融合算法类型定义
#define FUSION_VQF_ULTRA        0   // Q15定点优化，最快，96B RAM
#define FUSION_VQF_ADVANCED     1   // 完整VQF，支持磁力计，180B RAM (推荐)
#define FUSION_VQF_OPT          2   // 平衡精度与性能，120B RAM
#define FUSION_VQF_SIMPLE       3   // 基础实现，80B RAM
#define FUSION_EKF              4   // 扩展卡尔曼滤波，200B RAM

// v0.6.2: 默认使用 VQF Advanced (完整功能，支持磁力计)
#ifndef FUSION_TYPE
#define FUSION_TYPE             FUSION_VQF_ADVANCED
#endif

// 兼容旧配置: 取消注释以使用 EKF
// #define USE_EKF_INSTEAD_OF_VQF

/*============================================================================
 * RF 配置 / RF Configuration
 *============================================================================*/

// RF 发射功率 (默认最大功率 +4dBm)
// RF transmit power (default maximum +4dBm)
// 可选值: -20, -16, -12, -8, -4, 0, +3, +4 dBm
#ifndef RF_TX_POWER_DBM
#define RF_TX_POWER_DBM     4           // +4dBm (最大功率)
#endif

// RF 通道 (BLE 广播通道)
#define RF_CHANNEL_37       37          // 2402 MHz
#define RF_CHANNEL_38       38          // 2426 MHz
#define RF_CHANNEL_39       39          // 2480 MHz
#define RF_DEFAULT_CHANNEL  RF_CHANNEL_38

// 数据率 / Data rate
#define RF_DATA_RATE_HZ     200         // 200Hz 传输速率

// 丢包重传次数 / Packet retransmit count
#define RF_RETRANSMIT_COUNT 2           // 每包重传 2 次

// 跳频间隔 / Channel hop interval (ms)
#define RF_HOP_INTERVAL_MS  50          // 每 50ms 跳频

/*============================================================================
 * 引脚配置 / Pin Configuration
 * 
 * CH591D (20-pin QFN) 追踪器引脚分配
 * 
 * ⚠️ 重要：原理图存在引脚标签偏移错误！
 * Pin 3 (VIO33) 标签缺失，导致 Pin 6-11 标签全部偏移一位。
 * 以下定义基于芯片手册的正确引脚分配。
 * 
 * CH591D 引脚映射：
 * Pin 1  = PA11 (INT1)      Pin 11 = PB4 (SW0/BOOT)
 * Pin 2  = PA10 (CHRG)      Pin 12 = X32MO
 * Pin 3  = VIO33 (3.3V)     Pin 13 = X32MI
 * Pin 4  = VDCID            Pin 14 = VINTA
 * Pin 5  = VSW              Pin 15 = ANT
 * Pin 6  = PA8 (ADC)        Pin 16 = VDCIA
 * Pin 7  = PA9 (LED)        Pin 17 = PA15 (MISO)
 * Pin 8  = PB11 (USB D+)    Pin 18 = PA14 (MOSI)
 * Pin 9  = PB10 (USB D-)    Pin 19 = PA13 (CLK)
 * Pin 10 = PB7 (RST)        Pin 20 = PA12 (CS)
 *============================================================================*/

// LED 状态指示灯
// 注意：原理图 Pin 7 标签显示 PA8，但实际是 PA9！
#define PIN_LED             9       // PA9 (Pin 7) - 原理图标签错误!
#define PIN_LED_PORT        GPIOA

// 用户按键 SW0 (原理图连接到 Pin 11)
// 注意：原理图 Pin 11 标签显示 PB7，但实际是 PB4！
// HAL 引脚编码: PA0-PA15=0-15, PB0-PB23=16-39，因此 PB4 = 20
#define PIN_SW0             20      // PB4 (Pin 11) - 原理图标签错误!
#define PIN_SW0_PORT        GPIOB
#define PIN_SW1             PIN_SW0 // 兼容旧代码

// IMU SPI 接口 (CH591D 20-pin 实际连接)
#define PIN_SPI_CS          12      // PA12 - 片选 (硬件固定)
#define PIN_SPI_CLK         13      // PA13 - 时钟 (硬件固定)
#define PIN_SPI_MOSI        14      // PA14 - 数据输出 (硬件固定)
#define PIN_SPI_MISO        15      // PA15 - 数据输入 (硬件固定)

// IMU 中断引脚
// INT1 连接到 Pin 1 (PA11)
#define PIN_IMU_INT1        11      // PA11 (Pin 1) - 数据就绪中断
#define PIN_IMU_INT1_PORT   GPIOA
// INT2 可选，如果使用需要确认实际连接
#define PIN_IMU_INT2        6       // PB6 - 运动检测中断 (可选)

// IMU 时钟同步输出 
// 注意：原理图中没有专门的CLKOUT连接
// 如果需要外部时钟，可以使用PA9的TMR0功能
// #define PIN_CLKOUT          9       // PA9 (Pin 7) - 可用于时钟输出

// 充电检测 (连接到 Pin 2)
// 原理图 CHRG 信号连接到 PA10
#define PIN_CHRG_DET        10      // PA10 (Pin 2) - 连接 TP4054 CHRG 引脚
#define PIN_CHRG_DET_PORT   GPIOA

// ADC 输入 (连接到 Pin 6)
// 原理图 ADC 信号连接到实际的 PA8
#define PIN_ADC_INPUT       8       // PA8 (Pin 6) - ADC/电池检测
#define PIN_ADC_CHANNEL     12      // PA8 对应 ADC 通道 AIN12
#define PIN_VBAT_ADC_CHANNEL 12     // 统一使用: PA8 -> AIN12

// USB 电源检测 (如果使用)
// 注意：CH591D没有专门的USB VBUS检测引脚，需要外部电路
// #define PIN_USB_VBUS        10      // 根据实际硬件配置

// I2C 备用接口 (当使用 I2C 连接 IMU 时)
// CH591D 没有 PB22/PB23，这些是 CH592 的引脚
// 如果需要I2C，请使用软件模拟或其他GPIO
// #define PIN_I2C_SCL         22      // 不适用于 CH591D
// #define PIN_I2C_SDA         23      // 不适用于 CH591D

// 电池电压 ADC 
// 使用 Pin 6 (PA8/AIN12) 进行电池检测
#define PIN_VBAT_ADC        8       // PA8 (Pin 6) - 对应 AIN12

// BOOT/SW0 按键 (连接到 Pin 11)
// 实际是 PB4，用于进入 Bootloader
#define PIN_BOOT            20      // PB4 (Pin 11)

// RST 引脚 (连接到 Pin 10)
// HAL 引脚编码: PB7 = 23
#define PIN_RST             23      // PB7 (Pin 10)

// RST 引脚 (连接到 Pin 10)
#define PIN_RST             7       // PB7 (Pin 10)

/*============================================================================
 * 电源管理配置 / Power Management Configuration
 *============================================================================*/

// 自动休眠 (0 = 禁用, 单位毫秒)
#define AUTO_SLEEP_TIMEOUT_MS   300000  // 5分钟静止后进入深睡眠（v0.6.2更新）

// 长按进入休眠时间
#define LONG_PRESS_SLEEP_MS     3000    // 3 秒

// 低电量阈值
#define BATTERY_LOW_PERCENT     10      // 10%
#define BATTERY_CRITICAL_PERCENT 5      // 5% (强制休眠)

/*============================================================================
 * v0.5.1 WOM唤醒引脚配置 (板级配置化)
 * 注意: WOM引脚不可与SPI-CS复用！
 *============================================================================*/
#define WOM_WAKE_PIN            4       // PA4 - IMU INT1 (WOM唤醒)
#define WOM_WAKE_PORT           GPIO_A
#define WOM_WAKE_EDGE           GPIO_ITMode_RiseEdge  // 统一使用上升沿
#define WOM_WAKE_PULL           GPIO_ModeIN_PD        // 下拉，高电平唤醒
#define WOM_THRESHOLD_MG        200     // WOM阈值 (mg)

// 唤醒源配置
#define WAKE_SOURCE_WOM         (1 << 0)  // IMU WOM中断
#define WAKE_SOURCE_BTN         (1 << 1)  // 按键
#define WAKE_SOURCE_CHRG        (1 << 2)  // 充电插入
#define WAKE_SOURCES_DEFAULT    (WAKE_SOURCE_WOM | WAKE_SOURCE_BTN)

/*============================================================================
 * v0.5.2 动态节流配置 (三档: 200/100/50Hz)
 *============================================================================*/
#define TX_RATE_MOVING          200     // 运动时 200Hz
#define TX_RATE_MICRO_MOTION    100     // 微动时 100Hz  
#define TX_RATE_STATIONARY      50      // 静止时 50Hz
#define TX_RATE_DIVIDER_MOVING  1       // 每1帧发送
#define TX_RATE_DIVIDER_MICRO   2       // 每2帧发送
#define TX_RATE_DIVIDER_STATIC  4       // 每4帧发送

/*============================================================================
 * 采样配置 / Sampling Configuration
 *============================================================================*/

#define SENSOR_ODR_HZ           200     // 传感器采样率
#define FUSION_RATE_HZ          200     // 融合算法运行频率
#define RF_REPORT_RATE_HZ       200     // RF 数据上报频率

/*============================================================================
 * 追踪器配置 / Tracker Configuration
 *============================================================================*/

/*============================================================================
 * Tracker数量配置（v0.4.22 P0修复）
 * 固定为10，以保证200Hz(5ms)超帧的稳定性
 * 时隙预算: SYNC(250us) + GUARD(250us) + 10*SLOT(400us) + TAIL(500us) = 5000us
 *============================================================================*/
#define MAX_TRACKERS            10      // 固定10个tracker，保证200Hz稳定性

// 旧配置（保留参考）
// #ifdef CH591
// #define MAX_TRACKERS            16      // CH591 内存较小
// #else
// #define MAX_TRACKERS            24      // CH592 支持更多
// #endif

/*============================================================================
 * 充电指示灯配置 / Charging LED Configuration
 * 
 * 硬件连接说明 / Hardware Connection:
 * 
 *   TP4054          CH592
 *   ──────          ─────
 *   CHRG ──┬────── PA5 (PIN_CHRG, 输入检测)
 *          │
 *          └────── PA11 (PIN_CHRG_LED, 输出控制)
 *                    │
 *                    └───── LED ────┬──── GND
 *                                   R (330Ω)
 * 
 * 工作原理 / Working Principle:
 * - TP4054 CHRG 引脚: 充电时拉低, 充满或空闲时高阻
 * - PA5 检测 CHRG 状态 (输入)
 * - PA11 可独立控制 LED (输出)
 * - 充电时: PA11 输出低, LED 亮
 * - 充满时: PA11 高阻或高, LED 灭
 * - 可通过软件实现更复杂的指示 (如呼吸灯)
 *============================================================================*/

// 充电指示灯模式
#define CHRG_LED_MODE_TP4054    0       // 完全由 TP4054 控制
#define CHRG_LED_MODE_MCU       1       // 由 MCU 控制

#ifndef CHRG_LED_MODE
#define CHRG_LED_MODE           CHRG_LED_MODE_MCU
#endif

/*============================================================================
 * USB 配置 / USB Configuration
 *============================================================================*/

// USB VID/PID
#define USB_VID                 0x1209  // pid.codes (开源硬件)
#define USB_PID                 0x7690  // SlimeVR CH592

// USB 描述符字符串
#define USB_MANUFACTURER        "SlimeVR"
#define USB_PRODUCT             "SlimeVR CH592 Receiver"

/*============================================================================
 * 调试配置 / Debug Configuration
 *============================================================================*/

// #define DEBUG_ENABLE             // 启用调试输出
// #define DEBUG_UART               // 使用 UART 调试
// #define DEBUG_USB                // 使用 USB CDC 调试

/*============================================================================
 * 功耗分析 / Power Consumption Analysis
 * 
 * 组件功耗估算 (CH592 + ICM45686):
 * 
 * | 组件 | 活动模式 | 休眠模式 |
 * |------|----------|----------|
 * | CH592 MCU (60MHz) | 8.0 mA | 2.0 µA |
 * | CH592 RF TX (+4dBm) | 12.5 mA | - |
 * | CH592 RF 平均 | 3.0 mA | - |
 * | ICM-45686 | 0.65 mA | 3.0 µA |
 * | LED (亮) | 5.0 mA | 0 mA |
 * | 其他 (LDO等) | 1.0 mA | 0.5 µA |
 * | ────────── | ────────── | ────────── |
 * | 总计 (LED亮) | ~17.7 mA | ~5.5 µA |
 * | 总计 (LED灭) | ~12.7 mA | ~5.5 µA |
 * 
 * 150mAh 电池续航估算 (LED 灭):
 * - 活动模式: 150mAh / 12.7mA ≈ 11.8 小时
 * - 混合模式 (90%活动+10%休眠): ≈ 13 小时
 * 
 * 优化建议:
 * 1. 降低 RF 发射功率 (0dBm 可降低 ~3mA)
 * 2. 降低采样率 (100Hz 可降低 ~1mA)
 * 3. 使用间歇传输 (节省 RF 功耗)
 *============================================================================*/

// 功耗常量 (用于电池估算)
#define POWER_MCU_ACTIVE_MA     8.0f
#define POWER_MCU_SLEEP_UA      2.0f
#define POWER_RF_TX_MA          12.5f
#define POWER_RF_AVG_MA         3.0f
#define POWER_IMU_ACTIVE_MA     0.65f
#define POWER_IMU_SLEEP_UA      3.0f
#define POWER_LED_MA            5.0f
#define POWER_OTHER_MA          1.0f

// 总功耗 (mA)
#define POWER_TOTAL_ACTIVE_MA   (POWER_MCU_ACTIVE_MA + POWER_RF_AVG_MA + \
                                  POWER_IMU_ACTIVE_MA + POWER_OTHER_MA)
// = 8.0 + 3.0 + 0.65 + 1.0 = 12.65 mA

/*============================================================================
 * 丢包优化配置 / Packet Loss Optimization
 * 
 * 丢包原因分析:
 * 1. RF 干扰 (WiFi, 蓝牙设备)
 * 2. 距离过远 / 障碍物
 * 3. 接收器处理不及时
 * 4. 通道拥塞
 * 
 * 优化措施:
 * 1. 启用包重传 (RF_RETRANSMIT_COUNT)
 * 2. 跳频机制 (RF_HOP_INTERVAL_MS)
 * 3. 前向纠错 (简单奇偶校验)
 * 4. 接收端插值 (丢包时使用预测值)
 *============================================================================*/

// 丢包优化
#define PACKET_LOSS_ENABLE_RETRANSMIT   1   // 启用重传
#define PACKET_LOSS_ENABLE_FEC          0   // 前向纠错 (增加开销)
#define PACKET_LOSS_ENABLE_INTERPOLATION 1  // 接收端插值

// 丢包检测阈值
#define PACKET_LOSS_THRESHOLD_PERCENT   5   // 超过 5% 认为信号差

/*============================================================================
 * v0.6.2 可选功能开关
 *============================================================================*/

// ============== 传感器读取模式 (三选一) ==============
// 1. USE_SENSOR_OPTIMIZED - FIFO+中断驱动 (推荐，延迟最低)
// 2. USE_SENSOR_DMA - DMA异步读取 (次选)
// 3. 都不定义 - 标准轮询读取 (兼容性最好)
// 注意: USE_SENSOR_OPTIMIZED 和 USE_SENSOR_DMA 不能同时启用!

#define USE_SENSOR_OPTIMIZED    1   // 推荐：FIFO+中断驱动
// #define USE_SENSOR_DMA       0   // 备选：DMA异步读取 (与OPTIMIZED互斥)

// USB调试输出 (通过USB CDC输出调试信息)
#define USE_USB_DEBUG           1

// BLE蓝牙功能 (暂未完整实现)
// #define USE_BLE_SLIMEVR      0

// 诊断模块 (用于性能分析和故障排查)
#define USE_DIAGNOSTICS         1

/*============================================================================
 * v0.6.2 高级功能开关
 *============================================================================*/

// 智能信道管理 (CCA检测+自动避让)
#define USE_CHANNEL_MANAGER     1

// RF自愈机制 (通信中断自动恢复)
#define USE_RF_RECOVERY         1

// ============== RF优化模块 (推荐全部启用) ==============
// 时隙优化器 - 需要手动集成到rf_transmitter
// 时序优化 - 需要手动集成到rf_transmitter
// RF Ultra - 独立数据包格式，需要Receiver配合
// 注意: 这些模块已初始化但未深度集成到rf_transmitter

// 时隙优化器 (动态分配+优先级调度) - 已初始化，待深度集成
#define USE_RF_SLOT_OPTIMIZER   1

// 时序优化 (精确同步+延迟补偿) - 已初始化，待深度集成
#define USE_RF_TIMING_OPT       1

// RF Ultra模式 (极限性能) - 已初始化，待深度集成
#define USE_RF_ULTRA            1

// USB大容量存储 (UF2拖放升级)
#define USE_USB_MSC             1

// 磁力计支持 (航向校正) - 自动检测，未检测到则禁用
#define USE_MAGNETOMETER        1

/*============================================================================
 * 编译时冲突检查
 *============================================================================*/

#if defined(USE_SENSOR_OPTIMIZED) && USE_SENSOR_OPTIMIZED && \
    defined(USE_SENSOR_DMA) && USE_SENSOR_DMA
#error "USE_SENSOR_OPTIMIZED and USE_SENSOR_DMA cannot be enabled simultaneously!"
#endif

#endif /* __CONFIG_H__ */
