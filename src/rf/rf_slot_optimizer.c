/**
 * @file rf_slot_optimizer.c
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

#include "rf_protocol.h"
#include "rf_hw.h"
#include "hal.h"
#include <string.h>

/*============================================================================
 * 配置
 *============================================================================*/

#define MAX_TRACKERS            16
#define SLOT_BASE_US            300     // 基础时隙长度 (微秒)
#define SLOT_MIN_US             200     // 最小时隙
#define SLOT_MAX_US             1000    // 最大时隙
#define GUARD_TIME_US           50      // 保护时间
#define FRAME_PERIOD_US         5000    // 帧周期 (5ms = 200Hz)

#define PRIORITY_HIGH           3
#define PRIORITY_NORMAL         2
#define PRIORITY_LOW            1

/*============================================================================
 * 时隙状态
 *============================================================================*/

typedef struct {
    uint8_t tracker_id;
    uint8_t priority;
    uint16_t slot_duration_us;
    uint32_t last_tx_time;
    uint32_t latency_us;
    uint8_t success_rate;       // 0-100
    uint8_t retries;
    bool active;
    bool needs_retx;
} slot_info_t;

typedef struct {
    // 时隙分配
    slot_info_t slots[MAX_TRACKERS];
    uint8_t active_count;
    uint8_t current_slot;
    
    // 帧管理
    uint32_t frame_start_us;
    uint32_t frame_end_us;
    uint32_t frame_number;
    
    // 优化参数
    uint16_t avg_latency_us;
    uint16_t max_latency_us;
    uint8_t utilization;        // 0-100%
    
    // 预传输缓冲
    uint8_t prefetch_buffer[64];
    uint8_t prefetch_slot;
    bool prefetch_ready;
} slot_manager_t;

static slot_manager_t sm = {0};

/*============================================================================
 * 时隙计算
 *============================================================================*/

/**
 * 计算最优时隙长度
 * 
 * 基于: 数据大小 + 传输时间 + 响应时间 + 处理时间
 */
static uint16_t calculate_slot_duration(uint8_t data_size, uint8_t success_rate)
{
    // 基础传输时间 (2Mbps)
    uint16_t tx_time_us = (data_size * 8) / 2;  // bit / Mbps = us
    
    // 响应等待时间
    uint16_t response_us = 150;
    
    // 处理时间
    uint16_t process_us = 50;
    
    // 成功率补偿 (成功率低则增加时隙)
    uint16_t retry_margin = 0;
    if (success_rate < 90) {
        retry_margin = 100;
    } else if (success_rate < 95) {
        retry_margin = 50;
    }
    
    uint16_t total = tx_time_us + response_us + process_us + retry_margin + GUARD_TIME_US;
    
    // 限制范围
    if (total < SLOT_MIN_US) total = SLOT_MIN_US;
    if (total > SLOT_MAX_US) total = SLOT_MAX_US;
    
    return total;
}

/**
 * 动态调整时隙分配
 */
static void optimize_slot_allocation(void)
{
    if (sm.active_count == 0) return;
    
    // 计算总需求时间
    uint32_t total_needed = 0;
    for (int i = 0; i < MAX_TRACKERS; i++) {
        if (sm.slots[i].active) {
            total_needed += sm.slots[i].slot_duration_us;
        }
    }
    
    // 计算利用率
    sm.utilization = (total_needed * 100) / FRAME_PERIOD_US;
    
    // 如果过载，压缩低优先级时隙
    if (sm.utilization > 90) {
        for (int i = 0; i < MAX_TRACKERS; i++) {
            if (sm.slots[i].active && sm.slots[i].priority == PRIORITY_LOW) {
                sm.slots[i].slot_duration_us = (sm.slots[i].slot_duration_us * 80) / 100;
                if (sm.slots[i].slot_duration_us < SLOT_MIN_US) {
                    sm.slots[i].slot_duration_us = SLOT_MIN_US;
                }
            }
        }
    }
    
    // 如果空闲，扩展高优先级时隙
    if (sm.utilization < 70) {
        for (int i = 0; i < MAX_TRACKERS; i++) {
            if (sm.slots[i].active && sm.slots[i].priority == PRIORITY_HIGH) {
                sm.slots[i].slot_duration_us = (sm.slots[i].slot_duration_us * 110) / 100;
                if (sm.slots[i].slot_duration_us > SLOT_MAX_US) {
                    sm.slots[i].slot_duration_us = SLOT_MAX_US;
                }
            }
        }
    }
}

/*============================================================================
 * 优先级调度
 *============================================================================*/

