/**
 * @file config.h
 * @brief Generic Board Configuration File
 * 
 * Generic Board Configuration
 * Pins are not defined, need to be configured according to actual hardware
 */

#ifndef __BOARD_GENERIC_BOARD_CONFIG_H__
#define __BOARD_GENERIC_BOARD_CONFIG_H__

/*============================================================================
 * Chip Selection
 *============================================================================*/
#ifndef CHIP_TYPE
#define CHIP_TYPE           CH592       // Default CH592, can be changed to CH591
#endif

/*============================================================================
 * IMU Sensor Selection
 *============================================================================*/
#ifndef IMU_TYPE
#define IMU_TYPE            IMU_ICM45686    // Default: ICM-45686
#endif

// IMU Type Definitions
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
 * Sensor Fusion Algorithm Selection
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
 * RF Configuration
 *============================================================================*/
#ifndef RF_TX_POWER_DBM
#define RF_TX_POWER_DBM     4           // +4dBm (maximum power)
#endif

#define RF_CHANNEL_37       37
#define RF_CHANNEL_38       38
#define RF_CHANNEL_39       39
#define RF_DEFAULT_CHANNEL  RF_CHANNEL_38
#define RF_DATA_RATE_HZ     200
#define RF_RETRANSMIT_COUNT 2
#define RF_HOP_INTERVAL_MS  50

/*============================================================================
 * Pin Configuration
 * 
 * Note: The following pin definitions need to be configured according to actual hardware
 *============================================================================*/

// LED Status Indicator - needs to be configured according to actual hardware
#ifndef PIN_LED
#define PIN_LED             -1          // Undefined, needs configuration
#define PIN_LED_PORT        GPIOA       // Modify according to actual hardware
#endif

// User Button - needs to be configured according to actual hardware
#ifndef PIN_SW0
#define PIN_SW0             -1          // Undefined, needs configuration
#define PIN_SW0_PORT        GPIOB       // Modify according to actual hardware
#endif
#define PIN_SW1             PIN_SW0

// IMU SPI Interface - needs to be configured according to actual hardware
#ifndef PIN_SPI_CS
#define PIN_SPI_CS          -1          // Undefined, needs configuration
#define PIN_SPI_CS_PORT     GPIOA       // Modify according to actual hardware
#endif
#ifndef PIN_SPI_CLK
#define PIN_SPI_CLK         -1          // Undefined, needs configuration
#endif
#ifndef PIN_SPI_MOSI
#define PIN_SPI_MOSI        -1          // Undefined, needs configuration
#endif
#ifndef PIN_SPI_MISO
#define PIN_SPI_MISO        -1          // Undefined, needs configuration
#endif

// IMU Interrupt Pins - needs to be configured according to actual hardware
#ifndef PIN_IMU_INT1
#define PIN_IMU_INT1        -1          // Undefined, needs configuration
#define PIN_IMU_INT1_PORT   GPIOA       // Modify according to actual hardware
#endif
#ifndef PIN_IMU_INT2
#define PIN_IMU_INT2        -1          // Undefined, needs configuration (optional)
#define PIN_IMU_INT2_PORT   GPIOB       // Modify according to actual hardware
#endif

// Charging Detection - needs to be configured according to actual hardware
#ifndef PIN_CHRG_DET
#define PIN_CHRG_DET        -1          // Undefined, needs configuration
#define PIN_CHRG_DET_PORT   GPIOA       // Modify according to actual hardware
#endif
#define PIN_USB_VBUS        PIN_CHRG_DET

// Battery Detection - needs to be configured according to actual hardware
#ifndef PIN_VBAT_ADC
#define PIN_VBAT_ADC        -1          // Undefined, needs configuration
#define PIN_VBAT_ADC_PORT   GPIOA       // Modify according to actual hardware
#define PIN_VBAT_ADC_CHANNEL -1         // Undefined, needs configuration
#endif

// Battery Detection Voltage Divider Resistors
#define VBAT_DIVIDER_R1_OHMS 100000.0f
#define VBAT_DIVIDER_R2_OHMS 100000.0f
#define ADC_REF_VOLTAGE      1.2f

// BOOT Button - needs to be configured according to actual hardware
#ifndef PIN_BOOT
#define PIN_BOOT            -1          // Undefined, needs configuration
#endif

// RST Pin - needs to be configured according to actual hardware
#ifndef PIN_RST
#define PIN_RST             -1          // Undefined, needs configuration
#endif

