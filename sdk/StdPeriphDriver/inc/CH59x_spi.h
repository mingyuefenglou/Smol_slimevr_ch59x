/**
 * @file CH59x_spi.h
 * @brief CH59X SPI Driver Header
 */

#ifndef __CH59X_SPI_H__
#define __CH59X_SPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*============================================================================
 * SPI Mode Definitions
 *============================================================================*/

typedef enum {
    Mode0_LowBitINFront,    // CPOL=0, CPHA=0, LSB first
    Mode0_HighBitINFront,   // CPOL=0, CPHA=0, MSB first
    Mode1_LowBitINFront,    // CPOL=0, CPHA=1, LSB first
    Mode1_HighBitINFront,   // CPOL=0, CPHA=1, MSB first
    Mode2_LowBitINFront,    // CPOL=1, CPHA=0, LSB first
    Mode2_HighBitINFront,   // CPOL=1, CPHA=0, MSB first
    Mode3_LowBitINFront,    // CPOL=1, CPHA=1, LSB first
    Mode3_HighBitINFront,   // CPOL=1, CPHA=1, MSB first
} SPI_ModeTypeDef;

/*============================================================================
 * SPI0 Functions
 *============================================================================*/

/**
 * @brief Initialize SPI0 as master with default settings
 */
void SPI0_MasterDefInit(void);

/**
 * @brief Configure SPI0 clock divider
 * @param div Clock divider (1-255), SPI_CLK = SYS_CLK / (div * 2)
 */
void SPI0_CLKCfg(uint8_t div);

/**
 * @brief Configure SPI0 data mode
 * @param mode SPI mode (CPOL, CPHA, bit order)
 */
void SPI0_DataMode(SPI_ModeTypeDef mode);

/**
 * @brief Send and receive a single byte via SPI0
 * @param data Byte to send
 * @return Received byte
 */
uint8_t SPI0_MasterSendByte(uint8_t data);

/**
 * @brief Send multiple bytes via SPI0
 * @param data Data buffer to send
 * @param len Number of bytes to send
 */
void SPI0_MasterSendData(const uint8_t *data, uint16_t len);

/**
 * @brief Receive multiple bytes via SPI0
 * @param data Buffer to store received data
 * @param len Number of bytes to receive
 */
void SPI0_MasterRecvData(uint8_t *data, uint16_t len);

/**
 * @brief Enable/Disable SPI0
 * @param state ENABLE or DISABLE
 */
void SPI0_Enable(uint8_t state);

/*============================================================================
 * SPI1 Functions
 *============================================================================*/

void SPI1_MasterDefInit(void);
void SPI1_CLKCfg(uint8_t div);
void SPI1_DataMode(SPI_ModeTypeDef mode);
uint8_t SPI1_MasterSendByte(uint8_t data);
void SPI1_MasterSendData(const uint8_t *data, uint16_t len);
void SPI1_MasterRecvData(uint8_t *data, uint16_t len);
void SPI1_Enable(uint8_t state);

/*============================================================================
 * SPI Slave Functions
 *============================================================================*/

void SPI0_SlaveInit(void);
void SPI1_SlaveInit(void);

#ifdef __cplusplus
}
#endif

#endif /* __CH59X_SPI_H__ */
