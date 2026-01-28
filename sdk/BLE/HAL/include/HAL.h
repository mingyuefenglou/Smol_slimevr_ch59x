/**
 * @file HAL.h
 * @brief CH59X BLE HAL Interface
 */

#ifndef __HAL_H__
#define __HAL_H__

#include "CONFIG.h"

#ifdef __cplusplus
extern "C" {
#endif

// HAL initialization
void HAL_Init(void);
void HAL_ProcessEvent(void);
uint16_t HAL_GetBatteryVoltage(void);

// TMOS (Task Management OS)
void TMOS_SystemProcess(void);
uint8_t TMOS_AddTask(void (*task)(void), uint32_t interval);
void TMOS_RemoveTask(uint8_t taskId);
void TMOS_SetTimer(uint8_t taskId, uint32_t timeout);

// BLE Task events
#define HAL_KEY_EVENT           0x0001
#define HAL_LED_EVENT           0x0002
#define HAL_REG_INIT_EVENT      0x2000

#ifdef __cplusplus
}
#endif

#endif /* __HAL_H__ */