// I2C Pin Configuration (if using I2C for IMU) - needs to be configured
#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL         -1          // Undefined, needs configuration
#define PIN_I2C_SCL_PORT    GPIOB       // Modify according to actual hardware
#endif
#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA         -1          // Undefined, needs configuration
#define PIN_I2C_SDA_PORT    GPIOB       // Modify according to actual hardware
#endif

// USB Pins (hardware fixed, usually no need to modify)
#ifndef PIN_USB_DP
#define PIN_USB_DP          11          // PB11 (usually fixed)
#define PIN_USB_DM          10          // PB10 (usually fixed)
#endif

/*============================================================================
 * Power Management Configuration
 *============================================================================*/
#define AUTO_SLEEP_TIMEOUT_MS   300000
#define LONG_PRESS_SLEEP_MS     3000
#define BATTERY_LOW_PERCENT     10
#define BATTERY_CRITICAL_PERCENT 5

/*============================================================================
 * WOM Wake-up Pin Configuration
 *============================================================================*/
#ifndef WOM_WAKE_PIN
#define WOM_WAKE_PIN            -1       // Undefined, needs configuration
#define WOM_WAKE_PORT           GPIO_A  // Modify according to actual hardware
#endif
#define WOM_WAKE_EDGE           GPIO_ITMode_RiseEdge
#define WOM_WAKE_PULL           GPIO_ModeIN_PD
#define WOM_THRESHOLD_MG        200

#define WAKE_SOURCE_WOM         (1 << 0)
#define WAKE_SOURCE_BTN         (1 << 1)
#define WAKE_SOURCE_CHRG        (1 << 2)
#define WAKE_SOURCES_DEFAULT    (WAKE_SOURCE_WOM | WAKE_SOURCE_BTN)

/*============================================================================
 * Dynamic Throttling Configuration
 *============================================================================*/
#define TX_RATE_MOVING          200
#define TX_RATE_MICRO_MOTION    100
#define TX_RATE_STATIONARY      50
#define TX_RATE_DIVIDER_MOVING  1
#define TX_RATE_DIVIDER_MICRO   2
#define TX_RATE_DIVIDER_STATIC  4

/*============================================================================
 * Sampling Configuration
 *============================================================================*/
#define SENSOR_ODR_HZ           200
#define FUSION_RATE_HZ          200
#define RF_REPORT_RATE_HZ       200

/*============================================================================
 * Tracker Configuration
 *============================================================================*/
#define MAX_TRACKERS            10

/*============================================================================
 * USB Configuration
 *============================================================================*/
#define USB_VID                 0x1209
#define USB_PID                 0x7690
#define USB_MANUFACTURER        "SlimeVR"
#define USB_PRODUCT             "SlimeVR Generic Board"

/*============================================================================
 * Feature Switches
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
 * Power Consumption Constants
 *============================================================================*/
#define POWER_MCU_ACTIVE_MA     8.0f
#define POWER_MCU_SLEEP_UA     2.0f
#define POWER_RF_TX_MA          12.5f
#define POWER_RF_AVG_MA         3.0f
#define POWER_IMU_ACTIVE_MA     0.65f
#define POWER_IMU_SLEEP_UA      3.0f
#define POWER_LED_MA            5.0f
#define POWER_OTHER_MA          1.0f

#define POWER_TOTAL_ACTIVE_MA   (POWER_MCU_ACTIVE_MA + POWER_RF_AVG_MA + \
                                  POWER_IMU_ACTIVE_MA + POWER_OTHER_MA)

/*============================================================================
 * Packet Loss Optimization Configuration
 *============================================================================*/
#define PACKET_LOSS_ENABLE_RETRANSMIT   1
#define PACKET_LOSS_ENABLE_FEC          0
#define PACKET_LOSS_ENABLE_INTERPOLATION 1
#define PACKET_LOSS_THRESHOLD_PERCENT   5

/*============================================================================
 * Compile-time Checks
 *============================================================================*/
#if PIN_LED == -1
#warning "PIN_LED is not defined! Please configure it in board/generic_board/config.h"
#endif

#if PIN_SW0 == -1
#warning "PIN_SW0 is not defined! Please configure it in board/generic_board/config.h"
#endif

#if PIN_SPI_CS == -1
#warning "PIN_SPI_CS is not defined! Please configure it in board/generic_board/config.h"
#endif

#if PIN_IMU_INT1 == -1
#warning "PIN_IMU_INT1 is not defined! Please configure it in board/generic_board/config.h"
#endif

#endif /* __BOARD_GENERIC_BOARD_CONFIG_H__ */
