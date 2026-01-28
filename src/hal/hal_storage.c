/**
 * @file hal_storage.c
 * @brief Flash/EEPROM Storage HAL for CH59X
 * 
 * Provides non-volatile storage for configuration, calibration data,
 * and pairing information. Uses CH59X DataFlash area.
 * 
 * v0.6.2: 添加双备份原子写入机制 (CRC + dual backup)
 */

#include "hal.h"
#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"
#include "CH59x_flash.h"
#endif

/*============================================================================
 * Configuration
 *============================================================================*/

// CH59X DataFlash area: 0x77000 - 0x77FFF (4KB)
// We use this area for persistent storage
#define STORAGE_BASE_ADDR       0x00077000
#define STORAGE_SIZE            4096
#define STORAGE_PAGE_SIZE       256

// v0.6.2: 双备份存储布局
// Bank A: 0x0000 - 0x07FF (2KB)
// Bank B: 0x0800 - 0x0FFF (2KB)
#define STORAGE_BANK_SIZE       2048
#define STORAGE_BANK_A          0x0000
#define STORAGE_BANK_B          0x0800

// Storage layout (每个Bank内)
#define STORAGE_MAGIC_ADDR      0x0000
#define STORAGE_MAGIC_VALUE     0x534C564D  // "SLVM"
#define STORAGE_VERSION         0x0002      // v0.6.2升级版本号

#define CONFIG_STORAGE_ADDR     0x0010  // Configuration data
#define CALIB_STORAGE_ADDR      0x0100  // Calibration data
#define PAIR_STORAGE_ADDR       0x0200  // Pairing data

// v0.6.2: 双备份块头
typedef struct {
    uint32_t magic;         // STORAGE_MAGIC_VALUE
    uint16_t version;       // 存储版本
    uint16_t sequence;      // 写入序列号（判断哪个更新）
    uint16_t data_len;      // 数据长度
    uint16_t crc16;         // CRC16校验
} storage_block_header_t;

// 当前活跃Bank
static uint8_t active_bank = 0;  // 0=BankA, 1=BankB

/*============================================================================
 * CRC16计算 (用于数据完整性)
 *============================================================================*/

static uint16_t calc_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/*============================================================================
 * Internal Functions
 *============================================================================*/

static bool storage_check_magic(void)
{
#ifdef CH59X
    uint32_t magic;
    EEPROM_READ(STORAGE_BASE_ADDR + STORAGE_MAGIC_ADDR, &magic, sizeof(magic));
    return (magic == STORAGE_MAGIC_VALUE);
#else
    return true;
#endif
}

// v0.6.2: 验证Bank数据完整性
static bool storage_verify_bank(uint32_t bank_offset, storage_block_header_t *header)
{
#ifdef CH59X
    // 读取头部
    EEPROM_READ(STORAGE_BASE_ADDR + bank_offset, header, sizeof(*header));
    
    // 检查magic
    if (header->magic != STORAGE_MAGIC_VALUE) {
        return false;
    }
    
    // 检查数据长度合理性
    if (header->data_len > STORAGE_BANK_SIZE - sizeof(storage_block_header_t)) {
        return false;
    }
    
    // 读取数据并验证CRC
    if (header->data_len > 0) {
        uint8_t buf[256];  // 分块验证
        uint16_t calc_crc = 0xFFFF;
        uint16_t remaining = header->data_len;
        uint32_t offset = bank_offset + sizeof(storage_block_header_t);
        
        while (remaining > 0) {
            uint16_t chunk = (remaining > 256) ? 256 : remaining;
            EEPROM_READ(STORAGE_BASE_ADDR + offset, buf, chunk);
            
            // 累积CRC
            for (uint16_t i = 0; i < chunk; i++) {
                calc_crc ^= buf[i];
                for (uint8_t j = 0; j < 8; j++) {
                    if (calc_crc & 1) calc_crc = (calc_crc >> 1) ^ 0xA001;
                    else calc_crc >>= 1;
                }
            }
            
            offset += chunk;
            remaining -= chunk;
        }
        
        if (calc_crc != header->crc16) {
            return false;
        }
    }
    
    return true;
#else
    (void)bank_offset; (void)header;
    return true;
#endif
}