/**
 * 获取下一个应该发送的时隙
 * 
 * 策略:
 * 1. 需要重传的优先
 * 2. 高优先级优先
 * 3. 最久未传输的优先
 */
static int get_next_slot(void)
{
    int best_slot = -1;
    int best_score = -1;
    uint32_t now = hal_get_tick_us();
    
    for (int i = 0; i < MAX_TRACKERS; i++) {
        if (!sm.slots[i].active) continue;
        
        int score = 0;
        
        // 重传加权
        if (sm.slots[i].needs_retx) {
            score += 1000;
        }
        
        // 优先级加权
        score += sm.slots[i].priority * 100;
        
        // 等待时间加权 (最多 500)
        uint32_t wait_time = now - sm.slots[i].last_tx_time;
        score += (wait_time / 100) > 500 ? 500 : (wait_time / 100);
        
        if (score > best_score) {
            best_score = score;
            best_slot = i;
        }
    }
    
    return best_slot;
}

/*============================================================================
 * 预传输准备
 *============================================================================*/

/**
 * 预加载下一帧数据到发送缓冲
 */
static void prefetch_next_packet(void)
{
    int next_slot = get_next_slot();
    if (next_slot < 0) return;
    
    // 准备数据包
    // 此处应调用实际的数据打包函数
    memset(sm.prefetch_buffer, 0, sizeof(sm.prefetch_buffer));
    sm.prefetch_buffer[0] = sm.slots[next_slot].tracker_id;
    
    sm.prefetch_slot = next_slot;
    sm.prefetch_ready = true;
}

/*============================================================================
 * 公共接口
 *============================================================================*/

void slot_optimizer_init(void)
{
    memset(&sm, 0, sizeof(sm));
}

