/**
 * @file rf_slot_optimizer.h
 * @brief RF 时隙优化模块
 * 
 * 目标: RF 延迟 < 5ms
 * 
 * 优化方案:
 * 1. 动态时隙分配
 * 2. 优先级调度
 * 3. 自适应时隙长度
 * 4. 预传输准备
 * 5. 并行处理
 */

#ifndef __RF_SLOT_OPTIMIZER_H__
#define __RF_SLOT_OPTIMIZER_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 优先级定义
 *============================================================================*/

#define SLOT_PRIORITY_HIGH      3
#define SLOT_PRIORITY_NORMAL    2
#define SLOT_PRIORITY_LOW       1

/*============================================================================
 * 数据结构
 *============================================================================*/

typedef struct {
    uint8_t active_trackers;
    uint16_t avg_latency_us;
    uint16_t max_latency_us;
    uint8_t utilization;        // 0-100%
    uint32_t frame_number;
} slot_optimizer_stats_t;

typedef struct {
    uint8_t tracker_id;
    uint8_t *data;
    uint8_t len;
} batch_item_t;

/*============================================================================
 * API 函数
 *============================================================================*/

/**
 * @brief 初始化时隙优化器
 */
void slot_optimizer_init(void);

/**
 * @brief 注册Tracker
 * @param tracker_id Tracker ID
 * @param priority 优先级 (SLOT_PRIORITY_HIGH/NORMAL/LOW)
 * @return 0 成功
 */
int slot_optimizer_register(uint8_t tracker_id, uint8_t priority);

/**
 * @brief 注销Tracker
 */
void slot_optimizer_unregister(uint8_t tracker_id);

/**
 * @brief 开始新帧
 */
void slot_optimizer_frame_start(void);

/**
 * @brief 获取当前时隙信息
 * @param tracker_id 输出Tracker ID
 * @param duration_us 输出时隙长度
 * @return 0 成功, -1 无可用时隙
 */
int slot_optimizer_get_current(uint8_t *tracker_id, uint16_t *duration_us);

/**
 * @brief 报告传输结果
 * @param tracker_id Tracker ID
 * @param success 是否成功
 * @param latency_us 延迟 (微秒)
 */
void slot_optimizer_report_result(uint8_t tracker_id, bool success, uint32_t latency_us);

/**
 * @brief 检查帧内是否还有时间
 * @return true 如果还有时间
 */
bool slot_optimizer_frame_time_available(void);

/**
 * @brief 获取统计信息
 */
void slot_optimizer_get_stats(slot_optimizer_stats_t *stats);

/**
 * @brief 快速传输 (跳过队列)
 */
int slot_optimizer_fast_transmit(uint8_t tracker_id, const uint8_t *data, uint8_t len);

/**
 * @brief 批量传输
 */
int slot_optimizer_batch_transmit(batch_item_t *items, uint8_t count);

/**
 * @brief 调试输出
 */
void slot_optimizer_dump(void);

#ifdef __cplusplus
}
#endif

#endif /* __RF_SLOT_OPTIMIZER_H__ */
