/**
 * @file rf_recovery.h
 * @brief v0.5.1 RF自愈闭环模块
 * 
 * 功能:
 * - miss_sync分级恢复
 * - slot越界检测与abort
 * - 超时分级处理
 */

#ifndef RF_RECOVERY_H
#define RF_RECOVERY_H

#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * 配置
 *============================================================================*/

// miss_sync恢复分级
#define MISS_SYNC_LEVEL1_THRESHOLD  3       // 连续丢失3帧: 重同步当前信道
#define MISS_SYNC_LEVEL2_THRESHOLD  10      // 连续丢失10帧: 切换信道
#define MISS_SYNC_LEVEL3_THRESHOLD  30      // 连续丢失30帧: 全信道扫描
#define MISS_SYNC_LEVEL4_THRESHOLD  100     // 连续丢失100帧: 进入深度搜索

// slot时序保护
#define SLOT_GUARD_US               50      // slot保护时间
#define SLOT_ABORT_THRESHOLD        3       // 连续3次越界触发abort

// 超时分级
#define TIMEOUT_LEVEL1_MS           10      // 10ms: 软超时，继续等待
#define TIMEOUT_LEVEL2_MS           50      // 50ms: 中超时，重试
#define TIMEOUT_LEVEL3_MS           100     // 100ms: 硬超时，复位RF
#define TIMEOUT_LEVEL4_MS           500     // 500ms: 严重超时，重新配对

/*============================================================================
 * 恢复状态
 *============================================================================*/

typedef enum {
    RECOVERY_IDLE = 0,
    RECOVERY_RESYNC,            // 重同步
    RECOVERY_CHANNEL_SWITCH,    // 切换信道
    RECOVERY_FULL_SCAN,         // 全信道扫描
    RECOVERY_DEEP_SEARCH,       // 深度搜索
    RECOVERY_ABORT,             // 中止当前操作
} recovery_action_t;

typedef struct {
    // 计数器
    uint32_t miss_sync_count;
    uint32_t consecutive_miss;
    uint32_t slot_overrun_count;
    uint32_t timeout_count;
    
    // 分级统计
    uint32_t level1_recoveries;
    uint32_t level2_recoveries;
    uint32_t level3_recoveries;
    uint32_t level4_recoveries;
    
    // 状态
    recovery_action_t current_action;
    uint8_t current_channel;
    uint32_t action_start_ms;
    uint32_t last_sync_ms;
    
    // slot监控
    uint32_t slot_start_us;
    uint8_t slot_overrun_consecutive;
} rf_recovery_state_t;

/*============================================================================
 * API
 *============================================================================*/

/**
 * @brief 初始化恢复模块
 */
void rf_recovery_init(rf_recovery_state_t *state);

/**
 * @brief 报告sync丢失
 * @return 建议的恢复动作
 */
recovery_action_t rf_recovery_report_miss_sync(rf_recovery_state_t *state);

/**
 * @brief 报告sync成功
 */
void rf_recovery_report_sync_ok(rf_recovery_state_t *state);

/**
 * @brief 检查slot是否越界
 * @param slot_id slot编号
 * @param elapsed_us 已用时间
 * @return true 越界
 */
bool rf_recovery_check_slot_overrun(rf_recovery_state_t *state, 
                                     uint8_t slot_id, 
                                     uint32_t elapsed_us);

/**
 * @brief 报告slot开始
 */
void rf_recovery_slot_start(rf_recovery_state_t *state, uint8_t slot_id);

/**
 * @brief 报告slot结束
 * @return true slot正常完成
 */
bool rf_recovery_slot_end(rf_recovery_state_t *state, uint8_t slot_id);

/**
 * @brief 检查超时等级
 * @param wait_ms 等待时间
 * @return 超时等级 (0=无超时, 1-4=各级超时)
 */
uint8_t rf_recovery_check_timeout(uint32_t wait_ms);

/**
 * @brief 执行恢复动作
 * @param action 恢复动作
 * @return 0成功，负值失败
 */
int rf_recovery_execute(rf_recovery_state_t *state, recovery_action_t action);

/**
 * @brief 获取统计信息
 */
void rf_recovery_get_stats(rf_recovery_state_t *state,
                           uint32_t *miss_sync_count,
                           uint32_t *slot_overrun_count,
                           uint32_t *timeout_count);

/**
 * @brief 重置计数器
 */
void rf_recovery_reset_counters(rf_recovery_state_t *state);

#endif // RF_RECOVERY_H
