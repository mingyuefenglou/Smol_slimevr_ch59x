/**
 * @file rf_timing_opt.c
 * @brief RF 时隙优化模块
 * 
 * 目标: RF 延迟 < 5ms
 * 
 * 优化策略:
 * 1. 精确时隙同步 (误差 < 100us)
 * 2. 时钟漂移补偿
 * 3. 自适应时隙调整
 * 4. 提前唤醒
 */

#include "hal.h"
#include "rf_hw.h"
#include <string.h>

/*============================================================================
 * 时序参数 (优化后)
 *============================================================================*/

#define SUPERFRAME_US           10000   // 超帧 10ms (100Hz)
#define SYNC_SLOT_US            500     // 同步时隙 500us
#define TRACKER_SLOT_US         500     // Tracker 时隙 500us
#define ACK_WAIT_US             200     // ACK 等待 200us
#define GUARD_US                50      // 保护间隔 50us
#define WAKEUP_ADVANCE_US       100     // 提前唤醒 100us
#define CHANNEL_SWITCH_US       50      // 信道切换 50us

/*============================================================================
 * 时序状态
 *============================================================================*/

typedef struct {
    // 同步信息
    uint32_t sync_time_us;          // 同步时间点
    uint32_t last_sync_us;          // 上次同步
    uint16_t frame_number;          // 帧号
    
    // 时隙信息
    uint8_t my_slot;                // 我的时隙
    uint8_t total_slots;            // 总时隙数
    
    // 时钟补偿
    int32_t clock_drift_ppb;        // 时钟漂移 (ppb)
    int32_t drift_accumulator;      // 累积漂移
    int16_t slot_offset_us;         // 时隙微调
    
    // 自适应参数
    uint8_t early_count;            // 早到计数
    uint8_t late_count;             // 晚到计数
    
    // 统计
    uint32_t sync_count;            // 同步次数
    uint32_t miss_count;            // 丢失次数
    uint32_t collision_count;       // 冲突次数
    uint32_t avg_latency_us;        // 平均延迟
    uint32_t max_latency_us;        // 最大延迟
} rf_timing_t;

static rf_timing_t rf_timing = {0};

/*============================================================================
 * 初始化
 *============================================================================*/

void rf_timing_init(void)
{
    memset(&rf_timing, 0, sizeof(rf_timing));
    rf_timing.slot_offset_us = WAKEUP_ADVANCE_US;
}

/*============================================================================
 * 时钟漂移补偿
 *============================================================================*/

static void update_clock_drift(uint32_t expected_us, uint32_t actual_us)
{
    int32_t error = (int32_t)(actual_us - expected_us);
    
    // 限制误差范围
    if (error > 5000 || error < -5000) {
        return;  // 跳过异常值
    }
    
    // IIR 滤波更新漂移估计
    // drift_ppb = error_us / frame_period_us * 1000000
    int32_t instant_drift = (error * 100);  // 简化计算
    rf_timing.clock_drift_ppb = (rf_timing.clock_drift_ppb * 15 + instant_drift) / 16;
    
    // 限制范围 (±500ppm)
    if (rf_timing.clock_drift_ppb > 500000) rf_timing.clock_drift_ppb = 500000;
    if (rf_timing.clock_drift_ppb < -500000) rf_timing.clock_drift_ppb = -500000;
}

/*============================================================================
 * 计算补偿后的时隙时间
 *============================================================================*/

uint32_t rf_timing_get_slot_time(void)
{
    uint32_t now_us = hal_micros();
    
    // 基础时隙时间
    uint32_t slot_start = rf_timing.sync_time_us + 
                          SYNC_SLOT_US + 
                          (rf_timing.my_slot * TRACKER_SLOT_US);
    
    // 时钟漂移补偿
    uint32_t elapsed = now_us - rf_timing.sync_time_us;
    int32_t drift_comp = (int32_t)((int64_t)elapsed * rf_timing.clock_drift_ppb / 1000000);
    slot_start += drift_comp;
    
    // 自适应微调
    slot_start -= rf_timing.slot_offset_us;
    
    // 如果已经过了，计算下一帧
    while (slot_start < now_us + WAKEUP_ADVANCE_US) {
        slot_start += SUPERFRAME_US;
    }
    
    return slot_start;
}

