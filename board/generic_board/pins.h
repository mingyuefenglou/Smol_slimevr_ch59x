/**
 * @file pins.h
 * @brief Generic Board Pin Definitions
 * 
 * Generic Board Pin Definitions
 * Pin mappings are not defined, need to be configured according to actual hardware
 */

#ifndef __BOARD_GENERIC_BOARD_PINS_H__
#define __BOARD_GENERIC_BOARD_PINS_H__

/*============================================================================
 * ADC Channel Mapping
 * Needs to be configured according to actual hardware
 *============================================================================*/
// Example: If using PA5 as ADC input
// #define ADC_CH_PA5_AIN1         1

/*============================================================================
 * PWM Channel Mapping
 * Needs to be configured according to actual hardware
 *============================================================================*/
// Example: If using PA6 as PWM4
// #define PWM_CH_PA6_PWM4         4

/*============================================================================
 * UART Channel Mapping
 * Needs to be configured according to actual hardware
 *============================================================================*/
// Example: If using PB7/PB4 as UART0
// #define UART0_TX                PB7
// #define UART0_RX                PB4

/*============================================================================
 * Timer Mapping
 * Needs to be configured according to actual hardware
 *============================================================================*/
// Example: If using PA9 as TMR0
// #define TMR0_PIN                PA9

/*============================================================================
 * Pin Configuration Instructions
 *============================================================================*/
/*
 * When using this generic configuration, you need to:
 * 
 * 1. Edit board/generic_board/config.h, configure all pin definitions
 * 2. Edit board/generic_board/pins.h, configure ADC/PWM/UART mappings
 * 3. Ensure pin definitions match the actual hardware schematic
 * 4. Compile with: make TARGET=tracker BOARD=generic_board
 * 
 * Configuration Example:
 * 
 * // In config.h
 * #define PIN_LED             9       // PA9
 * #define PIN_LED_PORT        GPIOA
 * 
 * #define PIN_SPI_CS          12      // PA12
 * #define PIN_SPI_CS_PORT     GPIOA
 * 
 * // In pins.h
 * #define ADC_CH_PA5_AIN1     1
 */

#endif /* __BOARD_GENERIC_BOARD_PINS_H__ */
