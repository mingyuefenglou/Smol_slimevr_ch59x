/**
 * @file retained_state.c
 * @brief Retained State Manager - 唤醒后快速恢复
 * 
 * v0.5.0: 保存和恢复关键状态，实现唤醒后姿态快速稳定
 */

#include "retained_state.h"
#include "hal.h"
#include <string.h>

/*============================================================================
 * 常量
 *============================================================================*/

#define RETAINED_MAGIC          0x52455441  // "RETA"
#define RETAINED_VERSION        1

/*============================================================================
 * 静态变量
 *============================================================================*/

// RAM缓存 (用于快速访问)
static retained_state_t cached_state;
static bool cache_valid = false;

// 上次保存时间 (用于限制写入频率)
static uint32_t last_save_time = 0;
#define MIN_SAVE_INTERVAL_MS    5000    // 最少5秒保存一次

/*============================================================================
 * 内部函数
 *============================================================================*/

/**
 * @brief 计算CRC16
 */
static uint16_t calc_crc16(const void *data, uint16_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint16_t crc = 0xFFFF;
    
    for (uint16_t i = 0; i < len; i++) {
        crc ^= p[i];
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

/**
 * @brief 从Flash读取状态
 */
static int read_from_flash(retained_state_t *state)
{
#if RETAINED_USE_FLASH
    int ret = hal_storage_read(RETAINED_FLASH_OFFSET, state, sizeof(retained_state_t));
    if (ret != 0) return -1;
    
    // 验证魔数
    if (state->magic != RETAINED_MAGIC) {
        return -2;  // 无效数据
    }
    
    // 验证版本
    if (state->version != RETAINED_VERSION) {
        return -3;  // 版本不匹配
    }
    
    // 验证CRC
    uint16_t calc_crc = calc_crc16(state, sizeof(retained_state_t) - 2);
    if (calc_crc != state->crc) {
        return -4;  // CRC错误
    }
    
    return 0;
#else
    return -1;
#endif
}

/**
 * @brief 写入Flash
 */
static int write_to_flash(const retained_state_t *state)
{
#if RETAINED_USE_FLASH
    // 擦除扇区
    int ret = hal_storage_erase(RETAINED_FLASH_OFFSET, sizeof(retained_state_t));
    if (ret != 0) return -1;
    
    // 写入数据
    ret = hal_storage_write(RETAINED_FLASH_OFFSET, state, sizeof(retained_state_t));
    if (ret != 0) return -2;
    
    return 0;
#else
    return -1;
#endif
}

/*============================================================================
 * 公共API
 *============================================================================*/

int retained_init(void)
{
    memset(&cached_state, 0, sizeof(cached_state));
    cache_valid = false;
    
    // 尝试从Flash加载
    int ret = read_from_flash(&cached_state);
    if (ret == 0) {
        cache_valid = true;
    }
    
    return 0;
}

int retained_save(const float quat[4], const float gyro_bias[3])
{
    uint32_t now = hal_get_tick_ms();
    
    // 限制写入频率
    if (cache_valid && (now - last_save_time) < MIN_SAVE_INTERVAL_MS) {
        // 只更新RAM缓存
        memcpy(cached_state.quat, quat, sizeof(float) * 4);
        memcpy(cached_state.gyro_bias, gyro_bias, sizeof(float) * 3);
        cached_state.save_time_ms = now;
        return 0;
    }
    
    // 准备数据
    cached_state.magic = RETAINED_MAGIC;
    cached_state.version = RETAINED_VERSION;
    cached_state.save_time_ms = now;
    
    memcpy(cached_state.quat, quat, sizeof(float) * 4);
    memcpy(cached_state.gyro_bias, gyro_bias, sizeof(float) * 3);
    
    // 计算CRC
    cached_state.crc = calc_crc16(&cached_state, sizeof(retained_state_t) - 2);
    
    // 写入Flash
    int ret = write_to_flash(&cached_state);
    if (ret == 0) {
        cache_valid = true;
        last_save_time = now;
    }
    
    return ret;
}

int retained_restore(float quat[4], float gyro_bias[3])
{
    if (!cache_valid) {
        // 尝试从Flash加载
        int ret = read_from_flash(&cached_state);
        if (ret != 0) {
            return ret;
        }
        cache_valid = true;
    }
    
    // 复制数据
    memcpy(quat, cached_state.quat, sizeof(float) * 4);
    memcpy(gyro_bias, cached_state.gyro_bias, sizeof(float) * 3);
    
    // 检查是否过期
    uint32_t now = hal_get_tick_ms();
    uint32_t age = now - cached_state.save_time_ms;
    
    if (age > RETAINED_VALID_TIMEOUT) {
        return 1;  // 状态过期，但仍返回数据
    }
    
    return 0;
}

bool retained_is_valid(void)
{
    if (!cache_valid) {
        int ret = read_from_flash(&cached_state);
        if (ret == 0) {
            cache_valid = true;
        }
    }
    
    if (!cache_valid) return false;
    
    // 检查时效性
    uint32_t now = hal_get_tick_ms();
    uint32_t age = now - cached_state.save_time_ms;
    
    return (age <= RETAINED_VALID_TIMEOUT);
}

void retained_clear(void)
{
    memset(&cached_state, 0, sizeof(cached_state));
    cache_valid = false;
    
#if RETAINED_USE_FLASH
    hal_storage_erase(RETAINED_FLASH_OFFSET, sizeof(retained_state_t));
#endif
}

int retained_save_fusion_state(const void *state, uint16_t state_size)
{
    // 融合器状态通常较大，存储在单独区域
    uint32_t offset = RETAINED_FLASH_OFFSET + sizeof(retained_state_t);
    
#if RETAINED_USE_FLASH
    // 擦除
    int ret = hal_storage_erase(offset, state_size);
    if (ret != 0) return -1;
    
    // 写入
    ret = hal_storage_write(offset, state, state_size);
    if (ret != 0) return -2;
    
    return 0;
#else
    (void)state;
    (void)state_size;
    return -1;
#endif
}

int retained_restore_fusion_state(void *state, uint16_t state_size)
{
    uint32_t offset = RETAINED_FLASH_OFFSET + sizeof(retained_state_t);
    
#if RETAINED_USE_FLASH
    int ret = hal_storage_read(offset, state, state_size);
    return ret;
#else
    (void)state;
    (void)state_size;
    return -1;
#endif
}

void retained_increment_sleep_count(void)
{
    if (!cache_valid) {
        retained_init();
    }
    cached_state.sleep_count++;
}

void retained_increment_wake_count(void)
{
    if (!cache_valid) {
        retained_init();
    }
    cached_state.wake_count++;
}

void retained_get_stats(uint32_t *sleep_count, uint32_t *wake_count, 
                        uint32_t *runtime_ms)
{
    if (!cache_valid) {
        retained_init();
    }
    
    if (sleep_count) *sleep_count = cached_state.sleep_count;
    if (wake_count) *wake_count = cached_state.wake_count;
    if (runtime_ms) *runtime_ms = cached_state.total_runtime_ms;
}
