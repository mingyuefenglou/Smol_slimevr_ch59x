/**
 * @file usb_bootloader.c
 * @brief UF2 USB Bootloader for CH59X
 * 
 * 支持 UF2 格式固件更新
 * - USB MSC (Mass Storage Class) 模拟
 * - UF2 文件解析和 Flash 写入
 * - 固件验证和启动
 */

#include "usb_bootloader.h"
#include "hal.h"
#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"
#endif

/*============================================================================
 * UF2 格式定义
 *============================================================================*/

#define UF2_MAGIC_START0        0x0A324655  // "UF2\n"
#define UF2_MAGIC_START1        0x9E5D5157
#define UF2_MAGIC_END           0x0AB16F30

#define UF2_FLAG_FAMILY_ID      0x00002000
#define UF2_FLAG_NOT_MAIN_FLASH 0x00000001

// CH59X Family ID
#define UF2_FAMILY_CH59X        0x699b62ec

// UF2 Block 结构 (512 bytes)
typedef struct {
    uint32_t magic_start0;      // UF2_MAGIC_START0
    uint32_t magic_start1;      // UF2_MAGIC_START1
    uint32_t flags;             // Flags
    uint32_t target_addr;       // Flash 目标地址
    uint32_t payload_size;      // 有效数据大小 (max 476)
    uint32_t block_no;          // 当前块号
    uint32_t num_blocks;        // 总块数
    uint32_t family_id;         // Family ID (if FLAG_FAMILY_ID set)
    uint8_t  data[476];         // 数据
    uint32_t magic_end;         // UF2_MAGIC_END
} uf2_block_t;

/*============================================================================
 * USB MSC 描述符
 *============================================================================*/

// MSC Device Descriptor
static const uint8_t msc_device_desc[] = {
    18,                     // bLength
    0x01,                   // bDescriptorType
    0x00, 0x02,             // bcdUSB 2.0
    0x00,                   // bDeviceClass
    0x00,                   // bDeviceSubClass
    0x00,                   // bDeviceProtocol
    64,                     // bMaxPacketSize0
    0x09, 0x12,             // idVendor (pid.codes)
    0x91, 0x76,             // idProduct (SlimeVR Boot)
    0x01, 0x00,             // bcdDevice
    1, 2, 3,                // String indices
    1,                      // bNumConfigurations
};

// MSC Configuration Descriptor
static const uint8_t msc_config_desc[] = {
    // Configuration
    9, 0x02, 32, 0, 1, 1, 0, 0x80, 50,
    // Interface (MSC)
    9, 0x04, 0, 0, 2, 0x08, 0x06, 0x50, 0,
    // Endpoint OUT
    7, 0x05, 0x01, 0x02, 64, 0, 0,
    // Endpoint IN
    7, 0x05, 0x81, 0x02, 64, 0, 0,
};

/*============================================================================
 * FAT12 虚拟文件系统
 *============================================================================*/

#define SECTOR_SIZE             512
#define SECTORS_PER_CLUSTER     1
#define RESERVED_SECTORS        1
#define NUM_FATS                2
#define ROOT_ENTRIES            16
#define TOTAL_SECTORS           128  // 64KB 虚拟盘
#define SECTORS_PER_FAT         1

// FAT12 Boot Sector
static const uint8_t fat_boot_sector[SECTOR_SIZE] = {
    0xEB, 0x3C, 0x90,                       // Jump instruction
    'M', 'S', 'D', 'O', 'S', '5', '.', '0', // OEM Name
    SECTOR_SIZE & 0xFF, SECTOR_SIZE >> 8,   // Bytes per sector
    SECTORS_PER_CLUSTER,                    // Sectors per cluster
    RESERVED_SECTORS, 0,                    // Reserved sectors
    NUM_FATS,                               // Number of FATs
    ROOT_ENTRIES, 0,                        // Root entries
    TOTAL_SECTORS & 0xFF, TOTAL_SECTORS >> 8, // Total sectors
    0xF8,                                   // Media descriptor
    SECTORS_PER_FAT, 0,                     // Sectors per FAT
    1, 0,                                   // Sectors per track
    1, 0,                                   // Number of heads
    0, 0, 0, 0,                             // Hidden sectors
    0, 0, 0, 0,                             // Large sectors
    0x00,                                   // Drive number
    0x00,                                   // Reserved
    0x29,                                   // Boot signature
    0x12, 0x34, 0x56, 0x78,                 // Volume serial
    'S', 'L', 'I', 'M', 'E', 'V', 'R', ' ', 'B', 'O', 'O', // Volume label
    'F', 'A', 'T', '1', '2', ' ', ' ', ' ', // File system type
};

