/**
 * @file diagnostics.h
 * @brief v0.5.0 诊断统计模块
 * 
 * 用于现场诊断和性能监控:
 * - 丢包统计 (per-tracker)
 * - 重传次数
 * - RSSI分布
 * - 帧率统计
 * - 功耗估算
 */

#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "rf_protocol.h"  // 包含RF_CHANNEL_COUNT定义

/*============================================================================
 * 统计结构
 *============================================================================*/

/**
 * @brief 单个Tracker的统计信息
 */
typedef struct {
    // 基本计数
    uint32_t total_packets;         // 总接收包数
    uint32_t lost_packets;          // 丢失包数
    uint32_t retransmit_count;      // 重传次数
    uint32_t crc_errors;            // CRC错误次数
    uint32_t timeout_count;         // 超时次数
    
    // RSSI统计
    int8_t rssi_min;
    int8_t rssi_max;
    int32_t rssi_sum;               // 用于计算平均值
    uint32_t rssi_samples;
    
    // 时序统计
    uint32_t late_packets;          // 迟到包数
    uint32_t early_packets;         // 早到包数
    uint32_t jitter_sum_us;         // 抖动累积
    
    // 会话信息
    uint32_t connect_time_ms;       // 连接时间
    uint32_t disconnect_count;      // 断连次数
    uint32_t last_seen_ms;          // 最后活跃时间
} tracker_stats_t;

/**
 * @brief Receiver全局统计
 */
typedef struct {
    // 帧统计
    uint32_t superframe_count;      // 超帧总数
    uint32_t beacon_sent;           // 发送的beacon数
    uint32_t data_received;         // 接收的数据包数
    
    // 时序
    uint32_t frame_overrun;         // 帧超时次数
    uint32_t frame_underrun;        // 帧提前完成次数
    uint32_t avg_frame_time_us;     // 平均帧时间
    
    // 信道质量
    uint8_t channel_errors[RF_CHANNEL_COUNT];
    uint8_t current_channel;
    
    // 系统
    uint32_t uptime_sec;            // 运行时间
    uint32_t usb_tx_count;          // USB发送次数
    uint32_t usb_errors;            // USB错误次数
} receiver_stats_t;

/**
 * @brief Tracker端统计
 */
typedef struct {
    // 发送统计
    uint32_t tx_count;              // 发送次数
    uint32_t tx_ack_success;        // ACK成功次数
    uint32_t tx_ack_timeout;        // ACK超时次数
    uint32_t tx_retry;              // 重试次数
    
    // 同步统计
    uint32_t sync_received;         // 收到的sync beacon
    uint32_t sync_lost;             // 丢失的sync次数
    uint32_t resync_count;          // 重同步次数
    
    // 功耗估算
    uint32_t active_time_ms;        // 活跃时间
    uint32_t sleep_time_ms;         // 睡眠时间
    uint32_t radio_on_time_ms;      // 射频开启时间
    
    // 传感器
    uint32_t imu_samples;           // IMU采样数
    uint32_t imu_errors;            // IMU错误数
    uint32_t fusion_updates;        // 融合更新数
} tracker_tx_stats_t;

/*============================================================================
 * 全局实例
 *============================================================================*/

extern tracker_stats_t g_tracker_stats[MAX_TRACKERS];
extern receiver_stats_t g_receiver_stats;
extern tracker_tx_stats_t g_tx_stats;

/*============================================================================
 * API
 *============================================================================*/

/**
 * @brief 初始化诊断模块
 */
void diag_init(void);

/**
 * @brief 重置所有统计
 */
void diag_reset(void);

/**
 * @brief 更新tracker丢包统计
 */
void diag_update_packet_loss(uint8_t tracker_id, uint8_t expected_seq, uint8_t actual_seq);

/**
 * @brief 记录RSSI采样
 */
void diag_record_rssi(uint8_t tracker_id, int8_t rssi);

/**
 * @brief 记录CRC错误
 */
void diag_record_crc_error(uint8_t tracker_id);

/**
 * @brief 记录超时
 */
void diag_record_timeout(uint8_t tracker_id);

/**
 * @brief 记录重传
 */
void diag_record_retransmit(uint8_t tracker_id);

/**
 * @brief 获取丢包率 (百分比*10)
 */
uint16_t diag_get_loss_rate(uint8_t tracker_id);

/**
 * @brief 获取平均RSSI
 */
int8_t diag_get_avg_rssi(uint8_t tracker_id);

/**
 * @brief 生成诊断报告到缓冲区
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 实际写入字节数
 */
uint16_t diag_generate_report(uint8_t *buf, uint16_t buf_size);

/**
 * @brief 周期性更新 (每秒调用一次)
 */
void diag_periodic_update(void);

#endif // DIAGNOSTICS_H
