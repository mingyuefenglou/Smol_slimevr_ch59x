/**
 * @file channel_manager.h
 * @brief v0.6.0 动态信道黑名单与自适应跳频
 * 
 * 功能:
 * - 信道质量监控 (丢包率/RSSI)
 * - 动态黑名单 (自动禁用差信道)
 * - 自适应跳频序列
 */

#ifndef CHANNEL_MANAGER_H
#define CHANNEL_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "rf_protocol.h"

/*============================================================================
 * 配置
 *============================================================================*/

#define CH_QUALITY_WINDOW_SEC   10      // 质量统计窗口 (秒)
#define CH_BLACKLIST_THRESHOLD  30      // 丢包率>30%时黑名单
#define CH_RECOVERY_THRESHOLD   10      // 丢包率<10%时恢复
#define CH_MIN_ACTIVE_CHANNELS  3       // 最少保留3个活跃信道
#define CH_QUALITY_UPDATE_MS    1000    // 质量更新周期

/*============================================================================
 * 信道质量结构
 *============================================================================*/

typedef struct {
    // 10秒窗口统计
    uint16_t tx_count;          // 发送次数
    uint16_t ack_count;         // ACK成功次数
    uint16_t crc_errors;        // CRC错误次数
    int16_t rssi_sum;           // RSSI累积
    uint16_t rssi_samples;      // RSSI采样数
    
    // 计算值
    uint8_t loss_rate_pct;      // 丢包率百分比
    int8_t avg_rssi;            // 平均RSSI
    
    // 状态
    bool blacklisted;           // 是否黑名单
    uint32_t blacklist_time;    // 黑名单时间
    uint8_t recovery_count;     // 恢复尝试次数
} channel_quality_t;

/*============================================================================
 * 信道管理器状态
 *============================================================================*/

typedef struct {
    channel_quality_t channels[RF_CHANNEL_COUNT];
    uint8_t active_channels[RF_CHANNEL_COUNT];  // 活跃信道列表
    uint8_t active_count;                        // 活跃信道数
    uint8_t current_index;                       // 当前跳频索引
    uint32_t last_update_ms;                     // 上次更新时间
} channel_manager_t;

/*============================================================================
 * API
 *============================================================================*/

/**
 * @brief 初始化信道管理器
 * @param mgr 管理器指针
 */
void ch_mgr_init(channel_manager_t *mgr);

/**
 * @brief 记录发送结果
 * @param mgr 管理器指针
 * @param channel 信道号
 * @param ack_received 是否收到ACK
 * @param rssi 信号强度
 */
void ch_mgr_record_tx(channel_manager_t *mgr, uint8_t channel, bool ack_received, int8_t rssi);

/**
 * @brief 记录CRC错误
 */
void ch_mgr_record_crc_error(channel_manager_t *mgr, uint8_t channel);

/**
 * @brief 周期性更新 (每秒调用)
 * @param mgr 管理器指针
 */
void ch_mgr_periodic_update(channel_manager_t *mgr);

/**
 * @brief 获取下一个跳频信道
 * @param mgr 管理器指针
 * @return 信道号
 */
uint8_t ch_mgr_get_next_channel(channel_manager_t *mgr);

/**
 * @brief 获取当前信道
 */
uint8_t ch_mgr_get_current_channel(channel_manager_t *mgr);

/**
 * @brief 检查信道是否在黑名单
 */
bool ch_mgr_is_blacklisted(channel_manager_t *mgr, uint8_t channel);

/**
 * @brief 获取信道质量
 */
uint8_t ch_mgr_get_channel_quality(channel_manager_t *mgr, uint8_t channel);

/**
 * @brief 获取活跃信道数
 */
uint8_t ch_mgr_get_active_count(channel_manager_t *mgr);

/**
 * @brief 获取最差信道
 */
uint8_t ch_mgr_get_worst_channel(channel_manager_t *mgr);

/**
 * @brief 获取最佳信道
 */
uint8_t ch_mgr_get_best_channel(channel_manager_t *mgr);

/**
 * @brief 强制刷新跳频序列
 */
void ch_mgr_refresh_hop_sequence(channel_manager_t *mgr);

/**
 * @brief 获取10秒窗口健康报告
 * @param mgr 管理器指针
 * @param total_loss_pct 总丢包率输出
 * @param worst_channel 最差信道输出
 * @param worst_loss_pct 最差信道丢包率输出
 */
void ch_mgr_get_health_report(channel_manager_t *mgr, 
                               uint8_t *total_loss_pct,
                               uint8_t *worst_channel,
                               uint8_t *worst_loss_pct);

/**
 * @brief v0.6.1: CCA (Clear Channel Assessment) 信道空闲检测
 * @param channel 信道号
 * @param threshold_dbm RSSI阈值 (默认-65dBm)
 * @return true 信道空闲, false 信道忙
 */
bool ch_mgr_is_channel_clear(uint8_t channel, int8_t threshold_dbm);

/**
 * @brief v0.6.1: 带CCA的发送 (自动避让忙信道)
 * @param mgr 管理器指针
 * @param max_retries 最大重试次数
 * @return 选中的空闲信道，失败返回当前信道
 */
uint8_t ch_mgr_get_clear_channel(channel_manager_t *mgr, uint8_t max_retries);

#endif // CHANNEL_MANAGER_H