int slot_optimizer_register(uint8_t tracker_id, uint8_t priority)
{
    if (sm.active_count >= MAX_TRACKERS) return -1;
    
    // 查找空闲槽位
    int slot = -1;
    for (int i = 0; i < MAX_TRACKERS; i++) {
        if (!sm.slots[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) return -1;
    
    sm.slots[slot].tracker_id = tracker_id;
    sm.slots[slot].priority = priority;
    sm.slots[slot].slot_duration_us = SLOT_BASE_US;
    sm.slots[slot].success_rate = 100;
    sm.slots[slot].active = true;
    sm.slots[slot].last_tx_time = hal_get_tick_us();
    sm.active_count++;
    
    // 重新优化分配
    optimize_slot_allocation();
    
    return slot;
}

void slot_optimizer_unregister(uint8_t tracker_id)
{
    for (int i = 0; i < MAX_TRACKERS; i++) {
        if (sm.slots[i].active && sm.slots[i].tracker_id == tracker_id) {
            sm.slots[i].active = false;
            sm.active_count--;
            break;
        }
    }
    
    optimize_slot_allocation();
}

/**
 * 帧开始处理
 */
void slot_optimizer_frame_start(void)
{
    sm.frame_start_us = hal_get_tick_us();
    sm.frame_number++;
    sm.current_slot = 0;
    
    // 预加载第一个数据包
    prefetch_next_packet();
}

/**
 * 获取当前时隙信息
 */
int slot_optimizer_get_current(uint8_t *tracker_id, uint16_t *duration_us)
{
    int slot = get_next_slot();
    if (slot < 0) return -1;
    
    if (tracker_id) *tracker_id = sm.slots[slot].tracker_id;
    if (duration_us) *duration_us = sm.slots[slot].slot_duration_us;
    
    return slot;
}

/**
 * 报告传输结果
 */
void slot_optimizer_report_result(uint8_t tracker_id, bool success, uint32_t latency_us)
{
    for (int i = 0; i < MAX_TRACKERS; i++) {
        if (sm.slots[i].active && sm.slots[i].tracker_id == tracker_id) {
            // 更新统计
            if (success) {
                sm.slots[i].success_rate = (sm.slots[i].success_rate * 15 + 100) / 16;
                sm.slots[i].needs_retx = false;
                sm.slots[i].retries = 0;
            } else {
                sm.slots[i].success_rate = (sm.slots[i].success_rate * 15 + 0) / 16;
                sm.slots[i].needs_retx = true;
                sm.slots[i].retries++;
            }
            
            sm.slots[i].latency_us = latency_us;
            sm.slots[i].last_tx_time = hal_get_tick_us();
            
            // 自适应调整时隙
            sm.slots[i].slot_duration_us = calculate_slot_duration(32, sm.slots[i].success_rate);
            
            // 更新全局统计
            if (latency_us > sm.max_latency_us) {
                sm.max_latency_us = latency_us;
            }
            sm.avg_latency_us = (sm.avg_latency_us * 15 + latency_us) / 16;
            
            break;
        }
    }
    
    // 预加载下一帧
    prefetch_next_packet();
}

/**
 * 检查帧时间是否耗尽
 */
bool slot_optimizer_frame_time_available(void)
{
    uint32_t elapsed = hal_get_tick_us() - sm.frame_start_us;
    return elapsed < (FRAME_PERIOD_US - 500);  // 保留 500us 余量
}

/**
 * 获取剩余帧时间
 */
uint32_t slot_optimizer_get_remaining_us(void)
{
    uint32_t elapsed = hal_get_tick_us() - sm.frame_start_us;
    if (elapsed >= FRAME_PERIOD_US) return 0;
    return FRAME_PERIOD_US - elapsed;
}

/*============================================================================
 * 统计接口
 *============================================================================*/

typedef struct {
    uint8_t active_trackers;
    uint16_t avg_latency_us;
    uint16_t max_latency_us;
    uint8_t utilization;
    uint32_t frame_number;
} slot_optimizer_stats_t;

void slot_optimizer_get_stats(slot_optimizer_stats_t *stats)
{
    if (!stats) return;
    
    stats->active_trackers = sm.active_count;
    stats->avg_latency_us = sm.avg_latency_us;
    stats->max_latency_us = sm.max_latency_us;
    stats->utilization = sm.utilization;
    stats->frame_number = sm.frame_number;
}

/*============================================================================
 * 快速通道 (Fast Path)
 *============================================================================*/

/**
 * 快速传输路径 - 最低延迟
 * 
 * 用于单个 Tracker 场景或紧急数据
 */
int slot_optimizer_fast_transmit(uint8_t tracker_id, const uint8_t *data, uint8_t len)
{
    uint32_t start = hal_get_tick_us();
    
    // 直接传输，不经过调度
    int ret = rf_hw_transmit(data, len);
    
    uint32_t latency = hal_get_tick_us() - start;
    
    // 更新统计
    slot_optimizer_report_result(tracker_id, (ret == 0), latency);
    
    return ret;
}

/*============================================================================
 * 批量传输优化
 *============================================================================*/

typedef struct {
    uint8_t tracker_id;
    uint8_t *data;
    uint8_t len;
} batch_item_t;

/**
 * 批量传输多个 Tracker 的数据
 * 
 * 优化: 减少每帧的切换开销
 */
int slot_optimizer_batch_transmit(batch_item_t *items, uint8_t count)
{
    if (!items || count == 0) return -1;
    
    int success = 0;
    uint32_t frame_start = hal_get_tick_us();
    
    // 按优先级排序
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            int pri_i = PRIORITY_NORMAL, pri_j = PRIORITY_NORMAL;
            
            for (int k = 0; k < MAX_TRACKERS; k++) {
                if (sm.slots[k].active) {
                    if (sm.slots[k].tracker_id == items[i].tracker_id) pri_i = sm.slots[k].priority;
                    if (sm.slots[k].tracker_id == items[j].tracker_id) pri_j = sm.slots[k].priority;
                }
            }
            
            if (pri_j > pri_i) {
                batch_item_t tmp = items[i];
                items[i] = items[j];
                items[j] = tmp;
            }
        }
    }
    
    // 顺序传输
    for (int i = 0; i < count; i++) {
        // 检查时间
        if (!slot_optimizer_frame_time_available()) break;
        
        uint32_t tx_start = hal_get_tick_us();
        
        int ret = rf_hw_transmit(items[i].data, items[i].len);
        
        uint32_t latency = hal_get_tick_us() - tx_start;
        slot_optimizer_report_result(items[i].tracker_id, (ret == 0), latency);
        
        if (ret == 0) success++;
    }
    
    return success;
}

/*============================================================================
 * 调试接口
 *============================================================================*/

void slot_optimizer_dump(void)
{
    #ifdef DEBUG_ENABLE
    printf("=== Slot Optimizer Status ===\n");
    printf("Active: %d, Frame: %lu, Util: %d%%\n", 
           sm.active_count, sm.frame_number, sm.utilization);
    printf("Latency: avg=%uus, max=%uus\n", sm.avg_latency_us, sm.max_latency_us);
    
    for (int i = 0; i < MAX_TRACKERS; i++) {
        if (sm.slots[i].active) {
            printf("  Slot[%d]: ID=%d, Pri=%d, Dur=%uus, Rate=%d%%\n",
                   i, sm.slots[i].tracker_id, sm.slots[i].priority,
                   sm.slots[i].slot_duration_us, sm.slots[i].success_rate);
        }
    }
    #endif
}
