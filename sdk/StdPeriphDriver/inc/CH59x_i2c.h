/**
 * @file CH59x_i2c.h
 * @brief CH59X I2C Driver Header
 */

#ifndef __CH59X_I2C_H__
#define __CH59X_I2C_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*============================================================================
 * I2C Status Definitions
 *============================================================================*/

#define I2C_STATUS_START        0x08    // START condition transmitted
#define I2C_STATUS_RSTART       0x10    // Repeated START transmitted
#define I2C_STATUS_SLAW_ACK     0x18    // SLA+W transmitted, ACK received
#define I2C_STATUS_SLAW_NACK    0x20    // SLA+W transmitted, NACK received
#define I2C_STATUS_DATA_ACK     0x28    // Data transmitted, ACK received
#define I2C_STATUS_DATA_NACK    0x30    // Data transmitted, NACK received
#define I2C_STATUS_SLAR_ACK     0x40    // SLA+R transmitted, ACK received
#define I2C_STATUS_SLAR_NACK    0x48    // SLA+R transmitted, NACK received
#define I2C_STATUS_RECV_ACK     0x50    // Data received, ACK returned
#define I2C_STATUS_RECV_NACK    0x58    // Data received, NACK returned

/*============================================================================
 * I2C Functions
 *============================================================================*/

/**
 * @brief Initialize I2C in master mode with default settings
 */
void I2C_Init(void);

/**
 * @brief Configure I2C clock
 * @param clock_hz Desired I2C clock frequency in Hz
 */
void I2C_SetClock(uint32_t clock_hz);

/**
 * @brief Generate I2C START condition
 */
void I2C_Start(void);

/**
 * @brief Generate I2C STOP condition
 */
void I2C_Stop(void);

/**
 * @brief Send a byte on I2C bus
 * @param data Byte to send
 */
void I2C_SendByte(uint8_t data);

/**
 * @brief Receive a byte from I2C bus
 * @param ack Non-zero to send ACK, zero for NACK
 * @return Received byte
 */
uint8_t I2C_RecvByte(uint8_t ack);

/**
 * @brief Wait for ACK from slave
 * @return 0 if ACK received, non-zero for NACK
 */
uint8_t I2C_WaitAck(void);

/**
 * @brief Send ACK signal
 */
void I2C_Ack(void);

/**
 * @brief Send NACK signal
 */
void I2C_NAck(void);

/**
 * @brief Get I2C status
 * @return I2C status register value
 */
uint8_t I2C_GetStatus(void);

/**
 * @brief Check if I2C bus is busy
 * @return Non-zero if busy
 */
uint8_t I2C_IsBusy(void);

/**
 * @brief Master mode write to slave
 * @param addr 7-bit slave address
 * @param data Data buffer to write
 * @param len Number of bytes to write
 * @return 0 on success, non-zero on error
 */
uint8_t I2C_MasterWrite(uint8_t addr, const uint8_t *data, uint16_t len);

/**
 * @brief Master mode read from slave
 * @param addr 7-bit slave address
 * @param data Buffer to store received data
 * @param len Number of bytes to read
 * @return 0 on success, non-zero on error
 */
uint8_t I2C_MasterRead(uint8_t addr, uint8_t *data, uint16_t len);

/**
 * @brief Master mode write then read (combined transaction)
 * @param addr 7-bit slave address
 * @param wdata Data to write
 * @param wlen Write length
 * @param rdata Buffer for read data
 * @param rlen Read length
 * @return 0 on success, non-zero on error
 */
uint8_t I2C_MasterWriteRead(uint8_t addr, const uint8_t *wdata, uint16_t wlen,
                             uint8_t *rdata, uint16_t rlen);

#ifdef __cplusplus
}
#endif

#endif /* __CH59X_I2C_H__ */
