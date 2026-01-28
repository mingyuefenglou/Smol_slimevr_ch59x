/**
 * @file CH59x_flash.h
 * @brief CH59X Flash/EEPROM Driver Header
 */

#ifndef __CH59X_FLASH_H__
#define __CH59X_FLASH_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// Flash layout for CH592
#define FLASH_CODE_START    0x00000000
#define FLASH_CODE_SIZE     0x00070000  // 448KB code flash
#define FLASH_DATA_START    0x00070000  // Data flash start
#define FLASH_DATA_SIZE     0x00008000  // 32KB data flash

// EEPROM emulation area
#define EEPROM_BASE_ADDR    0x00077000
#define EEPROM_SIZE         0x00001000  // 4KB

/**
 * @brief Read from EEPROM area
 * @param addr Address to read from
 * @param data Buffer to store data
 * @param len Number of bytes to read
 * @return 0 on success
 */
uint8_t EEPROM_READ(uint32_t addr, void *data, uint16_t len);

/**
 * @brief Write to EEPROM area
 * @param addr Address to write to
 * @param data Data to write
 * @param len Number of bytes to write
 * @return 0 on success
 */
uint8_t EEPROM_WRITE(uint32_t addr, void *data, uint16_t len);

/**
 * @brief Erase EEPROM area
 * @param addr Start address to erase
 * @param len Number of bytes to erase (rounded up to page)
 * @return 0 on success
 */
uint8_t EEPROM_ERASE(uint32_t addr, uint16_t len);

/**
 * @brief Get unique device ID
 * @param id Buffer to store 8-byte ID
 */
void GetUniqueID(uint8_t *id);

/**
 * @brief Get MAC address
 * @param mac Buffer to store 6-byte MAC
 */
void GetMACAddress(uint8_t *mac);

#ifdef __cplusplus
}
#endif

#endif /* __CH59X_FLASH_H__ */
