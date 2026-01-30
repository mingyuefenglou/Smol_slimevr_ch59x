/**
 * @file config.h
 * @brief CH591D 板子配置文件
 * 
 * CH591D (20-pin QFN) 板子配置
 * 所有可配置参数集中在此文件
 */

#ifndef __BOARD_CH591D_CONFIG_H__
#define __BOARD_CH591D_CONFIG_H__

/*============================================================================
 * 芯片选择 / Chip Selection
 *============================================================================*/
#define CHIP_TYPE           CH591

/*============================================================================
 * IMU 传感器选择 / IMU Sensor Selection
 *============================================================================*/
#ifndef IMU_TYPE
#define IMU_TYPE            IMU_ICM45686    // 默认使用 ICM-45686
#endif

// IMU 类型定义
#define IMU_UNKNOWN         0
#define IMU_MPU6050         1
#define IMU_BMI160          2
#define IMU_BMI270          3
#define IMU_ICM42688        4
#define IMU_ICM45686        5
#define IMU_LSM6DSV         6
#define IMU_LSM6DSR         7
#define IMU_LSM6DSO         8
#define IMU_ICM20948        9
#define IMU_MPU9250         10
#define IMU_BMI323          11
#define IMU_ICM42605        12
#define IMU_QMI8658         13
#define IMU_IIM42652        14
#define IMU_SC7I22          15
#define IMU_MAX_TYPE        16

/*============================================================================
 * 传感器融合算法选择 / Sensor Fusion Algorithm Selection
 *============================================================================*/
#define FUSION_VQF_ULTRA        0
#define FUSION_VQF_ADVANCED     1
#define FUSION_VQF_OPT          2
#define FUSION_VQF_SIMPLE       3
#define FUSION_EKF              4

#ifndef FUSION_TYPE
#define FUSION_TYPE             FUSION_VQF_ADVANCED
#endif

/*============================================================================
 * RF 配置 / RF Configuration
 *============================================================================*/
#ifndef RF_TX_POWER_DBM
#define RF_TX_POWER_DBM     4
#endif

#define RF_CHANNEL_37       37
#define RF_CHANNEL_38       38
#define RF_CHANNEL_39       39
#define RF_DEFAULT_CHANNEL  RF_CHANNEL_38
#define RF_DATA_RATE_HZ     200
#define RF_RETRANSMIT_COUNT 2
#define RF_HOP_INTERVAL_MS  50

/*============================================================================
 * 引脚配置 / Pin Configuration
 * 
 * CH591D (20-pin QFN) 追踪器引脚分配
 * 
 * 重要：原理图存在引脚标签偏移错误！
 * Pin 3 (VIO33) 标签缺失，导致 Pin 6-11 标签全部偏移一位。
 * 以下定义基于芯片手册的正确引脚分配。
 *============================================================================*/

// LED 状态指示灯
#define PIN_LED             9       // PA9 (Pin 7)
#define PIN_LED_PORT        GPIOA

// 用户按键 SW0
#define PIN_SW0             20      // PB4 (Pin 11)
#define PIN_SW0_PORT        GPIOB
#define PIN_SW1             PIN_SW0

// IMU SPI 接口
#define PIN_SPI_CS          12      // PA12
#define PIN_SPI_CLK         13      // PA13
#define PIN_SPI_MOSI        14      // PA14
#define PIN_SPI_MISO        15      // PA15

// IMU 中断引脚
#define PIN_IMU_INT1        11      // PA11 (Pin 1)
#define PIN_IMU_INT1_PORT   GPIOA
#define PIN_IMU_INT2        6       // PB6 (可选)

// 充电检测
#define PIN_CHRG_DET        10      // PA10 (Pin 2)
#define PIN_CHRG_DET_PORT   GPIOA
#define PIN_USB_VBUS        PIN_CHRG_DET

// 电池检测
#define PIN_VBAT_ADC        8       // PA8 (Pin 6)
#define PIN_ADC_CHANNEL     12      // AIN12
#define PIN_VBAT_ADC_CHANNEL 12

