/**
 * @file hal.h
 * @brief Hardware Abstraction Layer for CH59X SlimeVR Tracker
 * 
 * This header provides a unified interface for hardware operations,
 * making it easier to port sensor drivers from the nRF version.
 */

#ifndef __HAL_H__
#define __HAL_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * I2C Interface
 *============================================================================*/

typedef struct {
    uint8_t addr;       // 7-bit I2C address
    uint32_t speed_hz;  // I2C clock speed
} hal_i2c_config_t;

/**
 * @brief Initialize I2C peripheral
 * @param config I2C configuration
 * @return 0 on success, negative on error
 */
int hal_i2c_init(const hal_i2c_config_t *config);

/**
 * @brief Read registers from I2C device
 * @param addr 7-bit device address
 * @param reg Register address
 * @param data Buffer to store read data
 * @param len Number of bytes to read
 * @return 0 on success, negative on error
 */
int hal_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len);

/**
 * @brief Write registers to I2C device
 * @param addr 7-bit device address
 * @param reg Register address
 * @param data Data to write
 * @param len Number of bytes to write
 * @return 0 on success, negative on error
 */
int hal_i2c_write_reg(uint8_t addr, uint8_t reg, const uint8_t *data, uint16_t len);

/**
 * @brief Read single byte from I2C device
 */
static inline int hal_i2c_read_byte(uint8_t addr, uint8_t reg, uint8_t *data) {
    return hal_i2c_read_reg(addr, reg, data, 1);
}

/**
 * @brief Write single byte to I2C device
 */
static inline int hal_i2c_write_byte(uint8_t addr, uint8_t reg, uint8_t data) {
    return hal_i2c_write_reg(addr, reg, &data, 1);
}

/*============================================================================
 * SPI Interface
 *============================================================================*/

typedef struct {
    uint32_t speed_hz;  // SPI clock speed
    uint8_t mode;       // SPI mode (0-3)
    uint8_t cs_pin;     // Chip select GPIO pin
} hal_spi_config_t;

/**
 * @brief Initialize SPI peripheral
 */
int hal_spi_init(const hal_spi_config_t *config);

/**
 * @brief SPI transfer (full duplex)
 */
int hal_spi_transfer(const uint8_t *tx, uint8_t *rx, uint16_t len);

/**
 * @brief Read registers via SPI
 */
int hal_spi_read_reg(uint8_t reg, uint8_t *data, uint16_t len);

/**
 * @brief Write registers via SPI
 */
int hal_spi_write_reg(uint8_t reg, const uint8_t *data, uint16_t len);

/**
 * @brief Single byte SPI transfer (for raw SPI access)
 * @param data Byte to send
 * @return Received byte
 */
uint8_t hal_spi_xfer(uint8_t data);

/**
 * @brief Control SPI chip select
 * @param state 0 = low (active), 1 = high (inactive)
 */
void hal_spi_cs(uint8_t state);

/*============================================================================
 * DMA Interface
 *============================================================================*/

/**
 * @brief Initialize DMA subsystem
 */
void hal_dma_init(void);

/*============================================================================
 * GPIO Interface
 *============================================================================*/

typedef enum {
    HAL_GPIO_INPUT,
    HAL_GPIO_OUTPUT,
    HAL_GPIO_INPUT_PULLUP,
    HAL_GPIO_INPUT_PULLDOWN,
} hal_gpio_mode_t;

typedef enum {
    HAL_GPIO_INT_RISING,
    HAL_GPIO_INT_FALLING,
    HAL_GPIO_INT_BOTH,
} hal_gpio_int_t;

/**
 * @brief Configure GPIO pin
 */
int hal_gpio_config(uint8_t pin, hal_gpio_mode_t mode);

/**
 * @brief Set GPIO output level
 */
void hal_gpio_write(uint8_t pin, bool level);

/**
 * @brief Read GPIO input level
 */
bool hal_gpio_read(uint8_t pin);

/**
 * @brief Configure GPIO interrupt
 */
int hal_gpio_set_interrupt(uint8_t pin, hal_gpio_int_t type, void (*callback)(void));

/*============================================================================
 * Timer Interface
 *============================================================================*/

/**
 * @brief Initialize system timer
 */
int hal_timer_init(void);

/**
 * @brief Get milliseconds since boot
 */
uint32_t hal_millis(void);

/**
 * @brief Get microseconds since boot
 */
uint32_t hal_micros(void);

/**
 * @brief Delay in milliseconds
 */
void hal_delay_ms(uint32_t ms);

/**
 * @brief Delay in microseconds
 */
void hal_delay_us(uint32_t us);

/*============================================================================
 * Power Management
 *============================================================================*/

/**
 * @brief Enter low power mode
 */
void hal_enter_sleep(void);

/**
 * @brief Enter deep sleep (with wakeup source)
 */
void hal_enter_deep_sleep(uint8_t wakeup_pin);

/*============================================================================
 * Flash/EEPROM Storage
 *============================================================================*/

/**
 * @brief Initialize persistent storage
 */
int hal_storage_init(void);

/**
 * @brief Read from persistent storage
 */
int hal_storage_read(uint32_t addr, void *data, uint16_t len);

/**
 * @brief Write to persistent storage
 */
int hal_storage_write(uint32_t addr, const void *data, uint16_t len);

/**
 * @brief Erase storage sector
 */
int hal_storage_erase(uint32_t addr, uint16_t len);

/*============================================================================
 * Network Key Storage (用于 RF 配对安全)
 *============================================================================*/

/**
 * @brief 加载网络密钥
 * @param key 存储密钥的指针
 * @return true 如果密钥存在并已加载, false 如果不存在
 */
bool hal_storage_load_network_key(uint32_t *key);

/**
 * @brief 保存网络密钥
 * @param key 要保存的密钥
 * @return 0 成功, 负数错误
 */
int hal_storage_save_network_key(uint32_t key);

/*============================================================================
 * Random Number Generation
 *============================================================================*/

/**
 * @brief 获取 32 位随机数
 * @return 随机数 (如果硬件不支持则返回伪随机数)
 */
uint32_t hal_get_random_u32(void);

/*============================================================================
 * System Control
 *============================================================================*/

/**
 * @brief 系统复位
 */
void hal_reset(void);

/**
 * @brief 获取系统时间 (毫秒)
 */
uint32_t hal_get_tick_ms(void);

/**
 * @brief 获取系统时间 (微秒)
 */
uint32_t hal_get_tick_us(void);

/*============================================================================
 * CRC Calculation (v0.6.2 统一实现)
 *============================================================================*/

/**
 * @brief 计算CRC16 (Modbus多项式0xA001)
 * @param data 数据指针
 * @param len 数据长度
 * @return CRC16值
 */
uint16_t hal_crc16(const void *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __HAL_H__ */
