/**
 * @file CH59x_i2c.c
 * @brief CH59X I2C Driver Implementation
 */

#include "CH59x_common.h"

typedef struct {
    __IO uint32_t CTRL1;
    __IO uint32_t CTRL2;
    __IO uint32_t STAR1;
    __IO uint32_t STAR2;
    __IO uint32_t DATAR;
    __IO uint32_t CKCFGR;
} I2C_TypeDef;

#define I2C     ((I2C_TypeDef *)I2C_BASE)

void I2C_Init(void)
{
    I2C->CTRL1 = 0x0001;
    I2C->CKCFGR = 150;
}

void I2C_SetClock(uint32_t clock_hz)
{
    uint32_t sysclk = GetSysClock();
    I2C->CKCFGR = sysclk / (2 * clock_hz);
}

void I2C_Start(void)
{
    I2C->CTRL1 |= 0x0100;
    while (!(I2C->STAR1 & 0x01));
}

void I2C_Stop(void)
{
    I2C->CTRL1 |= 0x0200;
}

void I2C_SendByte(uint8_t data)
{
    I2C->DATAR = data;
    while (!(I2C->STAR1 & 0x80));
}

uint8_t I2C_RecvByte(uint8_t ack)
{
    if (ack) I2C->CTRL1 |= 0x0400;
    else I2C->CTRL1 &= ~0x0400;
    while (!(I2C->STAR1 & 0x40));
    return (uint8_t)I2C->DATAR;
}

uint8_t I2C_WaitAck(void)
{
    uint32_t timeout = 10000;
    while (timeout-- && !(I2C->STAR1 & 0x02));
    return (I2C->STAR1 & 0x10) ? 1 : 0;
}

void I2C_Ack(void) { I2C->CTRL1 |= 0x0400; }
void I2C_NAck(void) { I2C->CTRL1 &= ~0x0400; }
uint8_t I2C_GetStatus(void) { return (uint8_t)I2C->STAR1; }
uint8_t I2C_IsBusy(void) { return (I2C->STAR2 & 0x02) ? 1 : 0; }

uint8_t I2C_MasterWrite(uint8_t addr, const uint8_t *data, uint16_t len)
{
    I2C_Start();
    I2C_SendByte(addr << 1);
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }
    for (uint16_t i = 0; i < len; i++) {
        I2C_SendByte(data[i]);
        if (I2C_WaitAck()) { I2C_Stop(); return 1; }
    }
    I2C_Stop();
    return 0;
}

uint8_t I2C_MasterRead(uint8_t addr, uint8_t *data, uint16_t len)
{
    I2C_Start();
    I2C_SendByte((addr << 1) | 1);
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }
    for (uint16_t i = 0; i < len; i++) {
        data[i] = I2C_RecvByte(i < len - 1);
    }
    I2C_Stop();
    return 0;
}

uint8_t I2C_MasterWriteRead(uint8_t addr, const uint8_t *wdata, uint16_t wlen, uint8_t *rdata, uint16_t rlen)
{
    I2C_Start();
    I2C_SendByte(addr << 1);
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }
    for (uint16_t i = 0; i < wlen; i++) {
        I2C_SendByte(wdata[i]);
        if (I2C_WaitAck()) { I2C_Stop(); return 1; }
    }
    I2C_Start();
    I2C_SendByte((addr << 1) | 1);
    if (I2C_WaitAck()) { I2C_Stop(); return 1; }
    for (uint16_t i = 0; i < rlen; i++) {
        rdata[i] = I2C_RecvByte(i < rlen - 1);
    }
    I2C_Stop();
    return 0;
}
