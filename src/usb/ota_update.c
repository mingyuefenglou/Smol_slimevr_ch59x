/**
 * @file ota_update.c
 * @brief OTA 无线更新 (可选功能)
 * 
 * 编译开关: OTA_ENABLED
 * 默认禁用，不影响主功能
 * 
 * 启用方法: make OTA=1
 */

#include "hal.h"
#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"
#endif

// 编译开关
#ifndef OTA_ENABLED
#define OTA_ENABLED     0
#endif

#if OTA_ENABLED

/*============================================================================
 * 配置
 *============================================================================*/

#define OTA_MAGIC           0x4F544155  // "OTAU"
#define OTA_BLOCK_SIZE      256
#define OTA_MAX_SIZE        0x38000     // 224KB
#define OTA_PARTITION_ADDR  0x38000     // OTA 分区
#define APP_PARTITION_ADDR  0x1000      // 应用分区
#define OTA_TIMEOUT_MS      30000

/*============================================================================
 * CodeFlash 操作
 *============================================================================*/

#ifdef CH59X
// CodeFlash 擦除
static int codeflash_erase(uint32_t addr, uint32_t len)
{
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    R32_FLASH_ADDR = addr;
    R8_FLASH_COMMAND = ROM_CMD_ERASE;
    while (R8_FLASH_STATUS & 0x01);
    R8_SAFE_ACCESS_SIG = 0;
    return (R8_FLASH_STATUS & 0x04) ? -1 : 0;
}

// CodeFlash 写入
static int codeflash_write(uint32_t addr, const void *data, uint32_t len)
{
    const uint8_t *src = (const uint8_t *)data;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    
    for (uint32_t i = 0; i < len; i++) {
        R32_FLASH_ADDR = addr + i;
        R8_FLASH_DATA = src[i];
        R8_FLASH_COMMAND = ROM_CMD_PROG;
        while (R8_FLASH_STATUS & 0x01);
        if (R8_FLASH_STATUS & 0x04) {
            R8_SAFE_ACCESS_SIG = 0;
            return -1;
        }
    }
    R8_SAFE_ACCESS_SIG = 0;
    return 0;
}
#endif

/*============================================================================
 * 状态
 *============================================================================*/

typedef enum {
    OTA_IDLE = 0,
    OTA_RECEIVING,
    OTA_VERIFYING,
    OTA_COMPLETE,
    OTA_ERROR
} ota_state_t;

typedef struct {
    ota_state_t state;
    uint32_t total_size;
    uint32_t received;
    uint32_t total_blocks;
    uint32_t received_blocks;
    uint32_t start_time;
    uint32_t last_activity;
    uint8_t error_code;
    uint32_t crc32;
} ota_ctx_t;

static ota_ctx_t ota = {0};

/*============================================================================
 * CRC32
 *============================================================================*/

static uint32_t calc_crc32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

/*============================================================================
 * OTA API
 *============================================================================*/

int ota_start(uint32_t size, uint32_t crc)
{
    if (size > OTA_MAX_SIZE) return -1;
    if (ota.state == OTA_RECEIVING) return -2;
    
    memset(&ota, 0, sizeof(ota));
    ota.state = OTA_RECEIVING;
    ota.total_size = size;
    ota.crc32 = crc;
    ota.total_blocks = (size + OTA_BLOCK_SIZE - 1) / OTA_BLOCK_SIZE;
    ota.start_time = hal_get_tick_ms();
    ota.last_activity = ota.start_time;
    
    // 擦除 OTA 分区
#ifdef CH59X
    for (uint32_t addr = OTA_PARTITION_ADDR; addr < OTA_PARTITION_ADDR + size; addr += 256) {
        codeflash_erase(addr, 256);
    }
#endif
    
    return 0;
}

int ota_write_block(uint16_t block_num, const uint8_t *data, uint16_t len)
{
    if (ota.state != OTA_RECEIVING) return -1;
    if (block_num >= ota.total_blocks) return -2;
    
    uint32_t addr = OTA_PARTITION_ADDR + block_num * OTA_BLOCK_SIZE;
    
#ifdef CH59X
    if (codeflash_write(addr, data, len) != 0) {
        ota.state = OTA_ERROR;
        ota.error_code = 1;
        return -3;
    }
#endif
    
    ota.received += len;
    ota.received_blocks++;
    ota.last_activity = hal_get_tick_ms();
    
    if (ota.received >= ota.total_size) {
        ota.state = OTA_VERIFYING;
    }
    
    return 0;
}

int ota_verify(void)
{
    if (ota.state != OTA_VERIFYING) return -1;
    
    uint32_t calc = calc_crc32((uint8_t*)OTA_PARTITION_ADDR, ota.total_size);
    
    if (calc != ota.crc32) {
        ota.state = OTA_ERROR;
        ota.error_code = 2;
        return -1;
    }
    
    ota.state = OTA_COMPLETE;
    return 0;
}

int ota_apply(void)
{
    if (ota.state != OTA_COMPLETE) return -1;
    
#ifdef CH59X
    // 擦除应用分区
    for (uint32_t addr = APP_PARTITION_ADDR; addr < APP_PARTITION_ADDR + ota.total_size; addr += 256) {
        codeflash_erase(addr, 256);
    }
    
    // 复制固件
    for (uint32_t i = 0; i < ota.total_size; i += OTA_BLOCK_SIZE) {
        uint32_t src = OTA_PARTITION_ADDR + i;
        uint32_t dst = APP_PARTITION_ADDR + i;
        uint32_t len = (ota.total_size - i > OTA_BLOCK_SIZE) ? OTA_BLOCK_SIZE : (ota.total_size - i);
        codeflash_write(dst, (void*)src, len);
    }
#endif
    
    return 0;
}

void ota_abort(void)
{
    ota.state = OTA_IDLE;
}

void ota_process(void)
{
    if (ota.state == OTA_RECEIVING) {
        if (hal_get_tick_ms() - ota.last_activity > OTA_TIMEOUT_MS) {
            ota.state = OTA_ERROR;
            ota.error_code = 3;  // Timeout
        }
    }
}

ota_state_t ota_get_state(void) { return ota.state; }
uint8_t ota_get_progress(void) 
{
    if (ota.total_blocks == 0) return 0;
    return (ota.received_blocks * 100) / ota.total_blocks;
}
uint8_t ota_get_error(void) { return ota.error_code; }

#else  // OTA_ENABLED == 0

// 空实现
void ota_init(void) {}
int ota_start(uint32_t size, uint32_t crc) { (void)size; (void)crc; return -1; }
int ota_write_block(uint16_t n, const uint8_t *d, uint16_t l) { (void)n; (void)d; (void)l; return -1; }
int ota_verify(void) { return -1; }
int ota_apply(void) { return -1; }
void ota_abort(void) {}
void ota_process(void) {}
uint8_t ota_get_progress(void) { return 0; }

#endif // OTA_ENABLED
