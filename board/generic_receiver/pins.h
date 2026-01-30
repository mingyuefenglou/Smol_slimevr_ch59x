/**
 * @file pins.h
 * @brief Generic Receiver Board Pin Definitions
 * 
 * Generic Receiver Board Pin Definitions
 * Pin mappings are not defined, need to be configured according to actual hardware
 */

#ifndef __BOARD_GENERIC_RECEIVER_PINS_H__
#define __BOARD_GENERIC_RECEIVER_PINS_H__

/*============================================================================
 * UART Channel Mapping
 * Needs to be configured according to actual hardware
 *============================================================================*/
// Example: If using PB7/PB4 as UART0
// #define UART0_TX                PB7
// #define UART0_RX                PB4

/*============================================================================
 * Pin Configuration Instructions
 *============================================================================*/
/*
 * When using this generic receiver configuration, you need to:
 * 
 * 1. Edit board/generic_receiver/config.h, configure all pin definitions
 * 2. Edit board/generic_receiver/pins.h, configure UART mappings if needed
 * 3. Ensure pin definitions match the actual hardware schematic
 * 4. Compile with: make TARGET=receiver BOARD=generic_receiver
 * 
 * Configuration Example:
 * 
 * // In config.h
 * #define PIN_LED             9       // PA9
 * #define PIN_LED_PORT        GPIOA
 * 
 * #define PIN_PAIR_BTN        4       // PB4
 * #define PIN_PAIR_BTN_PORT   GPIOB
 * 
 * // In pins.h
 * #define UART0_TX            PB7
 * #define UART0_RX            PB4
 */

#endif /* __BOARD_GENERIC_RECEIVER_PINS_H__ */