// 电池检测分压电阻
#define VBAT_DIVIDER_R1_OHMS 100000.0f
#define VBAT_DIVIDER_R2_OHMS 100000.0f
#define ADC_REF_VOLTAGE      1.2f

// BOOT 按键
#define PIN_BOOT            20      // PB4 (Pin 11)

// RST 引脚
#define PIN_RST             23      // PB7 (Pin 10)

/*============================================================================
 * 电源管理配置 / Power Management Configuration
 *============================================================================*/
#define AUTO_SLEEP_TIMEOUT_MS   300000
#define LONG_PRESS_SLEEP_MS     3000
#define BATTERY_LOW_PERCENT     10
#define BATTERY_CRITICAL_PERCENT 5

/*============================================================================
 * WOM唤醒引脚配置
 *============================================================================*/
#define WOM_WAKE_PIN            4       // PA4
#define WOM_WAKE_PORT           GPIO_A
#define WOM_WAKE_EDGE           GPIO_ITMode_RiseEdge
#define WOM_WAKE_PULL           GPIO_ModeIN_PD
#define WOM_THRESHOLD_MG        200

#define WAKE_SOURCE_WOM         (1 << 0)
#define WAKE_SOURCE_BTN         (1 << 1)
#define WAKE_SOURCE_CHRG        (1 << 2)
#define WAKE_SOURCES_DEFAULT    (WAKE_SOURCE_WOM | WAKE_SOURCE_BTN)

/*============================================================================
 * 动态节流配置
 *============================================================================*/
#define TX_RATE_MOVING          200
#define TX_RATE_MICRO_MOTION    100
#define TX_RATE_STATIONARY      50
#define TX_RATE_DIVIDER_MOVING  1
#define TX_RATE_DIVIDER_MICRO   2
#define TX_RATE_DIVIDER_STATIC  4

/*============================================================================
 * 采样配置 / Sampling Configuration
 *============================================================================*/
#define SENSOR_ODR_HZ           200
#define FUSION_RATE_HZ          200
#define RF_REPORT_RATE_HZ       200

/*============================================================================
 * 追踪器配置 / Tracker Configuration
 *============================================================================*/
#define MAX_TRACKERS            10

/*============================================================================
 * USB 配置 / USB Configuration
 *============================================================================*/
#define USB_VID                 0x1209
#define USB_PID                 0x7690
#define USB_MANUFACTURER        "SlimeVR"
#define USB_PRODUCT             "SlimeVR CH591D Tracker"

/*============================================================================
 * 功能开关
 *============================================================================*/
#define USE_SENSOR_OPTIMIZED    1
#define USE_USB_DEBUG           1
#define USE_DIAGNOSTICS         1
#define USE_CHANNEL_MANAGER     1
#define USE_RF_RECOVERY         1
#define USE_RF_SLOT_OPTIMIZER   1
#define USE_RF_TIMING_OPT       1
#define USE_RF_ULTRA            1
#define USE_USB_MSC             1
#define USE_MAGNETOMETER        1

/*============================================================================
 * 功耗常量
 *============================================================================*/
#define POWER_MCU_ACTIVE_MA     8.0f
#define POWER_MCU_SLEEP_UA      2.0f
#define POWER_RF_TX_MA          12.5f
#define POWER_RF_AVG_MA         3.0f
#define POWER_IMU_ACTIVE_MA     0.65f
#define POWER_IMU_SLEEP_UA      3.0f
#define POWER_LED_MA            5.0f
#define POWER_OTHER_MA          1.0f

#define POWER_TOTAL_ACTIVE_MA   (POWER_MCU_ACTIVE_MA + POWER_RF_AVG_MA + \
                                  POWER_IMU_ACTIVE_MA + POWER_OTHER_MA)

/*============================================================================
 * 丢包优化配置
 *============================================================================*/
#define PACKET_LOSS_ENABLE_RETRANSMIT   1
#define PACKET_LOSS_ENABLE_FEC          0
#define PACKET_LOSS_ENABLE_INTERPOLATION 1
#define PACKET_LOSS_THRESHOLD_PERCENT   5

#endif /* __BOARD_CH591D_CONFIG_H__ */

