/**
 * @file usb_bootloader.h
 * @brief USB Mass Storage Bootloader for CH592 (Optimized)
 * 
 * Memory optimizations:
 * - Reduced virtual disk size (2MB)
 * - Smaller block bitmap
 * - Compact context structure
 * - ~400 bytes RAM total
 */

#ifndef __USB_BOOTLOADER_H__
#define __USB_BOOTLOADER_H__

#include <stdint.h>
#include <stdbool.h>
#include "optimize.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Configuration (Optimized)
 *============================================================================*/

// USB Drive Label
#define USB_DRIVE_LABEL         "NINI_SLIME "

// UF2 Configuration
#define UF2_MAGIC_START0        0x0A324655
#define UF2_MAGIC_START1        0x9E5D5157
#define UF2_MAGIC_END           0x0AB16F30
#define UF2_FAMILY_ID_CH592     0x4B37B5A5

// Flash regions (after 4KB bootloader)
#define FLASH_APP_START         0x00001000
#define FLASH_APP_END           0x0006FFFF
#define FLASH_APP_SIZE          (FLASH_APP_END - FLASH_APP_START + 1)
#define FLASH_PAGE_SIZE         256
#define FLASH_SECTOR_SIZE       4096

// Virtual disk (reduced to 2MB)
#define VDISK_SECTOR_SIZE       512
#define VDISK_SECTOR_COUNT      4096    // 2MB
#define VDISK_CLUSTER_SIZE      4096

// Timeout
#define UPDATE_TIMEOUT_MS       180000

/*============================================================================
 * UF2 Block Structure
 *============================================================================*/

typedef struct PACKED {
    u32 magic_start0;
    u32 magic_start1;
    u32 flags;
    u32 target_addr;
    u32 payload_size;
    u32 block_no;
    u32 num_blocks;
    u32 family_id;
    u8  data[476];
    u32 magic_end;
} uf2_block_t;

#define UF2_FLAG_FAMILY_ID      0x00002000
#define UF2_FLAG_NOT_MAIN_FLASH 0x00000001

/*============================================================================
 * Bootloader State (Compact)
 *============================================================================*/

typedef enum {
    BOOT_STATE_IDLE = 0,
    BOOT_STATE_UPDATE_MODE,
    BOOT_STATE_PROGRAMMING,
    BOOT_STATE_COMPLETE,
    BOOT_STATE_ERROR,
} boot_state_t;

typedef enum {
    BOOT_ERROR_NONE = 0,
    BOOT_ERROR_INVALID_UF2,
    BOOT_ERROR_INVALID_FAMILY,
    BOOT_ERROR_FLASH_FAIL,
    BOOT_ERROR_VERIFY_FAIL,
    BOOT_ERROR_TIMEOUT,
} boot_error_t;

/*============================================================================
 * Bootloader Context (~300 bytes)
 *============================================================================*/

typedef struct PACKED {
    u8  state;                  // boot_state_t
    u8  error;                  // boot_error_t
    u16 total_blocks;
    u16 received_blocks;
    u32 written_bytes;
    u32 start_time_ms;
    u32 last_activity_ms;
    u32 last_erased_sector;
    
    // Block bitmap - supports up to 2048 blocks (~1MB firmware)
    u8  block_bitmap[256];
    
    // Error message (compact)
    char error_msg[64];
} bootloader_ctx_t;

/*============================================================================
 * API Functions
 *============================================================================*/

int bootloader_init(void);
int bootloader_enter_update_mode(void);
void bootloader_exit_update_mode(bool reboot);
void bootloader_process(void);
bool bootloader_is_update_mode(void);
boot_state_t bootloader_get_state(void);
boot_error_t bootloader_get_error(void);
u8 bootloader_get_progress(void);

/*============================================================================
 * USB MSC Callbacks
 *============================================================================*/

void msc_get_capacity(u32 *block_count, u32 *block_size);
s32 msc_read(u32 lba, u32 offset, void *buffer, u32 length);
s32 msc_write(u32 lba, u32 offset, const void *buffer, u32 length);
bool msc_is_ready(void);
bool msc_is_writable(void);

#ifdef __cplusplus
}
#endif

#endif /* __USB_BOOTLOADER_H__ */
