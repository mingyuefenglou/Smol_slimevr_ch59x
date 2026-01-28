/**
 * @file rf_recovery.c
 * @brief v0.5.1 RF自愈闭环实现
 */

#include "rf_recovery.h"
#include "rf_hw.h"
#include "rf_protocol.h"
#include "hal.h"
#include "event_logger.h"
#include "channel_manager.h"
#include <string.h>

/*============================================================================
 * 初始化
 *============================================================================*/

void rf_recovery_init(rf_recovery_state_t *state)
{
    if (!state) return;
    
    memset(state, 0, sizeof(rf_recovery_state_t));
    state->current_action = RECOVERY_IDLE;
    state->last_sync_ms = hal_get_tick_ms();
}

/*============================================================================
 * Sync恢复
 *============================================================================*/

recovery_action_t rf_recovery_report_miss_sync(rf_recovery_state_t *state)
{
    if (!state) return RECOVERY_IDLE;
    
    state->miss_sync_count++;
    state->consecutive_miss++;
    
    recovery_action_t action = RECOVERY_IDLE;
    
    // 分级判定
    if (state->consecutive_miss >= MISS_SYNC_LEVEL4_THRESHOLD) {
        action = RECOVERY_DEEP_SEARCH;
        state->level4_recoveries++;
        event_log_u32(EVT_RF_SYNC_LOST, state->consecutive_miss);
    } else if (state->consecutive_miss >= MISS_SYNC_LEVEL3_THRESHOLD) {
        action = RECOVERY_FULL_SCAN;
        state->level3_recoveries++;
    } else if (state->consecutive_miss >= MISS_SYNC_LEVEL2_THRESHOLD) {
        action = RECOVERY_CHANNEL_SWITCH;
        state->level2_recoveries++;
    } else if (state->consecutive_miss >= MISS_SYNC_LEVEL1_THRESHOLD) {
        action = RECOVERY_RESYNC;
        state->level1_recoveries++;
    }
    
    if (action != RECOVERY_IDLE) {
        state->current_action = action;
        state->action_start_ms = hal_get_tick_ms();
    }
    
    return action;
}

void rf_recovery_report_sync_ok(rf_recovery_state_t *state)
{
    if (!state) return;
    
    // 如果之前在恢复状态，记录恢复成功
    if (state->consecutive_miss > 0) {
        event_log_u32(EVT_RF_SYNC_FOUND, state->consecutive_miss);
    }
    
    state->consecutive_miss = 0;
    state->current_action = RECOVERY_IDLE;
    state->last_sync_ms = hal_get_tick_ms();
}

/*============================================================================
 * Slot监控
 *============================================================================*/

void rf_recovery_slot_start(rf_recovery_state_t *state, uint8_t slot_id)
{
    if (!state) return;
    state->slot_start_us = hal_get_tick_us();
    (void)slot_id;
}

bool rf_recovery_slot_end(rf_recovery_state_t *state, uint8_t slot_id)
{
    if (!state) return true;
    
    uint32_t elapsed = hal_get_tick_us() - state->slot_start_us;
    uint32_t slot_limit = RF_DATA_SLOT_US - SLOT_GUARD_US;
    
    if (elapsed > slot_limit) {
        // 越界
        state->slot_overrun_count++;
        state->slot_overrun_consecutive++;
        
        if (state->slot_overrun_consecutive >= SLOT_ABORT_THRESHOLD) {
            // 触发abort
            state->current_action = RECOVERY_ABORT;
            event_log_u8(EVT_RF_TIMEOUT, slot_id);
            state->slot_overrun_consecutive = 0;
            return false;
        }
    } else {
        state->slot_overrun_consecutive = 0;
    }
    
    return true;
}

bool rf_recovery_check_slot_overrun(rf_recovery_state_t *state, 
                                     uint8_t slot_id, 
                                     uint32_t elapsed_us)
{
    if (!state) return false;
    
    uint32_t slot_limit = RF_DATA_SLOT_US - SLOT_GUARD_US;
    
    if (elapsed_us > slot_limit) {
        state->slot_overrun_count++;
        return true;
    }
    
    (void)slot_id;
    return false;
}

/*============================================================================
 * 超时分级
 *============================================================================*/

uint8_t rf_recovery_check_timeout(uint32_t wait_ms)
{
    if (wait_ms >= TIMEOUT_LEVEL4_MS) return 4;
    if (wait_ms >= TIMEOUT_LEVEL3_MS) return 3;
    if (wait_ms >= TIMEOUT_LEVEL2_MS) return 2;
    if (wait_ms >= TIMEOUT_LEVEL1_MS) return 1;
    return 0;
}

/*============================================================================
 * 恢复执行
 *============================================================================*/

int rf_recovery_execute(rf_recovery_state_t *state, recovery_action_t action)
{
    if (!state) return -1;
    
    switch (action) {
        case RECOVERY_IDLE:
            break;
            
        case RECOVERY_RESYNC:
            // 简单重同步：重新开始等待beacon
            rf_hw_set_mode(RF_MODE_RX);
            break;
            
        case RECOVERY_CHANNEL_SWITCH:
            // 切换到下一个信道
            {
                uint8_t next_ch = (state->current_channel + 1) % RF_CHANNEL_COUNT;
                rf_hw_set_channel(next_ch);
                state->current_channel = next_ch;
                event_log_u8(EVT_RF_CHANNEL_SWITCH, next_ch);
            }
            break;
            
        case RECOVERY_FULL_SCAN:
            // 全信道扫描（暂时回到配对信道）
            rf_hw_set_channel(RF_PAIRING_CHANNEL);
            state->current_channel = RF_PAIRING_CHANNEL;
            break;
            
        case RECOVERY_DEEP_SEARCH:
            // 深度搜索：停止发送，进入纯监听模式
            rf_hw_set_mode(RF_MODE_SLEEP);
            hal_delay_ms(100);
            rf_hw_set_channel(RF_PAIRING_CHANNEL);
            rf_hw_set_mode(RF_MODE_RX);
            break;
            
        case RECOVERY_ABORT:
            // 中止当前操作
            rf_hw_flush_tx();
            rf_hw_flush_rx();
            rf_hw_set_mode(RF_MODE_STANDBY);
            break;
            
        default:
            return -2;
    }
    
    return 0;
}

/*============================================================================
 * 统计接口
 *============================================================================*/

void rf_recovery_get_stats(rf_recovery_state_t *state,
                           uint32_t *miss_sync_count,
                           uint32_t *slot_overrun_count,
                           uint32_t *timeout_count)
{
    if (!state) return;
    
    if (miss_sync_count) *miss_sync_count = state->miss_sync_count;
    if (slot_overrun_count) *slot_overrun_count = state->slot_overrun_count;
    if (timeout_count) *timeout_count = state->timeout_count;
}

void rf_recovery_reset_counters(rf_recovery_state_t *state)
{
    if (!state) return;
    
    state->miss_sync_count = 0;
    state->slot_overrun_count = 0;
    state->timeout_count = 0;
    state->level1_recoveries = 0;
    state->level2_recoveries = 0;
    state->level3_recoveries = 0;
    state->level4_recoveries = 0;
}
