/**
 * @file CH59x_adc.h
 * @brief CH59X ADC Driver Header
 */

#ifndef __CH59X_ADC_H__
#define __CH59X_ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
    ADC_CH0 = 0,
    ADC_CH1,
    ADC_CH2,
    ADC_CH3,
    ADC_CH4,
    ADC_CH5,
    ADC_CH6,
    ADC_CH7,
    ADC_CH8,
    ADC_CH9,
    ADC_CH10,
    ADC_CH11,
    ADC_CH_TEMP,
    ADC_CH_VBAT,
} ADC_ChannelTypeDef;

typedef enum {
    ADC_PGA_1_4,    // 1/4 gain
    ADC_PGA_1_2,    // 1/2 gain
    ADC_PGA_1,      // Unity gain
    ADC_PGA_2,      // 2x gain
} ADC_PGATypeDef;

void ADC_ExtSingleChInit(ADC_ChannelTypeDef ch, ADC_PGATypeDef pga);
void ADC_TempSensorInit(void);
void ADC_BatteryInit(void);
uint16_t ADC_ExcutSingleConver(void);
int16_t ADC_GetTemperature(void);
uint16_t ADC_GetBatteryVoltage(void);

#ifdef __cplusplus
}
#endif

#endif /* __CH59X_ADC_H__ */
