/**
 * @file CH59x_adc.c
 * @brief CH592 ADC Driver (Stub)
 */

#include "CH59x_common.h"

// ADC registers (approximate addresses)
#define R8_ADC_CFG          (*((volatile uint8_t*)(0x4000D000)))
#define R8_ADC_CONVERT      (*((volatile uint8_t*)(0x4000D001)))
#define R8_ADC_CHANNEL      (*((volatile uint8_t*)(0x4000D002)))
#define R16_ADC_DATA        (*((volatile uint16_t*)(0x4000D004)))

#define RB_ADC_POWER_ON     0x01
#define RB_ADC_CLK_DIV      0x0E
#define RB_ADC_START        0x01

__attribute__((weak))
void ADC_ExtSingleChSampInit(uint8_t clk_div, uint8_t channel)
{
    R8_ADC_CHANNEL = channel;
    R8_ADC_CFG = RB_ADC_POWER_ON | (clk_div << 1);
}

__attribute__((weak))
void ADC_ExtSingleChSampStart(void)
{
    R8_ADC_CONVERT = RB_ADC_START;
}

__attribute__((weak))
uint16_t ADC_ExtSingleChSampGet(void)
{
    while (R8_ADC_CONVERT & RB_ADC_START);
    return R16_ADC_DATA & 0x0FFF;
}

__attribute__((weak))
uint16_t ADC_GetVoltage(uint8_t channel)
{
    ADC_ExtSingleChSampInit(4, channel);
    ADC_ExtSingleChSampStart();
    uint16_t raw = ADC_ExtSingleChSampGet();
    // Convert to mV (assuming 3.3V reference, 12-bit ADC)
    return (uint16_t)(((uint32_t)raw * 3300) >> 12);
}

__attribute__((weak))
int16_t ADC_GetTemperature(void)
{
    // Read internal temperature sensor
    // Returns temperature in 0.1°C
    ADC_ExtSingleChSampInit(4, 14);  // Channel 14 = temp sensor
    ADC_ExtSingleChSampStart();
    uint16_t raw = ADC_ExtSingleChSampGet();
    // Approximate conversion (needs calibration)
    return (int16_t)(raw - 1000) / 3;
}

__attribute__((weak))
void ADC_BatteryInit(void)
{
    // Initialize ADC for battery voltage measurement
    // Use channel for battery voltage (typically VBAT pin)
    ADC_ExtSingleChSampInit(4, ADC_CH_VBAT);
}

__attribute__((weak))
uint16_t ADC_ExcutSingleConver(void)
{
    // Execute single conversion and return result
    ADC_ExtSingleChSampStart();
    return ADC_ExtSingleChSampGet();
}