// 根目录条目
static const uint8_t fat_root_dir[32] = {
    // Volume label entry
    'S', 'L', 'I', 'M', 'E', 'V', 'R', ' ', 'B', 'O', 'O',
    0x08,                                   // Attribute (volume label)
    0x00, 0x00,                             // Reserved
    0x00, 0x00,                             // Creation time
    0x00, 0x00,                             // Creation date
    0x00, 0x00,                             // Last access date
    0x00, 0x00,                             // High cluster
    0x00, 0x00,                             // Modification time
    0x00, 0x00,                             // Modification date
    0x00, 0x00,                             // Starting cluster
    0x00, 0x00, 0x00, 0x00,                 // File size
};

// INFO_UF2.TXT 内容
static const char info_uf2_txt[] = 
    "UF2 Bootloader v0.4.2\r\n"
    "Model: SlimeVR CH59X\r\n"
    "Board-ID: CH592F-SlimeVR\r\n"
    "Family: 0x699b62ec\r\n";

/*============================================================================
 * Bootloader 状态
 *============================================================================*/

typedef enum {
    BOOT_STATE_IDLE = 0,
    BOOT_STATE_RECEIVING,
    BOOT_STATE_FLASHING,
    BOOT_STATE_COMPLETE,
    BOOT_STATE_ERROR
} boot_state_t;

static struct {
    boot_state_t state;
    uint32_t blocks_received;
    uint32_t total_blocks;
    uint32_t flash_offset;
    uint8_t  last_error;
    bool     flash_valid;
} bootloader_ctx;

// Flash 缓冲区
static uint8_t flash_buffer[256] __attribute__((aligned(4)));

/*============================================================================
 * Flash 操作
 *============================================================================*/

#define APP_START_ADDR      0x00001000  // 4KB after bootloader
#define APP_MAX_SIZE        0x0002F000  // 188KB max (for CH592)
#define FLASH_PAGE_SIZE     256

#ifdef CH59X

static int flash_erase_page(uint32_t addr)
{
    if (addr < APP_START_ADDR || addr >= APP_START_ADDR + APP_MAX_SIZE) {
        return -1;  // Invalid address
    }
    
    return FLASH_ROM_ERASE(addr, FLASH_PAGE_SIZE);
}

static int flash_write_page(uint32_t addr, const uint8_t *data, uint32_t len)
{
    if (addr < APP_START_ADDR || addr >= APP_START_ADDR + APP_MAX_SIZE) {
        return -1;
    }
    
    if (len > FLASH_PAGE_SIZE) {
        len = FLASH_PAGE_SIZE;
    }
    
    return FLASH_ROM_WRITE(addr, (void *)data, len);
}

static int flash_verify(uint32_t addr, const uint8_t *data, uint32_t len)
{
    const uint8_t *flash = (const uint8_t *)addr;
    for (uint32_t i = 0; i < len; i++) {
        if (flash[i] != data[i]) {
            return -1;
        }
    }
    return 0;
}

#endif

/*============================================================================
 * UF2 处理
 *============================================================================*/

