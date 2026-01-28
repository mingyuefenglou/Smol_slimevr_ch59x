/**
 * @file hal_spi.c
 * @brief SPI Hardware Abstraction Layer for CH59X
 * 
 * Provides SPI master interface for sensor communication.
 * Uses SPI0 peripheral with configurable chip select.
 */

#include "hal.h"

#ifdef CH59X
#include "CH59x_common.h"
#include "CH59x_spi.h"
#include "CH59x_gpio.h"
#endif

/*============================================================================
 * Pin Definitions (SPI0)
 *============================================================================*/

// CH59X SPI0 pins (fixed):
// PA12 - SPI0_SCK
// PA13 - SPI0_MOSI
// PA14 - SPI0_MISO
// CS is software controlled via GPIO

#define SPI_SCK_PIN     12  // PA12
#define SPI_MOSI_PIN    13  // PA13
#define SPI_MISO_PIN    14  // PA14

/*============================================================================
 * Static Variables
 *============================================================================*/

static uint8_t current_cs_pin = 0;
static bool spi_initialized = false;

/*============================================================================
 * Public API
 *============================================================================*/

int hal_spi_init(const hal_spi_config_t *config)
{
#ifdef CH59X
    if (!config) {
        return -1;
    }
    
    // Store CS pin
    current_cs_pin = config->cs_pin;
    
    // Configure CS pin as output, default high
    hal_gpio_config(current_cs_pin, HAL_GPIO_OUTPUT);
    hal_gpio_write(current_cs_pin, true);
    
    // Configure SPI pins
    GPIOA_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13, GPIO_ModeOut_PP_5mA);  // SCK, MOSI
    GPIOA_ModeCfg(GPIO_Pin_14, GPIO_ModeIN_PU);  // MISO
    
    // Calculate clock divider
    // SPI clock = SYS_CLK / (div * 2), where div = 1-255
    // For 60MHz system clock:
    //   div=1  -> 30MHz
    //   div=3  -> 10MHz
    //   div=6  -> 5MHz
    //   div=30 -> 1MHz
    uint8_t div;
    if (config->speed_hz >= 30000000) {
        div = 1;
    } else if (config->speed_hz >= 10000000) {
        div = 3;
    } else if (config->speed_hz >= 5000000) {
        div = 6;
    } else if (config->speed_hz >= 1000000) {
        div = 30;
    } else {
        div = 60;  // 500kHz
    }
    
    // Initialize SPI0 as master
    SPI0_MasterDefInit();
    SPI0_CLKCfg(div);
    
    // Configure mode (CPOL, CPHA)
    // Mode 0: CPOL=0, CPHA=0
    // Mode 1: CPOL=0, CPHA=1
    // Mode 2: CPOL=1, CPHA=0
    // Mode 3: CPOL=1, CPHA=1
    switch (config->mode) {
        case 0:
            SPI0_DataMode(Mode0_LowBitINFront);
            break;
        case 1:
            SPI0_DataMode(Mode1_LowBitINFront);
            break;
        case 2:
            SPI0_DataMode(Mode2_LowBitINFront);
            break;
        case 3:
        default:
            SPI0_DataMode(Mode3_LowBitINFront);
            break;
    }
    
    spi_initialized = true;
    return 0;
#else
    (void)config;
    return 0;
#endif
}

static void spi_cs_low(void)
{
    hal_gpio_write(current_cs_pin, false);
}

static void spi_cs_high(void)
{
    hal_gpio_write(current_cs_pin, true);
}

int hal_spi_transfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
#ifdef CH59X
    if (!spi_initialized) {
        return -1;
    }
    
    spi_cs_low();
    
    for (uint16_t i = 0; i < len; i++) {
        uint8_t tx_byte = tx ? tx[i] : 0xFF;
        uint8_t rx_byte = SPI0_MasterSendByte(tx_byte);
        if (rx) {
            rx[i] = rx_byte;
        }
    }
    
    spi_cs_high();
    return 0;
#else
    (void)tx; (void)rx; (void)len;
    return -1;
#endif
}

int hal_spi_read_reg(uint8_t reg, uint8_t *data, uint16_t len)
{
#ifdef CH59X
    if (!spi_initialized || !data) {
        return -1;
    }
    
    spi_cs_low();
    
    // Send register address with read bit (0x80)
    SPI0_MasterSendByte(reg | 0x80);
    
    // Read data
    for (uint16_t i = 0; i < len; i++) {
        data[i] = SPI0_MasterSendByte(0xFF);
    }
    
    spi_cs_high();
    return 0;
#else
    (void)reg; (void)data; (void)len;
    return -1;
#endif
}

int hal_spi_write_reg(uint8_t reg, const uint8_t *data, uint16_t len)
{
#ifdef CH59X
    if (!spi_initialized) {
        return -1;
    }
    
    spi_cs_low();
    
    // Send register address (write bit is 0)
    SPI0_MasterSendByte(reg & 0x7F);
    
    // Write data
    for (uint16_t i = 0; i < len; i++) {
        SPI0_MasterSendByte(data[i]);
    }
    
    spi_cs_high();
    return 0;
#else
    (void)reg; (void)data; (void)len;
    return -1;
#endif
}

int hal_spi_write_byte(uint8_t reg, uint8_t data)
{
    return hal_spi_write_reg(reg, &data, 1);
}

int hal_spi_read_byte(uint8_t reg, uint8_t *data)
{
    return hal_spi_read_reg(reg, data, 1);
}

uint8_t hal_spi_xfer(uint8_t data)
{
#ifdef CH59X
    // 单字节传输 (不控制CS，由调用者控制)
    return SPI0_MasterSendByte(data);
#else
    (void)data;
    return 0;
#endif
}

void hal_spi_cs(uint8_t state)
{
#ifdef CH59X
    if (spi_initialized) {
        hal_gpio_write(current_cs_pin, state != 0);
    }
#else
    (void)state;
#endif
}