/*============================================================================
 * 同步信标接收
 *============================================================================*/

void rf_timing_on_sync(uint32_t rx_time_us, uint16_t frame_num, uint8_t channel)
{
    rf_timing.sync_count++;
    
    // 计算期望时间
    uint32_t expected = rf_timing.sync_time_us + SUPERFRAME_US;
    
    // 更新时钟漂移 (第二次同步后)
    if (rf_timing.sync_count > 1) {
        update_clock_drift(expected, rx_time_us);
    }
    
    // 更新同步时间
    rf_timing.last_sync_us = rf_timing.sync_time_us;
    rf_timing.sync_time_us = rx_time_us;
    rf_timing.frame_number = frame_num;
    
    // 重置累积漂移
    rf_timing.drift_accumulator = 0;
}

/*============================================================================
 * 时隙结果反馈
 *============================================================================*/

void rf_timing_slot_feedback(bool success, int32_t offset_us)
{
    if (success) {
        // 计算延迟
        uint32_t latency = (offset_us > 0) ? offset_us : -offset_us;
        
        rf_timing.avg_latency_us = (rf_timing.avg_latency_us * 7 + latency) / 8;
        
        if (latency > rf_timing.max_latency_us) {
            rf_timing.max_latency_us = latency;
        }
        
        // 自适应调整
        if (offset_us > 100) {
            // 太晚，下次提前
            rf_timing.late_count++;
            if (rf_timing.late_count > 3) {
                rf_timing.slot_offset_us += 20;
                rf_timing.late_count = 0;
            }
        } else if (offset_us < -100) {
            // 太早，下次延后
            rf_timing.early_count++;
            if (rf_timing.early_count > 3) {
                if (rf_timing.slot_offset_us > 20) {
                    rf_timing.slot_offset_us -= 20;
                }
                rf_timing.early_count = 0;
            }
        } else {
            // 正常范围
            rf_timing.early_count = 0;
            rf_timing.late_count = 0;
        }
        
        // 限制偏移范围
        if (rf_timing.slot_offset_us > 500) rf_timing.slot_offset_us = 500;
        if (rf_timing.slot_offset_us < 0) rf_timing.slot_offset_us = 0;
    } else {
        rf_timing.miss_count++;
    }
}

/*============================================================================
 * 时隙配置
 *============================================================================*/

void rf_timing_set_slot(uint8_t slot, uint8_t total)
{
    rf_timing.my_slot = slot;
    rf_timing.total_slots = total;
}

/*============================================================================
 * 等待下一时隙
 *============================================================================*/

void rf_timing_wait_slot(void)
{
    uint32_t target = rf_timing_get_slot_time();
    uint32_t now = hal_micros();
    
    while (now < target) {
        // 长等待使用睡眠
        if (target - now > 1000) {
            hal_delay_us(100);
        } else {
            // 短等待忙等
            __asm volatile ("nop");
        }
        now = hal_micros();
    }
}

/*============================================================================
 * 信道预切换
 *============================================================================*/

void rf_timing_prepare_channel(uint8_t channel)
{
    rf_hw_set_channel(channel);
    hal_delay_us(CHANNEL_SWITCH_US);
}

/*============================================================================
 * 状态查询
 *============================================================================*/

void rf_timing_get_stats(uint32_t *sync, uint32_t *miss, 
                         uint32_t *avg_latency, uint32_t *max_latency,
                         int32_t *drift_ppb)
{
    if (sync) *sync = rf_timing.sync_count;
    if (miss) *miss = rf_timing.miss_count;
    if (avg_latency) *avg_latency = rf_timing.avg_latency_us;
    if (max_latency) *max_latency = rf_timing.max_latency_us;
    if (drift_ppb) *drift_ppb = rf_timing.clock_drift_ppb;
}

bool rf_timing_is_synced(void)
{
    uint32_t now = hal_micros();
    // 500ms 内有同步
    return (now - rf_timing.sync_time_us) < 500000;
}
