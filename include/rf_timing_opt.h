/**
 * @file rf_timing_opt.h
 * @brief RF 时序优化模块
 * 
 * 功能:
 * 1. 精确时序同步
 * 2. 预测性信道切换
 * 3. 时隙碰撞检测
 * 4. 自适应延迟补偿
 */

#ifndef __RF_TIMING_OPT_H__
#define __RF_TIMING_OPT_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * API 函数
 *============================================================================*/

/**
 * @brief 初始化时序优化模块
 */
void rf_timing_init(void);

/**
 * @brief 同步事件回调
 * @param rx_time_us 接收时间
 * @param frame_num 帧号
 * @param channel 信道
 */
void rf_timing_on_sync(uint32_t rx_time_us, uint16_t frame_num, uint8_t channel);

/**
 * @brief 时隙反馈
 * @param success 是否成功
 * @param offset_us 时间偏移
 */
void rf_timing_slot_feedback(bool success, int32_t offset_us);

/**
 * @brief 设置时隙位置
 * @param slot 当前时隙号
 * @param total 总时隙数
 */
void rf_timing_set_slot(uint8_t slot, uint8_t total);

/**
 * @brief 等待时隙开始
 */
void rf_timing_wait_slot(void);

/**
 * @brief 预备信道切换
 * @param channel 目标信道
 */
void rf_timing_prepare_channel(uint8_t channel);

/**
 * @brief 获取统计信息
 */
void rf_timing_get_stats(uint32_t *sync, uint32_t *miss, 
                         uint32_t *avg_latency, uint32_t *max_latency);

/**
 * @brief 检查是否已同步
 */
bool rf_timing_is_synced(void);

#ifdef __cplusplus
}
#endif

#endif /* __RF_TIMING_OPT_H__ */
