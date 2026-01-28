/**
 * @file diagnostics.c
 * @brief v0.5.0 诊断统计模块实现
 */

#include "diagnostics.h"
#include "hal.h"
#include <string.h>
#include <stdio.h>

/*============================================================================
 * 全局实例
 *============================================================================*/

tracker_stats_t g_tracker_stats[MAX_TRACKERS];
receiver_stats_t g_receiver_stats;
tracker_tx_stats_t g_tx_stats;

// 启动时间
static uint32_t startup_time_ms = 0;

/*============================================================================
 * 初始化
 *============================================================================*/

void diag_init(void)
{
    memset(g_tracker_stats, 0, sizeof(g_tracker_stats));
    memset(&g_receiver_stats, 0, sizeof(g_receiver_stats));
    memset(&g_tx_stats, 0, sizeof(g_tx_stats));
    
    // 初始化RSSI范围
    for (int i = 0; i < MAX_TRACKERS; i++) {
        g_tracker_stats[i].rssi_min = 0;
        g_tracker_stats[i].rssi_max = -127;
    }
    
    startup_time_ms = hal_get_tick_ms();
}

void diag_reset(void)
{
    diag_init();
}

/*============================================================================
 * Tracker统计更新
 *============================================================================*/

void diag_update_packet_loss(uint8_t tracker_id, uint8_t expected_seq, uint8_t actual_seq)
{
    if (tracker_id >= MAX_TRACKERS) return;
    
    tracker_stats_t *stats = &g_tracker_stats[tracker_id];
    stats->total_packets++;
    
    if (actual_seq != expected_seq) {
        // 计算丢失的包数 (处理序列号回绕)
        uint8_t lost = (uint8_t)(actual_seq - expected_seq);
        if (lost > 128) lost = 1;  // 回绕情况
        stats->lost_packets += lost;
    }
    
    stats->last_seen_ms = hal_get_tick_ms();
}

void diag_record_rssi(uint8_t tracker_id, int8_t rssi)
{
    if (tracker_id >= MAX_TRACKERS) return;
    
    tracker_stats_t *stats = &g_tracker_stats[tracker_id];
    
    if (rssi < stats->rssi_min) stats->rssi_min = rssi;
    if (rssi > stats->rssi_max) stats->rssi_max = rssi;
    
    stats->rssi_sum += rssi;
    stats->rssi_samples++;
}

void diag_record_crc_error(uint8_t tracker_id)
{
    if (tracker_id >= MAX_TRACKERS) return;
    g_tracker_stats[tracker_id].crc_errors++;
}

void diag_record_timeout(uint8_t tracker_id)
{
    if (tracker_id >= MAX_TRACKERS) return;
    g_tracker_stats[tracker_id].timeout_count++;
}

void diag_record_retransmit(uint8_t tracker_id)
{
    if (tracker_id >= MAX_TRACKERS) return;
    g_tracker_stats[tracker_id].retransmit_count++;
}

/*============================================================================
 * 统计查询
 *============================================================================*/

uint16_t diag_get_loss_rate(uint8_t tracker_id)
{
    if (tracker_id >= MAX_TRACKERS) return 0;
    
    tracker_stats_t *stats = &g_tracker_stats[tracker_id];
    if (stats->total_packets == 0) return 0;
    
    // 返回丢包率 * 10 (精度0.1%)
    return (uint16_t)((stats->lost_packets * 1000) / stats->total_packets);
}

int8_t diag_get_avg_rssi(uint8_t tracker_id)
{
    if (tracker_id >= MAX_TRACKERS) return -127;
    
    tracker_stats_t *stats = &g_tracker_stats[tracker_id];
    if (stats->rssi_samples == 0) return -127;
    
    return (int8_t)(stats->rssi_sum / (int32_t)stats->rssi_samples);
}

/*============================================================================
 * 诊断报告生成
 *============================================================================*/

uint16_t diag_generate_report(uint8_t *buf, uint16_t buf_size)
{
    if (buf == NULL || buf_size < 64) return 0;
    
    uint16_t pos = 0;
    
    // 报告头
    buf[pos++] = 0xD1;  // 诊断报告标识
    buf[pos++] = 0xA0;  // 修复: 0xAG不是有效的十六进制
    buf[pos++] = 0x01;  // 版本
    buf[pos++] = 0x00;
    
    // 运行时间 (秒)
    uint32_t uptime = (hal_get_tick_ms() - startup_time_ms) / 1000;
    buf[pos++] = (uptime >> 0) & 0xFF;
    buf[pos++] = (uptime >> 8) & 0xFF;
    buf[pos++] = (uptime >> 16) & 0xFF;
    buf[pos++] = (uptime >> 24) & 0xFF;
    
    // Receiver统计
    buf[pos++] = (g_receiver_stats.superframe_count >> 0) & 0xFF;
    buf[pos++] = (g_receiver_stats.superframe_count >> 8) & 0xFF;
    buf[pos++] = (g_receiver_stats.superframe_count >> 16) & 0xFF;
    buf[pos++] = (g_receiver_stats.superframe_count >> 24) & 0xFF;
    
    buf[pos++] = (g_receiver_stats.data_received >> 0) & 0xFF;
    buf[pos++] = (g_receiver_stats.data_received >> 8) & 0xFF;
    buf[pos++] = (g_receiver_stats.data_received >> 16) & 0xFF;
    buf[pos++] = (g_receiver_stats.data_received >> 24) & 0xFF;
    
    // Tracker统计 (每个tracker 8字节)
    uint8_t tracker_count = 0;
    for (int i = 0; i < MAX_TRACKERS && pos + 10 < buf_size; i++) {
        tracker_stats_t *stats = &g_tracker_stats[i];
        if (stats->total_packets == 0) continue;
        
        buf[pos++] = i;  // Tracker ID
        buf[pos++] = diag_get_loss_rate(i) & 0xFF;  // 丢包率低字节
        buf[pos++] = (diag_get_loss_rate(i) >> 8) & 0xFF;  // 丢包率高字节
        buf[pos++] = (uint8_t)(diag_get_avg_rssi(i) + 128);  // RSSI (偏移128)
        buf[pos++] = (stats->total_packets >> 0) & 0xFF;
        buf[pos++] = (stats->total_packets >> 8) & 0xFF;
        buf[pos++] = (stats->crc_errors >> 0) & 0xFF;
        buf[pos++] = (stats->retransmit_count >> 0) & 0xFF;
        
        tracker_count++;
    }
    
    // 更新tracker数量
    buf[3] = tracker_count;
    
    return pos;
}

/*============================================================================
 * 周期更新
 *============================================================================*/

void diag_periodic_update(void)
{
    uint32_t now = hal_get_tick_ms();
    g_receiver_stats.uptime_sec = (now - startup_time_ms) / 1000;
    
    // 检查tracker离线
    for (int i = 0; i < MAX_TRACKERS; i++) {
        tracker_stats_t *stats = &g_tracker_stats[i];
        if (stats->total_packets > 0 && stats->last_seen_ms > 0) {
            uint32_t idle_time = now - stats->last_seen_ms;
            if (idle_time > 1000) {  // 1秒无数据
                stats->disconnect_count++;
            }
        }
    }
}
