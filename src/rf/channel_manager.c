/**
 * @file channel_manager.c
 * @brief v0.6.0 动态信道黑名单与自适应跳频实现
 */

#include "channel_manager.h"
#include "hal.h"
#include "event_logger.h"
#include <string.h>

// v0.6.2: 黑名单恢复超时
#define CH_BLACKLIST_RECOVERY_MS    30000   // 30秒后尝试恢复

/*============================================================================
 * 默认跳频序列
 *============================================================================*/

// v0.6.1: 优化跳频序列 (避开WiFi 1/6/11频段)
// WiFi信道1: 2412MHz -> RF Channel ~2
// WiFi信道6: 2437MHz -> RF Channel ~27
// WiFi信道11: 2462MHz -> RF Channel ~52
// 选择远离这些频段的信道
static const uint8_t default_hop_sequence[] = {
    8, 13, 18, 33, 38, 43, 58, 63,   // 主序列 (避开WiFi 1/6/11)
    10, 15, 35, 40, 60, 65, 70, 75   // 备用序列
};
#define DEFAULT_HOP_COUNT   16

/*============================================================================
 * 初始化
 *============================================================================*/

void ch_mgr_init(channel_manager_t *mgr)
{
    if (!mgr) return;
    
    memset(mgr, 0, sizeof(channel_manager_t));
    
    // 初始化所有信道为活跃
    for (uint8_t i = 0; i < RF_CHANNEL_COUNT && i < DEFAULT_HOP_COUNT; i++) {
        mgr->channels[i].blacklisted = false;
        mgr->channels[i].loss_rate_pct = 0;
        mgr->channels[i].avg_rssi = -50;  // 默认中等RSSI
        
        mgr->active_channels[i] = default_hop_sequence[i];
    }
    mgr->active_count = DEFAULT_HOP_COUNT;
    mgr->current_index = 0;
    mgr->last_update_ms = hal_get_tick_ms();
}

/*============================================================================
 * 记录统计
 *============================================================================*/

void ch_mgr_record_tx(channel_manager_t *mgr, uint8_t channel, bool ack_received, int8_t rssi)
{
    if (!mgr || channel >= RF_CHANNEL_COUNT) return;
    
    channel_quality_t *ch = &mgr->channels[channel];
    
    ch->tx_count++;
    if (ack_received) {
        ch->ack_count++;
    }
    
    // RSSI统计
    ch->rssi_sum += rssi;
    ch->rssi_samples++;
}

void ch_mgr_record_crc_error(channel_manager_t *mgr, uint8_t channel)
{
    if (!mgr || channel >= RF_CHANNEL_COUNT) return;
    mgr->channels[channel].crc_errors++;
}

/*============================================================================
 * 周期更新
 *============================================================================*/

