/**
 * @file CH59x_gpio.c
 * @brief CH59X GPIO Driver Implementation
 */

#include "CH59x_common.h"

/*============================================================================
 * Register Definitions
 *============================================================================*/

// GPIO Register structure
typedef struct {
    __IO uint32_t DIR;          // Direction register
    __IO uint32_t PIN;          // Pin input register
    __IO uint32_t OUT;          // Output register
    __IO uint32_t CLR;          // Output clear register
    __IO uint32_t PU;           // Pull-up enable
    __IO uint32_t PD;           // Pull-down enable
    __IO uint32_t DRV;          // Drive strength
    __IO uint32_t INT_EN;       // Interrupt enable
    __IO uint32_t INT_MODE;     // Interrupt mode
    __IO uint32_t INT_FLAG;     // Interrupt flag
} GPIO_TypeDef;

#define GPIOA   ((GPIO_TypeDef *)(GPIO_BASE + 0x0000))
#define GPIOB   ((GPIO_TypeDef *)(GPIO_BASE + 0x0100))

/*============================================================================
 * GPIOA Implementation
 *============================================================================*/

void GPIOA_ModeCfg(uint32_t pin, GPIOModeTypeDef mode)
{
    switch (mode) {
        case GPIO_ModeIN_Floating:
            GPIOA->DIR &= ~pin;
            GPIOA->PU &= ~pin;
            GPIOA->PD &= ~pin;
            break;
        case GPIO_ModeIN_PU:
            GPIOA->DIR &= ~pin;
            GPIOA->PU |= pin;
            GPIOA->PD &= ~pin;
            break;
        case GPIO_ModeIN_PD:
            GPIOA->DIR &= ~pin;
            GPIOA->PU &= ~pin;
            GPIOA->PD |= pin;
            break;
        case GPIO_ModeOut_PP_5mA:
            GPIOA->DIR |= pin;
            GPIOA->DRV &= ~pin;
            break;
        case GPIO_ModeOut_PP_20mA:
            GPIOA->DIR |= pin;
            GPIOA->DRV |= pin;
            break;
    }
}

void GPIOA_SetBits(uint32_t pin)
{
    GPIOA->OUT |= pin;
}

void GPIOA_ResetBits(uint32_t pin)
{
    GPIOA->CLR = pin;
}

void GPIOA_InverseBits(uint32_t pin)
{
    GPIOA->OUT ^= pin;
}

uint32_t GPIOA_ReadPortPin(uint32_t pin)
{
    return GPIOA->PIN & pin;
}

uint32_t GPIOA_ReadPort(void)
{
    return GPIOA->PIN;
}

void GPIOA_ITModeCfg(uint32_t pin, GPIOITModeTpDef mode)
{
    // Configure interrupt mode based on edge/level
    if (mode == GPIO_ITMode_FallEdge || mode == GPIO_ITMode_RiseEdge) {
        GPIOA->INT_MODE |= pin;  // Edge triggered
    } else {
        GPIOA->INT_MODE &= ~pin; // Level triggered
    }
    
    // Enable interrupt
    GPIOA->INT_EN |= pin;
}

void GPIOA_ClearITFlagBit(uint32_t pin)
{
    GPIOA->INT_FLAG = pin;  // Write 1 to clear
}

uint16_t GPIOA_ReadITFlagPort(void)
{
    return (uint16_t)GPIOA->INT_FLAG;
}

/*============================================================================
 * GPIOB Implementation
 *============================================================================*/

void GPIOB_ModeCfg(uint32_t pin, GPIOModeTypeDef mode)
{
    switch (mode) {
        case GPIO_ModeIN_Floating:
            GPIOB->DIR &= ~pin;
            GPIOB->PU &= ~pin;
            GPIOB->PD &= ~pin;
            break;
        case GPIO_ModeIN_PU:
            GPIOB->DIR &= ~pin;
            GPIOB->PU |= pin;
            GPIOB->PD &= ~pin;
            break;
        case GPIO_ModeIN_PD:
            GPIOB->DIR &= ~pin;
            GPIOB->PU &= ~pin;
            GPIOB->PD |= pin;
            break;
        case GPIO_ModeOut_PP_5mA:
            GPIOB->DIR |= pin;
            GPIOB->DRV &= ~pin;
            break;
        case GPIO_ModeOut_PP_20mA:
            GPIOB->DIR |= pin;
            GPIOB->DRV |= pin;
            break;
    }
}

void GPIOB_SetBits(uint32_t pin)
{
    GPIOB->OUT |= pin;
}

void GPIOB_ResetBits(uint32_t pin)
{
    GPIOB->CLR = pin;
}

void GPIOB_InverseBits(uint32_t pin)
{
    GPIOB->OUT ^= pin;
}

uint32_t GPIOB_ReadPortPin(uint32_t pin)
{
    return GPIOB->PIN & pin;
}

uint32_t GPIOB_ReadPort(void)
{
    return GPIOB->PIN;
}

void GPIOB_ITModeCfg(uint32_t pin, GPIOITModeTpDef mode)
{
    if (mode == GPIO_ITMode_FallEdge || mode == GPIO_ITMode_RiseEdge) {
        GPIOB->INT_MODE |= pin;
    } else {
        GPIOB->INT_MODE &= ~pin;
    }
    GPIOB->INT_EN |= pin;
}

void GPIOB_ClearITFlagBit(uint32_t pin)
{
    GPIOB->INT_FLAG = pin;
}

uint16_t GPIOB_ReadITFlagPort(void)
{
    return (uint16_t)GPIOB->INT_FLAG;
}

void GPIOPinRemap(uint8_t remap)
{
    // Implementation depends on specific remap configuration
    (void)remap;
}

void GPIOAGPCfg(uint8_t pin_ana)
{
    // Configure analog function
    (void)pin_ana;
}