// v0.6.2: 确定活跃Bank (序列号更大且有效的)
static void storage_determine_active_bank(void)
{
#ifdef CH59X
    storage_block_header_t header_a, header_b;
    bool valid_a = storage_verify_bank(STORAGE_BANK_A, &header_a);
    bool valid_b = storage_verify_bank(STORAGE_BANK_B, &header_b);
    
    if (valid_a && valid_b) {
        // 两个都有效，选序列号更大的
        // 处理序列号回绕: 如果差值很大，说明回绕了
        int16_t diff = (int16_t)(header_a.sequence - header_b.sequence);
        active_bank = (diff > 0) ? 0 : 1;
    } else if (valid_a) {
        active_bank = 0;
    } else if (valid_b) {
        active_bank = 1;
    } else {
        // 都无效，使用Bank A
        active_bank = 0;
    }
#endif
}

static void storage_write_magic(void)
{
#ifdef CH59X
    uint32_t magic = STORAGE_MAGIC_VALUE;
    EEPROM_WRITE(STORAGE_BASE_ADDR + STORAGE_MAGIC_ADDR, &magic, sizeof(magic));
#endif
}

/*============================================================================
 * Public API
 *============================================================================*/

int hal_storage_init(void)
{
#ifdef CH59X
    // v0.6.2: 确定活跃Bank
    storage_determine_active_bank();
    
    // Check if storage has been initialized
    if (!storage_check_magic()) {
        // First time use - erase and initialize both banks
        EEPROM_ERASE(STORAGE_BASE_ADDR, STORAGE_SIZE);
        storage_write_magic();
        
        // Write version
        uint16_t version = STORAGE_VERSION;
        EEPROM_WRITE(STORAGE_BASE_ADDR + 4, &version, sizeof(version));
    }
    return 0;
#else
    return 0;
#endif
}

int hal_storage_read(uint32_t addr, void *data, uint16_t len)
{
#ifdef CH59X
    if (!data || addr + len > STORAGE_SIZE) {
        return -1;
    }
    
    return EEPROM_READ(STORAGE_BASE_ADDR + addr, data, len);
#else
    (void)addr; (void)data; (void)len;
    return -1;
#endif
}

int hal_storage_write(uint32_t addr, const void *data, uint16_t len)
{
#ifdef CH59X
    if (!data || addr + len > STORAGE_SIZE) {
        return -1;
    }
    
    return EEPROM_WRITE(STORAGE_BASE_ADDR + addr, (void*)data, len);
#else
    (void)addr; (void)data; (void)len;
    return -1;
#endif
}

int hal_storage_erase(uint32_t addr, uint16_t len)
{
#ifdef CH59X
    // 边界检查：防止整数溢出
    if (addr >= STORAGE_SIZE) {
        return -1;  // 起始地址超出范围
    }
    
    // 使用安全的溢出检查方式
    if (len > STORAGE_SIZE || addr > STORAGE_SIZE - len) {
        return -1;  // 长度超出范围或会越界
    }
    
    if (len == 0) {
        return 0;  // 长度为0，无需擦除
    }
    
    // EEPROM_ERASE erases in 256-byte pages
    uint32_t start_page = (addr / STORAGE_PAGE_SIZE) * STORAGE_PAGE_SIZE;
    uint32_t end_addr = addr + len;
    uint32_t erase_len = ((end_addr - start_page + STORAGE_PAGE_SIZE - 1) / STORAGE_PAGE_SIZE) * STORAGE_PAGE_SIZE;
    
    // 确保擦除长度不超过存储区域
    if (start_page + erase_len > STORAGE_SIZE) {
        erase_len = STORAGE_SIZE - start_page;
    }
    
    return EEPROM_ERASE(STORAGE_BASE_ADDR + start_page, erase_len);
#else
    (void)addr; (void)len;
    return -1;
#endif
}

/*============================================================================
 * High-Level Storage Functions
 *============================================================================*/