static int process_uf2_block(const uf2_block_t *block)
{
    // 验证 Magic
    if (block->magic_start0 != UF2_MAGIC_START0 ||
        block->magic_start1 != UF2_MAGIC_START1 ||
        block->magic_end != UF2_MAGIC_END) {
        return -1;  // Invalid UF2 block
    }
    
    // 检查 Family ID
    if ((block->flags & UF2_FLAG_FAMILY_ID) && block->family_id != UF2_FAMILY_CH59X) {
        return -2;  // Wrong family
    }
    
    // 跳过非 Flash 数据
    if (block->flags & UF2_FLAG_NOT_MAIN_FLASH) {
        return 0;
    }
    
    // 验证地址范围
    uint32_t addr = block->target_addr;
    if (addr < APP_START_ADDR || addr >= APP_START_ADDR + APP_MAX_SIZE) {
        return -3;  // Invalid address
    }
    
    // 验证数据大小
    if (block->payload_size > 256) {
        return -4;  // Payload too large
    }
    
    // 更新状态
    if (bootloader_ctx.state == BOOT_STATE_IDLE) {
        bootloader_ctx.state = BOOT_STATE_RECEIVING;
        bootloader_ctx.total_blocks = block->num_blocks;
        bootloader_ctx.blocks_received = 0;
    }
    
    // 写入 Flash
#ifdef CH59X
    // 擦除页 (每 256 字节)
    if ((addr & 0xFF) == 0) {
        if (flash_erase_page(addr) != 0) {
            bootloader_ctx.last_error = 1;
            return -5;
        }
    }
    
    // 写入数据
    memcpy(flash_buffer, block->data, block->payload_size);
    if (flash_write_page(addr, flash_buffer, block->payload_size) != 0) {
        bootloader_ctx.last_error = 2;
        return -6;
    }
    
    // 验证
    if (flash_verify(addr, flash_buffer, block->payload_size) != 0) {
        bootloader_ctx.last_error = 3;
        return -7;
    }
#endif
    
    bootloader_ctx.blocks_received++;
    
    // 检查完成
    if (bootloader_ctx.blocks_received >= bootloader_ctx.total_blocks) {
        bootloader_ctx.state = BOOT_STATE_COMPLETE;
        bootloader_ctx.flash_valid = true;
    }
    
    return 0;
}

/*============================================================================
 * SCSI 命令处理
 *============================================================================*/

#define SCSI_CMD_TEST_UNIT_READY    0x00
#define SCSI_CMD_REQUEST_SENSE      0x03
#define SCSI_CMD_INQUIRY            0x12
#define SCSI_CMD_MODE_SENSE_6       0x1A
#define SCSI_CMD_READ_CAPACITY      0x25
#define SCSI_CMD_READ_10            0x28
#define SCSI_CMD_WRITE_10           0x2A

// CBW/CSW 结构
typedef struct {
    uint32_t signature;         // 0x43425355
    uint32_t tag;
    uint32_t data_length;
    uint8_t  flags;
    uint8_t  lun;
    uint8_t  cb_length;
    uint8_t  cb[16];
} __attribute__((packed)) cbw_t;

typedef struct {
    uint32_t signature;         // 0x53425355
    uint32_t tag;
    uint32_t data_residue;
    uint8_t  status;
} __attribute__((packed)) csw_t;

static uint8_t scsi_response[64];
static csw_t csw;

static void scsi_handle_inquiry(const cbw_t *cbw)
{
    memset(scsi_response, 0, 36);
    scsi_response[0] = 0x00;    // Direct access device
    scsi_response[1] = 0x80;    // Removable
    scsi_response[2] = 0x02;    // SPC-2
    scsi_response[3] = 0x02;    // Response format
    scsi_response[4] = 36 - 5;  // Additional length
    
    memcpy(&scsi_response[8],  "SlimeVR ", 8);   // Vendor
    memcpy(&scsi_response[16], "CH59X Boot     ", 16); // Product
    memcpy(&scsi_response[32], "0.42", 4);       // Revision
}