void ch_mgr_periodic_update(channel_manager_t *mgr)
{
    if (!mgr) return;
    
    uint32_t now = hal_get_tick_ms();
    if ((now - mgr->last_update_ms) < CH_QUALITY_UPDATE_MS) {
        return;  // 未到更新时间
    }
    mgr->last_update_ms = now;
    
    uint8_t blacklist_count = 0;
    
    // 更新每个信道的质量
    for (uint8_t i = 0; i < RF_CHANNEL_COUNT; i++) {
        channel_quality_t *ch = &mgr->channels[i];
        
        if (ch->tx_count > 0) {
            // 计算丢包率
            uint32_t lost = ch->tx_count - ch->ack_count;
            ch->loss_rate_pct = (uint8_t)((lost * 100) / ch->tx_count);
        }
        
        if (ch->rssi_samples > 0) {
            // 计算平均RSSI
            ch->avg_rssi = (int8_t)(ch->rssi_sum / (int16_t)ch->rssi_samples);
        }
        
        // 黑名单判定
        if (!ch->blacklisted && ch->loss_rate_pct > CH_BLACKLIST_THRESHOLD) {
            // 检查是否还有足够的活跃信道
            if (mgr->active_count > CH_MIN_ACTIVE_CHANNELS) {
                ch->blacklisted = true;
                ch->blacklist_time = now;
                ch->recovery_count = 0;
                
                // 记录事件
                event_log_u8(EVT_RF_BLACKLIST, i);
            }
        }
        
        // 恢复判定 (黑名单超时后尝试恢复)
        if (ch->blacklisted && (now - ch->blacklist_time) > CH_BLACKLIST_RECOVERY_MS) {
            if (ch->loss_rate_pct < CH_RECOVERY_THRESHOLD) {
                ch->blacklisted = false;
            } else {
                // 重置黑名单计时，继续禁用
                ch->blacklist_time = now;
                ch->recovery_count++;
            }
        }
        
        if (ch->blacklisted) {
            blacklist_count++;
        }
        
        // 重置窗口统计 (部分衰减，保留历史趋势)
        ch->tx_count = ch->tx_count / 2;
        ch->ack_count = ch->ack_count / 2;
        ch->crc_errors = ch->crc_errors / 2;
        ch->rssi_sum = ch->rssi_sum / 2;
        ch->rssi_samples = ch->rssi_samples / 2;
    }
    
    // 刷新活跃信道列表
    ch_mgr_refresh_hop_sequence(mgr);
}

/*============================================================================
 * 跳频管理
 *============================================================================*/

void ch_mgr_refresh_hop_sequence(channel_manager_t *mgr)
{
    if (!mgr) return;
    
    mgr->active_count = 0;
    
    // 按丢包率排序，优先使用好的信道
    for (uint8_t i = 0; i < DEFAULT_HOP_COUNT; i++) {
        uint8_t ch = default_hop_sequence[i];
        if (ch < RF_CHANNEL_COUNT && !mgr->channels[ch].blacklisted) {
            mgr->active_channels[mgr->active_count++] = ch;
        }
    }
    
    // 保证最少有3个信道
    if (mgr->active_count < CH_MIN_ACTIVE_CHANNELS) {
        // 强制恢复一些黑名单信道
        for (uint8_t i = 0; i < DEFAULT_HOP_COUNT && mgr->active_count < CH_MIN_ACTIVE_CHANNELS; i++) {
            uint8_t ch = default_hop_sequence[i];
            if (ch < RF_CHANNEL_COUNT && mgr->channels[ch].blacklisted) {
                mgr->channels[ch].blacklisted = false;
                mgr->active_channels[mgr->active_count++] = ch;
            }
        }
    }
}

uint8_t ch_mgr_get_next_channel(channel_manager_t *mgr)
{
    if (!mgr || mgr->active_count == 0) {
        return default_hop_sequence[0];
    }
    
    mgr->current_index = (mgr->current_index + 1) % mgr->active_count;
    return mgr->active_channels[mgr->current_index];
}

uint8_t ch_mgr_get_current_channel(channel_manager_t *mgr)
{
    if (!mgr || mgr->active_count == 0) {
        return default_hop_sequence[0];
    }
    return mgr->active_channels[mgr->current_index];
}

/*============================================================================
 * 查询接口
 *============================================================================*/

bool ch_mgr_is_blacklisted(channel_manager_t *mgr, uint8_t channel)
{
    if (!mgr || channel >= RF_CHANNEL_COUNT) return false;
    return mgr->channels[channel].blacklisted;
}

uint8_t ch_mgr_get_channel_quality(channel_manager_t *mgr, uint8_t channel)
{
    if (!mgr || channel >= RF_CHANNEL_COUNT) return 0;
    
    // 返回质量分数 (0-100, 100最好)
    uint8_t loss = mgr->channels[channel].loss_rate_pct;
    return (loss > 100) ? 0 : (100 - loss);
}

uint8_t ch_mgr_get_active_count(channel_manager_t *mgr)
{
    return mgr ? mgr->active_count : 0;
}

