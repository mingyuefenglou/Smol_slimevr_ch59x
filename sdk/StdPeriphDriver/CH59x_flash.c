/**
 * @file CH59x_flash.c
 * @brief CH59X Flash/EEPROM Driver Implementation
 */

#include "CH59x_common.h"
#include <string.h>

typedef struct {
    __IO uint32_t CTRL;
    __IO uint32_t ADDR;
    __IO uint32_t DATA;
    __IO uint32_t STATUS;
} FLASH_TypeDef;

#define FLASH_CTRL  ((FLASH_TypeDef *)FLASH_CTRL_BASE)

#define FLASH_CMD_ERASE     0x01
#define FLASH_CMD_PROGRAM   0x02
#define FLASH_CMD_READ      0x03
#define FLASH_STATUS_BUSY   0x01

static void flash_wait_done(void)
{
    while (FLASH_CTRL->STATUS & FLASH_STATUS_BUSY);
}

uint8_t EEPROM_READ(uint32_t addr, void *data, uint16_t len)
{
    if (addr + len > EEPROM_BASE_ADDR + EEPROM_SIZE) {
        return 1;
    }
    
    uint8_t *dst = (uint8_t *)data;
    uint8_t *src = (uint8_t *)addr;
    
    for (uint16_t i = 0; i < len; i++) {
        dst[i] = src[i];
    }
    
    return 0;
}

uint8_t EEPROM_WRITE(uint32_t addr, void *data, uint16_t len)
{
    if (addr + len > EEPROM_BASE_ADDR + EEPROM_SIZE) {
        return 1;
    }
    
    uint8_t *src = (uint8_t *)data;
    
    for (uint16_t i = 0; i < len; i += 4) {
        uint32_t word = 0xFFFFFFFF;
        uint16_t bytes_to_write = (len - i) > 4 ? 4 : (len - i);
        memcpy(&word, src + i, bytes_to_write);
        
        FLASH_CTRL->ADDR = addr + i;
        FLASH_CTRL->DATA = word;
        FLASH_CTRL->CTRL = FLASH_CMD_PROGRAM;
        flash_wait_done();
    }
    
    return 0;
}

uint8_t EEPROM_ERASE(uint32_t addr, uint16_t len)
{
    if (addr + len > EEPROM_BASE_ADDR + EEPROM_SIZE) {
        return 1;
    }
    
    uint32_t page_addr = addr & ~0xFF;
    uint32_t end_addr = addr + len;
    
    while (page_addr < end_addr) {
        FLASH_CTRL->ADDR = page_addr;
        FLASH_CTRL->CTRL = FLASH_CMD_ERASE;
        flash_wait_done();
        page_addr += 256;
    }
    
    return 0;
}

void GetUniqueID(uint8_t *id)
{
    uint32_t *uid = (uint32_t *)0x1FFFF7E8;
    memcpy(id, uid, 8);
}

void GetMACAddress(uint8_t *mac)
{
    uint8_t uid[8];
    GetUniqueID(uid);
    mac[0] = uid[0] | 0x02;
    mac[1] = uid[1];
    mac[2] = uid[2];
    mac[3] = uid[5];
    mac[4] = uid[6];
    mac[5] = uid[7];
}