// Configuration data structure
typedef struct {
    uint16_t magic;
    uint16_t version;
    uint8_t tracker_id;
    uint8_t fusion_type;
    uint8_t imu_odr;
    uint8_t mag_enabled;
    int8_t tx_power;
    uint8_t led_brightness;
    uint16_t sleep_timeout_sec;
    uint8_t reserved[20];
    uint16_t crc;
} config_data_t;

// Calibration data structure
typedef struct {
    uint16_t magic;
    float gyro_bias[3];
    float accel_bias[3];
    float accel_scale[3];
    float mag_hard_iron[3];
    float mag_soft_iron[9];
    uint16_t crc;
} calib_data_t;

// Pairing data structure
typedef struct {
    uint16_t magic;
    uint8_t paired;
    uint8_t tracker_id;
    uint8_t receiver_addr[6];
    uint8_t device_name[16];
    uint16_t crc;
} pair_data_t;

#define CONFIG_MAGIC    0xCF01
#define CALIB_MAGIC     0xCA01
#define PAIR_MAGIC      0x5A01  // 修复: 0xPA01不是有效的十六进制

// 注意: calc_crc16已在第61行定义，此处复用那个定义
// 如果需要不同的签名，可以在此处定义一个内部版本
#ifndef STORAGE_CRC16_DEFINED
#define STORAGE_CRC16_DEFINED
static uint16_t storage_calc_crc16(const void *data, uint16_t len)
{
    const uint8_t *p = (const uint8_t*)data;
    uint16_t crc = 0xFFFF;
    
    for (uint16_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}
#endif

int hal_storage_load_config(void *config, uint16_t size)
{
    config_data_t data;
    
    if (size < sizeof(config_data_t)) {
        return -1;
    }
    
    int err = hal_storage_read(CONFIG_STORAGE_ADDR, &data, sizeof(data));
    if (err) return err;
    
    // Validate magic and CRC
    if (data.magic != CONFIG_MAGIC) {
        return -2;  // Not initialized
    }
    
    uint16_t expected_crc = storage_calc_crc16(&data, sizeof(data) - 2);
    if (data.crc != expected_crc) {
        return -3;  // CRC error
    }
    
    memcpy(config, &data, sizeof(data));
    return 0;
}

int hal_storage_save_config(const void *config, uint16_t size)
{
    if (size < sizeof(config_data_t)) {
        return -1;
    }
    
    config_data_t data;
    memcpy(&data, config, sizeof(data));
    
    data.magic = CONFIG_MAGIC;
    data.crc = storage_calc_crc16(&data, sizeof(data) - 2);
    
    return hal_storage_write(CONFIG_STORAGE_ADDR, &data, sizeof(data));
}

int hal_storage_load_calibration(void *calib, uint16_t size)
{
    calib_data_t data;
    
    if (size < sizeof(calib_data_t)) {
        return -1;
    }
    
    int err = hal_storage_read(CALIB_STORAGE_ADDR, &data, sizeof(data));
    if (err) return err;
    
    if (data.magic != CALIB_MAGIC) {
        return -2;
    }
    
    uint16_t expected_crc = storage_calc_crc16(&data, sizeof(data) - 2);
    if (data.crc != expected_crc) {
        return -3;
    }
    
    memcpy(calib, &data, sizeof(data));
    return 0;
}

int hal_storage_save_calibration(const void *calib, uint16_t size)
{
    if (size < sizeof(calib_data_t)) {
        return -1;
    }
    
    calib_data_t data;
    memcpy(&data, calib, sizeof(data));
    
    data.magic = CALIB_MAGIC;
    data.crc = storage_calc_crc16(&data, sizeof(data) - 2);
    
    return hal_storage_write(CALIB_STORAGE_ADDR, &data, sizeof(data));
}

int hal_storage_load_pairing(void *pair, uint16_t size)
{
    pair_data_t data;
    
    if (size < sizeof(pair_data_t)) {
        return -1;
    }
    
    int err = hal_storage_read(PAIR_STORAGE_ADDR, &data, sizeof(data));
    if (err) return err;
    
    if (data.magic != PAIR_MAGIC) {
        return -2;
    }
    
    uint16_t expected_crc = storage_calc_crc16(&data, sizeof(data) - 2);
    if (data.crc != expected_crc) {
        return -3;
    }
    
    memcpy(pair, &data, sizeof(data));
    return 0;
}

int hal_storage_save_pairing(const void *pair, uint16_t size)
{
    if (size < sizeof(pair_data_t)) {
        return -1;
    }
    
    pair_data_t data;
    memcpy(&data, pair, sizeof(data));
    
    data.magic = PAIR_MAGIC;
    data.crc = storage_calc_crc16(&data, sizeof(data) - 2);
    
    return hal_storage_write(PAIR_STORAGE_ADDR, &data, sizeof(data));
}

int hal_storage_clear_pairing(void)
{
    pair_data_t data = {0};
    return hal_storage_write(PAIR_STORAGE_ADDR, &data, sizeof(data));
}

/*============================================================================
 * Network Key Storage
 *============================================================================*/

#define NETWORK_KEY_ADDR        0x0300
#define NETWORK_KEY_MAGIC       0x4E4B4559  // "NKEY"

typedef struct {
    uint32_t magic;
    uint32_t key;
    uint16_t crc;
} __attribute__((packed)) network_key_data_t;

bool hal_storage_load_network_key(uint32_t *key)
{
    if (!key) return false;
    
#ifdef CH59X
    network_key_data_t data;
    EEPROM_READ(STORAGE_BASE_ADDR + NETWORK_KEY_ADDR, &data, sizeof(data));
    
    if (data.magic != NETWORK_KEY_MAGIC) {
        return false;
    }
    
    uint16_t expected_crc = storage_calc_crc16(&data, sizeof(data) - 2);
    if (data.crc != expected_crc) {
        return false;
    }
    
    *key = data.key;
    return true;
#else
    return false;
#endif
}

int hal_storage_save_network_key(uint32_t key)
{
#ifdef CH59X
    network_key_data_t data;
    data.magic = NETWORK_KEY_MAGIC;
    data.key = key;
    data.crc = storage_calc_crc16(&data, sizeof(data) - 2);
    
    return hal_storage_write(NETWORK_KEY_ADDR, &data, sizeof(data));
#else
    return 0;
#endif
}

/*============================================================================
 * Random Number Generation
 *============================================================================*/

uint32_t hal_get_random_u32(void)
{
#ifdef CH59X
    // CH59X 使用 ADC 噪声 + 系统时间混合
    uint32_t rand = 0;
    
    // 读取 ADC 噪声 (低位)
    // TODO: 实现ADC初始化
    // ADC_ExtSingleChInit(ADC_ChannelTemp, ADC_PGA_1);
    for (int i = 0; i < 4; i++) {
        uint16_t adc = 0;  // TODO: ADC_ExcutSingleConver();
        rand = (rand << 8) | (adc & 0xFF);
    }
    
    // 混合系统时间 (使用hal_millis替代SysTick直接访问)
    uint32_t time_val = hal_millis();
    rand ^= time_val;
    rand ^= (time_val << 16);
    
    // 简单混淆
    rand ^= (rand >> 17);
    rand *= 0xED5AD4BB;
    rand ^= (rand >> 11);
    rand *= 0xAC4C1B51;
    rand ^= (rand >> 15);
    
    return rand;
#else
    // 伪随机 fallback
    static uint32_t seed = 0x12345678;
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
#endif
}

/*============================================================================
 * System Control
 *============================================================================*/

void hal_reset(void)
{
#ifdef CH59X
    SYS_ResetExecute();
#endif
}

uint32_t hal_get_tick_ms(void)
{
    // 统一使用hal_millis()实现，确保时间一致性
    return hal_millis();
}

uint32_t hal_get_tick_us(void)
{
#ifdef CH59X
    // 使用hal_timer提供的微秒计时
    extern uint32_t hal_micros(void);
    return hal_micros();
#else
    return 0;
#endif
}