uint8_t ch_mgr_get_worst_channel(channel_manager_t *mgr)
{
    if (!mgr) return 0;
    
    uint8_t worst = 0;
    uint8_t worst_loss = 0;
    
    for (uint8_t i = 0; i < DEFAULT_HOP_COUNT; i++) {
        uint8_t ch = default_hop_sequence[i];
        if (ch < RF_CHANNEL_COUNT && mgr->channels[ch].loss_rate_pct > worst_loss) {
            worst_loss = mgr->channels[ch].loss_rate_pct;
            worst = ch;
        }
    }
    
    return worst;
}

uint8_t ch_mgr_get_best_channel(channel_manager_t *mgr)
{
    if (!mgr) return default_hop_sequence[0];
    
    uint8_t best = default_hop_sequence[0];
    uint8_t best_loss = 100;
    
    for (uint8_t i = 0; i < DEFAULT_HOP_COUNT; i++) {
        uint8_t ch = default_hop_sequence[i];
        if (ch < RF_CHANNEL_COUNT && 
            !mgr->channels[ch].blacklisted &&
            mgr->channels[ch].loss_rate_pct < best_loss) {
            best_loss = mgr->channels[ch].loss_rate_pct;
            best = ch;
        }
    }
    
    return best;
}

/*============================================================================
 * 健康报告
 *============================================================================*/

void ch_mgr_get_health_report(channel_manager_t *mgr, 
                               uint8_t *total_loss_pct,
                               uint8_t *worst_channel,
                               uint8_t *worst_loss_pct)
{
    if (!mgr) return;
    
    uint32_t total_tx = 0;
    uint32_t total_ack = 0;
    uint8_t worst_ch = 0;
    uint8_t worst_loss = 0;
    
    for (uint8_t i = 0; i < RF_CHANNEL_COUNT; i++) {
        total_tx += mgr->channels[i].tx_count;
        total_ack += mgr->channels[i].ack_count;
        
        if (mgr->channels[i].loss_rate_pct > worst_loss) {
            worst_loss = mgr->channels[i].loss_rate_pct;
            worst_ch = i;
        }
    }
    
    if (total_loss_pct) {
        if (total_tx > 0) {
            *total_loss_pct = (uint8_t)(((total_tx - total_ack) * 100) / total_tx);
        } else {
            *total_loss_pct = 0;
        }
    }
    
    if (worst_channel) *worst_channel = worst_ch;
    if (worst_loss_pct) *worst_loss_pct = worst_loss;
}

/*============================================================================
 * v0.6.1: CCA (Clear Channel Assessment)
 *============================================================================*/

#include "rf_hw.h"

bool ch_mgr_is_channel_clear(uint8_t channel, int8_t threshold_dbm)
{
    // 保存当前信道
    uint8_t saved_channel = rf_hw_get_channel();
    
    // 切换到目标信道
    rf_hw_set_channel(channel);
    
    // 短暂等待让RSSI稳定
    hal_delay_us(50);
    
    // 读取RSSI
    int8_t rssi = rf_hw_get_rssi();
    
    // 恢复原信道
    rf_hw_set_channel(saved_channel);
    
    // 判断信道是否空闲
    return (rssi < threshold_dbm);
}

uint8_t ch_mgr_get_clear_channel(channel_manager_t *mgr, uint8_t max_retries)
{
    if (!mgr) return 0;
    
    const int8_t CCA_THRESHOLD_DBM = -65;
    
    for (uint8_t retry = 0; retry < max_retries; retry++) {
        uint8_t channel = ch_mgr_get_next_channel(mgr);
        
        if (ch_mgr_is_channel_clear(channel, CCA_THRESHOLD_DBM)) {
            return channel;
        }
    }
    
    // 所有重试失败，返回当前信道
    return ch_mgr_get_current_channel(mgr);
}
