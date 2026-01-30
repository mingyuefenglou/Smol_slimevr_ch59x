/**
 * @file config.h
 * @brief CH592X 板子配置文件
 * 
 * CH592X (28-pin QFN) 板子配置
 * 所有可配置参数集中在此文件
 */

#ifndef __BOARD_CH592X_CONFIG_H__
#define __BOARD_CH592X_CONFIG_H__

/*============================================================================
 * 芯片选择 / Chip Selection
 *============================================================================*/
#define CHIP_TYPE           CH592

/*============================================================================
 * IMU 传感器选择 / IMU Sensor Selection
 *============================================================================*/
#ifndef IMU_TYPE
#define IMU_TYPE            IMU_ICM45686
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
 * CH592X (28-pin QFN) 引脚配置
 *============================================================================*/

#ifdef BUILD_TRACKER

// LED 状态指示灯
#define PIN_LED                 9       // PA9 (Pin 7)
#define PIN_LED_GPIO            GPIO_Pin_9
#define PIN_LED_PORT            GPIOA

// 用户按键 (BOOT)
#define PIN_SW0                 4       // PB4 (Pin 20)
#define PIN_SW0_GPIO            GPIO_Pin_4
#define PIN_SW0_PORT            GPIOB

// IMU SPI 接口
#define PIN_SPI_CS              12      // PA12 (底部焊盘)
#define PIN_SPI_CS_GPIO         GPIO_Pin_12
#define PIN_SPI_CS_PORT         GPIOA
#define PIN_SPI_CLK             13      // PA13 (Pin 15)
#define PIN_SPI_MOSI            14      // PA14 (Pin 16)
#define PIN_SPI_MISO            15      // PA15 (Pin 17)

// IMU 中断
#define PIN_IMU_INT1            11      // PA11 (Pin 1)
#define PIN_IMU_INT1_GPIO       GPIO_Pin_11
#define PIN_IMU_INT1_PORT       GPIOA
#define PIN_IMU_INT2            6       // PB6 (Pin 11)
#define PIN_IMU_INT2_GPIO       GPIO_Pin_6
#define PIN_IMU_INT2_PORT       GPIOB

// 充电检测
#define PIN_CHRG_DET            5       // PA5 (Pin 14)
#define PIN_CHRG_DET_GPIO       GPIO_Pin_5
#define PIN_CHRG_DET_PORT       GPIOA

// 电池 ADC
#define PIN_VBAT_ADC            7       // PA7 (Pin 12)
#define PIN_VBAT_ADC_GPIO       GPIO_Pin_7
#define PIN_VBAT_ADC_PORT       GPIOA
#define PIN_VBAT_ADC_CHANNEL    11      // AIN11

// USB (硬件固定)
#define PIN_USB_DP              11      // PB11 (Pin 8)
#define PIN_USB_DM              10      // PB10 (Pin 9)

#endif /* BUILD_TRACKER */

#ifdef BUILD_RECEIVER

// LED 状态指示灯
#define PIN_LED                 9       // PA9 (Pin 7)
#define PIN_LED_GPIO            GPIO_Pin_9
#define PIN_LED_PORT            GPIOA

// 用户按键 (配对按钮)
#define PIN_PAIR_BTN            4       // PB4 (Pin 20)
#define PIN_PAIR_BTN_GPIO       GPIO_Pin_4
#define PIN_PAIR_BTN_PORT       GPIOB

// USB (硬件固定)
#define PIN_USB_DP              11      // PB11 (Pin 8)
#define PIN_USB_DM              10      // PB10 (Pin 9)

// 调试串口
#define PIN_DEBUG_TX            7       // PB7 (Pin 10)
#define PIN_DEBUG_TX_GPIO       GPIO_Pin_7
#define PIN_DEBUG_TX_PORT       GPIOB
#define PIN_DEBUG_RX            4       // PB4 (Pin 20)
#define PIN_DEBUG_RX_GPIO       GPIO_Pin_4
#define PIN_DEBUG_RX_PORT       GPIOB

// RF 天线 (硬件固定)
#define PIN_RF_ANT              25      // ANT (Pin 25)

#endif /* BUILD_RECEIVER */

// I2C 引脚配置 (备用 IMU 接口)
#define PIN_I2C_SCL             22      // PB22 (Pin 18)
#define PIN_I2C_SCL_GPIO        GPIO_Pin_22
#define PIN_I2C_SCL_PORT        GPIOB
#define PIN_I2C_SDA             23      // PB23 (Pin 19)
#define PIN_I2C_SDA_GPIO        GPIO_Pin_23
#define PIN_I2C_SDA_PORT        GPIOB

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
#define USB_PRODUCT             "SlimeVR CH592X Receiver"

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

#endif /* __BOARD_CH592X_CONFIG_H__ */

