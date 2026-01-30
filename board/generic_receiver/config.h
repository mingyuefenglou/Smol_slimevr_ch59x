/**
 * @file config.h
 * @brief Generic Receiver Board Configuration File
 * 
 * Generic Receiver Board Configuration
 * Pins are not defined, need to be configured according to actual hardware
 */

#ifndef __BOARD_GENERIC_RECEIVER_CONFIG_H__
#define __BOARD_GENERIC_RECEIVER_CONFIG_H__

/*============================================================================
 * Chip Selection
 *============================================================================*/
#ifndef CHIP_TYPE
#define CHIP_TYPE           CH592       // Default CH592, can be changed to CH591
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

// Pairing Button - needs to be configured according to actual hardware
#ifndef PIN_PAIR_BTN
#define PIN_PAIR_BTN        -1          // Undefined, needs configuration
#define PIN_PAIR_BTN_PORT   GPIOB       // Modify according to actual hardware
#endif

// USB Pins (hardware fixed, usually no need to modify)
#ifndef PIN_USB_DP
#define PIN_USB_DP          11          // PB11 (usually fixed)
#define PIN_USB_DM          10          // PB10 (usually fixed)
#endif

// Debug UART (optional) - needs to be configured according to actual hardware
#ifndef PIN_DEBUG_TX
#define PIN_DEBUG_TX        -1          // Undefined, needs configuration
#define PIN_DEBUG_TX_PORT   GPIOB       // Modify according to actual hardware
#endif
#ifndef PIN_DEBUG_RX
#define PIN_DEBUG_RX        -1          // Undefined, needs configuration
#define PIN_DEBUG_RX_PORT   GPIOB       // Modify according to actual hardware
#endif

// RF Antenna (hardware fixed, usually no need to modify)
#ifndef PIN_RF_ANT
#define PIN_RF_ANT          -1          // Undefined, needs configuration (usually hardware fixed)
#endif

/*============================================================================
 * Power Management Configuration
 *============================================================================*/
#define AUTO_SLEEP_TIMEOUT_MS   300000
#define LONG_PRESS_SLEEP_MS     3000

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
#define USB_PRODUCT             "SlimeVR Generic Receiver"

/*============================================================================
 * Feature Switches
 *============================================================================*/
#define USE_USB_DEBUG           1
#define USE_DIAGNOSTICS         1
#define USE_CHANNEL_MANAGER     1
#define USE_RF_RECOVERY         1
#define USE_RF_SLOT_OPTIMIZER   1
#define USE_RF_TIMING_OPT       1
#define USE_RF_ULTRA            1
#define USE_USB_MSC             1

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
#warning "PIN_LED is not defined! Please configure it in board/generic_receiver/config.h"
#endif

#if PIN_PAIR_BTN == -1
#warning "PIN_PAIR_BTN is not defined! Please configure it in board/generic_receiver/config.h"
#endif

#endif /* __BOARD_GENERIC_RECEIVER_CONFIG_H__ */

