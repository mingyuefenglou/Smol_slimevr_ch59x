/**
 * @file retained_state.h
 * @brief Retained State Manager - 唤醒后快速恢复
 * 
 * v0.5.0: 保存和恢复关键状态，实现唤醒后姿态快速稳定
 * 
 * 保存内容:
 * - Gyro bias (零偏)
 * - 融合器状态 (VQF internal state)
 * - 校准参数
 * - 最后已知姿态
 * 
 * 存储位置: Flash或SRAM (根据配置)
 */

#ifndef RETAINED_STATE_H
#define RETAINED_STATE_H

#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * 配置
 *============================================================================*/

// 存储位置选择
#define RETAINED_USE_FLASH      1   // 使用Flash存储 (掉电保持)
#define RETAINED_USE_SRAM       0   // 使用SRAM存储 (更快但掉电丢失)

// 存储偏移地址 (Flash模式)
#define RETAINED_FLASH_OFFSET   0x0400  // 在hal_storage中的偏移

// 状态有效性超时 (ms)
#define RETAINED_VALID_TIMEOUT  60000   // 60秒内的状态仍然有效

/*============================================================================
 * 状态结构
 *============================================================================*/

/**
 * @brief 保留状态数据结构
 */
typedef struct {
    // 魔数和版本
    uint32_t magic;             // 0x52455441 "RETA"
    uint8_t version;            // 结构版本
    uint8_t reserved[3];
    
    // 时间戳
    uint32_t save_time_ms;      // 保存时间
    
    // Gyro偏置
    float gyro_bias[3];
    
    // 融合器状态 (VQF核心状态)
    float quat[4];              // 最后已知姿态
    float gyro_bias_est[3];     // 融合器估计的偏置
    float accel_ref[3];         // 加速度参考
    
    // 校准状态
    bool calibration_valid;
    float accel_offset[3];
    float accel_scale[3];
    
    // 统计信息
    uint32_t total_runtime_ms;  // 累计运行时间
    uint32_t sleep_count;       // 进入睡眠次数
    uint32_t wake_count;        // 唤醒次数
    
    // CRC校验
    uint16_t crc;
} __attribute__((packed)) retained_state_t;

/*============================================================================
 * API
 *============================================================================*/

/**
 * @brief 初始化retained state模块
 * @return 0成功，负值失败
 */
int retained_init(void);

/**
 * @brief 保存当前状态
 * @param quat 当前四元数
 * @param gyro_bias 当前陀螺仪偏置
 * @return 0成功，负值失败
 */
int retained_save(const float quat[4], const float gyro_bias[3]);

/**
 * @brief 恢复保存的状态
 * @param quat 输出四元数
 * @param gyro_bias 输出陀螺仪偏置
 * @return 0成功且状态有效，1成功但状态过期，负值失败/无效
 */
int retained_restore(float quat[4], float gyro_bias[3]);

/**
 * @brief 检查是否有有效的保存状态
 * @return true有有效状态
 */
bool retained_is_valid(void);

/**
 * @brief 清除保存的状态
 */
void retained_clear(void);

/**
 * @brief 保存完整融合器状态
 * @param state 融合器状态指针
 * @param state_size 状态大小
 * @return 0成功，负值失败
 */
int retained_save_fusion_state(const void *state, uint16_t state_size);

/**
 * @brief 恢复完整融合器状态
 * @param state 融合器状态指针
 * @param state_size 状态大小
 * @return 0成功，负值失败
 */
int retained_restore_fusion_state(void *state, uint16_t state_size);

/**
 * @brief 增加睡眠计数
 */
void retained_increment_sleep_count(void);

/**
 * @brief 增加唤醒计数
 */
void retained_increment_wake_count(void);

/**
 * @brief 获取统计信息
 * @param sleep_count 睡眠次数
 * @param wake_count 唤醒次数
 * @param runtime_ms 运行时间
 */
void retained_get_stats(uint32_t *sleep_count, uint32_t *wake_count, 
                        uint32_t *runtime_ms);

#endif // RETAINED_STATE_H