static void scsi_handle_read_capacity(const cbw_t *cbw)
{
    uint32_t last_lba = TOTAL_SECTORS - 1;
    
    scsi_response[0] = (last_lba >> 24) & 0xFF;
    scsi_response[1] = (last_lba >> 16) & 0xFF;
    scsi_response[2] = (last_lba >> 8) & 0xFF;
    scsi_response[3] = last_lba & 0xFF;
    
    scsi_response[4] = 0;
    scsi_response[5] = 0;
    scsi_response[6] = (SECTOR_SIZE >> 8) & 0xFF;
    scsi_response[7] = SECTOR_SIZE & 0xFF;
}

static void scsi_handle_read(const cbw_t *cbw, uint8_t *data)
{
    uint32_t lba = (cbw->cb[2] << 24) | (cbw->cb[3] << 16) |
                   (cbw->cb[4] << 8) | cbw->cb[5];
    uint16_t blocks = (cbw->cb[7] << 8) | cbw->cb[8];
    
    memset(data, 0, SECTOR_SIZE);
    
    if (lba == 0) {
        // Boot sector
        memcpy(data, fat_boot_sector, sizeof(fat_boot_sector));
    } else if (lba == 1 || lba == 2) {
        // FAT
        data[0] = 0xF8;
        data[1] = 0xFF;
        data[2] = 0xFF;
    } else if (lba == 3) {
        // Root directory
        memcpy(data, fat_root_dir, sizeof(fat_root_dir));
    }
}

static void scsi_handle_write(const cbw_t *cbw, const uint8_t *data)
{
    uint32_t lba = (cbw->cb[2] << 24) | (cbw->cb[3] << 16) |
                   (cbw->cb[4] << 8) | cbw->cb[5];
    
    // 检查是否是 UF2 块
    if (((const uf2_block_t *)data)->magic_start0 == UF2_MAGIC_START0) {
        process_uf2_block((const uf2_block_t *)data);
    }
}

/*============================================================================
 * Bootloader API
 *============================================================================*/

int bootloader_init(void)
{
    memset(&bootloader_ctx, 0, sizeof(bootloader_ctx));
    bootloader_ctx.state = BOOT_STATE_IDLE;
    
    // 检查是否有有效的 APP
#ifdef CH59X
    uint32_t *app_vector = (uint32_t *)APP_START_ADDR;
    if (app_vector[0] != 0xFFFFFFFF && app_vector[1] != 0xFFFFFFFF) {
        bootloader_ctx.flash_valid = true;
    }
#endif
    
    return 0;
}

bool bootloader_check_app_valid(void)
{
    return bootloader_ctx.flash_valid;
}

void bootloader_jump_to_app(void)
{
#ifdef CH59X
    // 禁用中断
    PFIC_DisableIRQ(USB_IRQn);
    
    // 跳转到 APP
    typedef void (*app_entry_t)(void);
    uint32_t *app_vector = (uint32_t *)APP_START_ADDR;
    app_entry_t app_entry = (app_entry_t)app_vector[1];
    
    // 设置 MSP 并跳转
    __asm volatile (
        "la sp, %0\n"
        "jr %1\n"
        :
        : "i" (0x20008000), "r" (app_entry)
    );
#endif
}

bool bootloader_should_stay(void)
{
#ifdef CH59X
    // 检查按键 (PB4 低电平时进入 Bootloader)
    GPIOB_ModeCfg(GPIO_Pin_4, GPIO_ModeIN_PU);
    DelayMs(10);
    
    if (GPIOB_ReadPortPin(GPIO_Pin_4) == 0) {
        return true;  // 按键按下，停留在 Bootloader
    }
    
    // 检查 Flash 是否有效
    if (!bootloader_ctx.flash_valid) {
        return true;  // 没有有效 APP，停留在 Bootloader
    }
#endif
    
    return false;
}

boot_state_t bootloader_get_state(void)
{
    return bootloader_ctx.state;
}

uint8_t bootloader_get_progress(void)
{
    if (bootloader_ctx.total_blocks == 0) return 0;
    return (bootloader_ctx.blocks_received * 100) / bootloader_ctx.total_blocks;
}

void bootloader_reset(void)
{
#ifdef CH59X
    PFIC_SystemReset();
#endif
}
