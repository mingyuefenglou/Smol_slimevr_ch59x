/**
 * @file CH59x_spi.c
 * @brief CH59X SPI Driver Implementation
 */

#include "CH59x_common.h"

typedef struct {
    __IO uint32_t CTRL;
    __IO uint32_t DATA;
    __IO uint32_t CLK_DIV;
    __IO uint32_t INT_EN;
    __IO uint32_t INT_FLAG;
    __IO uint32_t FIFO_CNT;
} SPI_TypeDef;

#define SPI0    ((SPI_TypeDef *)SPI0_BASE)
#define SPI1    ((SPI_TypeDef *)SPI1_BASE)

void SPI0_MasterDefInit(void)
{
    SPI0->CTRL = 0x60;  // Master mode, 8-bit
    SPI0->CLK_DIV = 6;  // Default ~10MHz at 60MHz sys clock
}

void SPI0_CLKCfg(uint8_t div)
{
    SPI0->CLK_DIV = div;
}

void SPI0_DataMode(SPI_ModeTypeDef mode)
{
    uint32_t ctrl = SPI0->CTRL & ~0x03;
    ctrl |= (mode >> 1) & 0x03;  // CPOL/CPHA bits
    SPI0->CTRL = ctrl;
}

uint8_t SPI0_MasterSendByte(uint8_t data)
{
    SPI0->DATA = data;
    while (!(SPI0->INT_FLAG & 0x01));  // Wait for transfer complete
    return (uint8_t)SPI0->DATA;
}

void SPI0_MasterSendData(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        SPI0_MasterSendByte(data[i]);
    }
}

void SPI0_MasterRecvData(uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        data[i] = SPI0_MasterSendByte(0xFF);
    }
}

void SPI0_Enable(uint8_t state)
{
    if (state) SPI0->CTRL |= 0x01;
    else SPI0->CTRL &= ~0x01;
}

void SPI1_MasterDefInit(void) { SPI1->CTRL = 0x60; SPI1->CLK_DIV = 6; }
void SPI1_CLKCfg(uint8_t div) { SPI1->CLK_DIV = div; }
void SPI1_DataMode(SPI_ModeTypeDef mode) { uint32_t ctrl = SPI1->CTRL & ~0x03; ctrl |= (mode >> 1) & 0x03; SPI1->CTRL = ctrl; }
uint8_t SPI1_MasterSendByte(uint8_t data) { SPI1->DATA = data; while (!(SPI1->INT_FLAG & 0x01)); return (uint8_t)SPI1->DATA; }
void SPI1_MasterSendData(const uint8_t *data, uint16_t len) { for (uint16_t i = 0; i < len; i++) SPI1_MasterSendByte(data[i]); }
void SPI1_MasterRecvData(uint8_t *data, uint16_t len) { for (uint16_t i = 0; i < len; i++) data[i] = SPI1_MasterSendByte(0xFF); }
void SPI1_Enable(uint8_t state) { if (state) SPI1->CTRL |= 0x01; else SPI1->CTRL &= ~0x01; }
void SPI0_SlaveInit(void) { SPI0->CTRL = 0x00; }
void SPI1_SlaveInit(void) { SPI1->CTRL = 0x00; }
