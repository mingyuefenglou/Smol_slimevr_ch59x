/**
 * @file hal_gpio.c
 * @brief GPIO Hardware Abstraction Layer for CH59X
 * 
 * Provides unified GPIO interface for LED, buttons, and sensor interrupts.
 */

#include "hal.h"

#ifdef CH59X
#include "CH59x_common.h"
#include "CH59x_gpio.h"
#endif

/*============================================================================
 * Pin Mapping
 *============================================================================*/

// CH59X has GPIOA (PA0-PA15) and GPIOB (PB0-PB23)
// We map pin numbers as: 0-15 = PA0-PA15, 16-39 = PB0-PB23

#define IS_PORT_A(pin)      ((pin) < 16)
#define GET_PIN_MASK(pin)   (1UL << ((pin) & 0x0F))
#define GET_PB_PIN(pin)     ((pin) - 16)

/*============================================================================
 * Interrupt Callbacks
 *============================================================================*/

#define MAX_GPIO_CALLBACKS  8

typedef struct {
    uint8_t pin;
    void (*callback)(void);
} gpio_callback_t;

static gpio_callback_t gpio_callbacks[MAX_GPIO_CALLBACKS] = {0};
static uint8_t callback_count = 0;

/*============================================================================
 * Public API
 *============================================================================*/

int hal_gpio_config(uint8_t pin, hal_gpio_mode_t mode)
{
#ifdef CH59X
    uint32_t pin_mask;
    GPIOModeTypeDef ch59x_mode;
    
    // Map our mode to CH59X mode
    switch (mode) {
        case HAL_GPIO_INPUT:
            ch59x_mode = GPIO_ModeIN_Floating;
            break;
        case HAL_GPIO_OUTPUT:
            ch59x_mode = GPIO_ModeOut_PP_5mA;
            break;
        case HAL_GPIO_INPUT_PULLUP:
            ch59x_mode = GPIO_ModeIN_PU;
            break;
        case HAL_GPIO_INPUT_PULLDOWN:
            ch59x_mode = GPIO_ModeIN_PD;
            break;
        default:
            return -1;
    }
    
    if (IS_PORT_A(pin)) {
        pin_mask = GET_PIN_MASK(pin);
        GPIOA_ModeCfg(pin_mask, ch59x_mode);
    } else {
        pin_mask = 1UL << GET_PB_PIN(pin);
        GPIOB_ModeCfg(pin_mask, ch59x_mode);
    }
    
    return 0;
#else
    (void)pin; (void)mode;
    return 0;
#endif
}

void hal_gpio_write(uint8_t pin, bool level)
{
#ifdef CH59X
    uint32_t pin_mask;
    
    if (IS_PORT_A(pin)) {
        pin_mask = GET_PIN_MASK(pin);
        if (level)
            GPIOA_SetBits(pin_mask);
        else
            GPIOA_ResetBits(pin_mask);
    } else {
        pin_mask = 1UL << GET_PB_PIN(pin);
        if (level)
            GPIOB_SetBits(pin_mask);
        else
            GPIOB_ResetBits(pin_mask);
    }
#else
    (void)pin; (void)level;
#endif
}

bool hal_gpio_read(uint8_t pin)
{
#ifdef CH59X
    uint32_t pin_mask;
    
    if (IS_PORT_A(pin)) {
        pin_mask = GET_PIN_MASK(pin);
        return (GPIOA_ReadPortPin(pin_mask) != 0);
    } else {
        pin_mask = 1UL << GET_PB_PIN(pin);
        return (GPIOB_ReadPortPin(pin_mask) != 0);
    }
#else
    (void)pin;
    return false;
#endif
}

void hal_gpio_toggle(uint8_t pin)
{
#ifdef CH59X
    uint32_t pin_mask;
    
    if (IS_PORT_A(pin)) {
        pin_mask = GET_PIN_MASK(pin);
        GPIOA_InverseBits(pin_mask);
    } else {
        pin_mask = 1UL << GET_PB_PIN(pin);
        GPIOB_InverseBits(pin_mask);
    }
#else
    (void)pin;
#endif
}

int hal_gpio_set_interrupt(uint8_t pin, hal_gpio_int_t type, void (*callback)(void))
{
#ifdef CH59X
    if (callback_count >= MAX_GPIO_CALLBACKS) {
        return -1;
    }
    
    // Store callback
    gpio_callbacks[callback_count].pin = pin;
    gpio_callbacks[callback_count].callback = callback;
    callback_count++;
    
    uint32_t pin_mask;
    GPIOITModeTpDef it_mode;
    
    // Map interrupt type
    switch (type) {
        case HAL_GPIO_INT_RISING:
            it_mode = GPIO_ITMode_RiseEdge;
            break;
        case HAL_GPIO_INT_FALLING:
            it_mode = GPIO_ITMode_FallEdge;
            break;
        case HAL_GPIO_INT_BOTH:
            // CH59X doesn't have both edges mode, use rising
            it_mode = GPIO_ITMode_RiseEdge;
            break;
        default:
            return -1;
    }
    
    if (IS_PORT_A(pin)) {
        pin_mask = GET_PIN_MASK(pin);
        GPIOA_ModeCfg(pin_mask, GPIO_ModeIN_PU);
        GPIOA_ITModeCfg(pin_mask, it_mode);
        PFIC_EnableIRQ(GPIO_A_IRQn);
    } else {
        pin_mask = 1UL << GET_PB_PIN(pin);
        GPIOB_ModeCfg(pin_mask, GPIO_ModeIN_PU);
        GPIOB_ITModeCfg(pin_mask, it_mode);
        PFIC_EnableIRQ(GPIO_B_IRQn);
    }
    
    return 0;
#else
    (void)pin; (void)type; (void)callback;
    return 0;
#endif
}

/*============================================================================
 * Interrupt Handlers
 *============================================================================*/

#ifdef CH59X

__INTERRUPT
__HIGH_CODE
__attribute__((weak))
void GPIOA_IRQHandler(void)
{
    uint16_t flag = GPIOA_ReadITFlagPort();
    GPIOA_ClearITFlagBit(flag);
    
    for (uint8_t i = 0; i < callback_count; i++) {
        if (IS_PORT_A(gpio_callbacks[i].pin)) {
            uint16_t pin_mask = GET_PIN_MASK(gpio_callbacks[i].pin);
            if (flag & pin_mask) {
                if (gpio_callbacks[i].callback) {
                    gpio_callbacks[i].callback();
                }
            }
        }
    }
}

__INTERRUPT
__HIGH_CODE
__attribute__((weak))
void GPIOB_IRQHandler(void)
{
    uint16_t flag = GPIOB_ReadITFlagPort();
    GPIOB_ClearITFlagBit(flag);
    
    for (uint8_t i = 0; i < callback_count; i++) {
        if (!IS_PORT_A(gpio_callbacks[i].pin)) {
            uint32_t pin_mask = 1UL << GET_PB_PIN(gpio_callbacks[i].pin);
            if (flag & pin_mask) {
                if (gpio_callbacks[i].callback) {
                    gpio_callbacks[i].callback();
                }
            }
        }
    }
}

#endif
