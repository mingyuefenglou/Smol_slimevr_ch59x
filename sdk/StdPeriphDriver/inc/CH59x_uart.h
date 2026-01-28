/**
 * @file CH59x_uart.h
 * @brief CH59X UART Driver Header
 */

#ifndef __CH59X_UART_H__
#define __CH59X_UART_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void UART0_DefInit(void);
void UART0_BaudRateCfg(uint32_t baudrate);
void UART0_SendByte(uint8_t data);
uint8_t UART0_RecvByte(void);
void UART0_SendString(const char *str);
uint8_t UART0_GetITFlag(uint8_t flag);
void UART0_ClearITFlag(uint8_t flag);

void UART1_DefInit(void);
void UART1_BaudRateCfg(uint32_t baudrate);
void UART1_SendByte(uint8_t data);
uint8_t UART1_RecvByte(void);
void UART1_SendString(const char *str);
uint8_t UART1_GetITFlag(uint8_t flag);
void UART1_ClearITFlag(uint8_t flag);

void UART2_DefInit(void);
void UART3_DefInit(void);

#ifdef __cplusplus
}
#endif

#endif /* __CH59X_UART_H__ */
