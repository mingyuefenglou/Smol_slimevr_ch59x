/**
 * @file CH59x_gpio.h
 * @brief CH59X GPIO Driver Header
 */

#ifndef __CH59X_GPIO_H__
#define __CH59X_GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*============================================================================
 * GPIO Pin Definitions
 *============================================================================*/

#define GPIO_Pin_0      (1 << 0)
#define GPIO_Pin_1      (1 << 1)
#define GPIO_Pin_2      (1 << 2)
#define GPIO_Pin_3      (1 << 3)
#define GPIO_Pin_4      (1 << 4)
#define GPIO_Pin_5      (1 << 5)
#define GPIO_Pin_6      (1 << 6)
#define GPIO_Pin_7      (1 << 7)
#define GPIO_Pin_8      (1 << 8)
#define GPIO_Pin_9      (1 << 9)
#define GPIO_Pin_10     (1 << 10)
#define GPIO_Pin_11     (1 << 11)
#define GPIO_Pin_12     (1 << 12)
#define GPIO_Pin_13     (1 << 13)
#define GPIO_Pin_14     (1 << 14)
#define GPIO_Pin_15     (1 << 15)
#define GPIO_Pin_16     (1 << 16)
#define GPIO_Pin_17     (1 << 17)
#define GPIO_Pin_18     (1 << 18)
#define GPIO_Pin_19     (1 << 19)
#define GPIO_Pin_20     (1 << 20)
#define GPIO_Pin_21     (1 << 21)
#define GPIO_Pin_22     (1 << 22)
#define GPIO_Pin_23     (1 << 23)
#define GPIO_Pin_All    0xFFFFFFFF

/*============================================================================
 * GPIO Mode Definitions
 *============================================================================*/

typedef enum {
    GPIO_ModeIN_Floating,       // Input floating
    GPIO_ModeIN_PU,             // Input with pull-up
    GPIO_ModeIN_PD,             // Input with pull-down
    GPIO_ModeOut_PP_5mA,        // Output push-pull 5mA
    GPIO_ModeOut_PP_20mA,       // Output push-pull 20mA
} GPIOModeTypeDef;

/*============================================================================
 * GPIO Interrupt Mode Definitions
 *============================================================================*/

typedef enum {
    GPIO_ITMode_LowLevel,       // Low level trigger
    GPIO_ITMode_HighLevel,      // High level trigger
    GPIO_ITMode_FallEdge,       // Falling edge trigger
    GPIO_ITMode_RiseEdge,       // Rising edge trigger
} GPIOITModeTpDef;

/*============================================================================
 * GPIOA Functions
 *============================================================================*/

/**
 * @brief Configure GPIOA pin mode
 * @param pin Pin mask (GPIO_Pin_x)
 * @param mode Pin mode
 */
void GPIOA_ModeCfg(uint32_t pin, GPIOModeTypeDef mode);

/**
 * @brief Set GPIOA pins high
 * @param pin Pin mask
 */
void GPIOA_SetBits(uint32_t pin);

/**
 * @brief Set GPIOA pins low
 * @param pin Pin mask
 */
void GPIOA_ResetBits(uint32_t pin);

/**
 * @brief Toggle GPIOA pins
 * @param pin Pin mask
 */
void GPIOA_InverseBits(uint32_t pin);

/**
 * @brief Read GPIOA pin input level
 * @param pin Pin mask
 * @return Pin input level (0 or non-zero)
 */
uint32_t GPIOA_ReadPortPin(uint32_t pin);

/**
 * @brief Read GPIOA port input
 * @return Port input value
 */
uint32_t GPIOA_ReadPort(void);

/**
 * @brief Configure GPIOA interrupt mode
 * @param pin Pin mask
 * @param mode Interrupt mode
 */
void GPIOA_ITModeCfg(uint32_t pin, GPIOITModeTpDef mode);

/**
 * @brief Clear GPIOA interrupt flag
 * @param pin Pin mask
 */
void GPIOA_ClearITFlagBit(uint32_t pin);

/**
 * @brief Read GPIOA interrupt flag
 * @return Interrupt flag bits
 */
uint16_t GPIOA_ReadITFlagPort(void);

/*============================================================================
 * GPIOB Functions
 *============================================================================*/

void GPIOB_ModeCfg(uint32_t pin, GPIOModeTypeDef mode);
void GPIOB_SetBits(uint32_t pin);
void GPIOB_ResetBits(uint32_t pin);
void GPIOB_InverseBits(uint32_t pin);
uint32_t GPIOB_ReadPortPin(uint32_t pin);
uint32_t GPIOB_ReadPort(void);
void GPIOB_ITModeCfg(uint32_t pin, GPIOITModeTpDef mode);
void GPIOB_ClearITFlagBit(uint32_t pin);
uint16_t GPIOB_ReadITFlagPort(void);

/*============================================================================
 * Alternate Function Configuration
 *============================================================================*/

/**
 * @brief Configure GPIO alternate function remapping
 * @param remap Remap configuration value
 */
void GPIOPinRemap(uint8_t remap);

/**
 * @brief Configure analog function
 * @param pin_ana Analog pin mask
 */
void GPIOAGPCfg(uint8_t pin_ana);

#ifdef __cplusplus
}
#endif

#endif /* __CH59X_GPIO_H__ */
