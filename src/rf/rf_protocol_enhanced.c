/**
 * @file rf_protocol_enhanced.c
 * @brief 增强的 RF 协议实现
 * 
 * 功能:
 * - 自动重传 (ARQ)
 * - RSSI 测量和报告
 * - 信道质量评估
 * - 自适应跳频
 * - 时间同步
 */

#include "rf_protocol.h"
#include "rf_hw.h"
#include "hal.h"
#include <string.h>

/*============================================================================
 * 配置常量
 *============================================================================*/

// 重传配置
#define MAX_RETRIES             3
#define RETRY_DELAY_US          100
#define ACK_TIMEOUT_US          500

// RSSI 阈值
#define RSSI_GOOD               -60
#define RSSI_FAIR               -75
#define RSSI_POOR               -85

// 信道质量
#define CHANNEL_COUNT           40
#define QUALITY_HISTORY_SIZE    16
#define BLACKLIST_THRESHOLD     50      // 低于50%成功率加入黑名单
#define BLACKLIST_TIMEOUT_MS    30000   // 30秒后重新评估

// 时间同步
#define SYNC_BEACON_INTERVAL_MS 5       // 5ms 超帧
#define SLOT_DURATION_US        400     // 每个 slot 400us
#define GUARD_TIME_US           50      // 保护时间

/*============================================================================
 * 数据结构
 *============================================================================*/

// 信道质量记录
typedef struct {
    uint8_t success_count;      // 成功次数
    uint8_t total_count;        // 总尝试次数
    int8_t avg_rssi;            // 平均 RSSI
    uint32_t last_update;       // 最后更新时间
    bool blacklisted;           // 是否黑名单
    uint32_t blacklist_time;    // 加入黑名单时间
} channel_quality_t;

// 重传统计
typedef struct {
    uint32_t tx_count;          // 发送总数
    uint32_t tx_success;        // 成功次数
    uint32_t tx_retry;          // 重传次数
    uint32_t ack_timeout;       // ACK 超时次数
} retransmit_stats_t;

// 连接质量
typedef struct {
    int8_t rssi;                // 当前 RSSI
    int8_t rssi_avg;            // 平均 RSSI
    uint8_t packet_loss;        // 丢包率 (%)
    uint8_t link_quality;       // 链路质量 (0-100)
} link_quality_t;

/*============================================================================
 * 全局变量
 *============================================================================*/

static channel_quality_t channels[CHANNEL_COUNT];
static retransmit_stats_t stats;
static link_quality_t link;

// RSSI 历史 (滑动窗口平均)
static int8_t rssi_history[16];
static uint8_t rssi_index = 0;

// 当前跳频序列
static uint8_t hop_sequence[8];
static uint8_t hop_index = 0;

/*============================================================================
 * 信道质量管理
 *============================================================================*/

void rf_channel_init(void)
{
    memset(channels, 0, sizeof(channels));
    memset(&stats, 0, sizeof(stats));
    memset(&link, 0, sizeof(link));
    
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        channels[i].avg_rssi = -100;
    }
}

void rf_channel_update(uint8_t channel, bool success, int8_t rssi)
{
    if (channel >= CHANNEL_COUNT) return;
    
    channel_quality_t *ch = &channels[channel];
    
    ch->total_count++;
    if (success) {
        ch->success_count++;
    }
    
    // 更新 RSSI (指数移动平均)
    if (ch->avg_rssi == -100) {
        ch->avg_rssi = rssi;
    } else {
        ch->avg_rssi = (ch->avg_rssi * 7 + rssi) / 8;
    }
    
    ch->last_update = hal_get_tick_ms();
    
    // 检查是否需要加入黑名单
    if (ch->total_count >= 10) {
        uint8_t success_rate = (ch->success_count * 100) / ch->total_count;
        
        if (success_rate < BLACKLIST_THRESHOLD && !ch->blacklisted) {
            ch->blacklisted = true;
            ch->blacklist_time = hal_get_tick_ms();
        }
        
        // 重置计数器
        ch->success_count = success_rate;  // 保持比例
        ch->total_count = 100;
    }
    
    // 检查黑名单超时
    if (ch->blacklisted) {
        if ((hal_get_tick_ms() - ch->blacklist_time) > BLACKLIST_TIMEOUT_MS) {
            ch->blacklisted = false;
            ch->success_count = 0;
            ch->total_count = 0;
        }
    }
}

uint8_t rf_channel_get_quality(uint8_t channel)
{
    if (channel >= CHANNEL_COUNT) return 0;
    
    channel_quality_t *ch = &channels[channel];
    
    if (ch->blacklisted) return 0;
    if (ch->total_count == 0) return 50;  // 未知
    
    // 综合成功率和 RSSI
    uint8_t success_rate = (ch->success_count * 100) / ch->total_count;
    
    uint8_t rssi_score;
    if (ch->avg_rssi >= RSSI_GOOD) rssi_score = 100;
    else if (ch->avg_rssi >= RSSI_FAIR) rssi_score = 70;
    else if (ch->avg_rssi >= RSSI_POOR) rssi_score = 40;
    else rssi_score = 10;
    
    return (success_rate + rssi_score) / 2;
}

bool rf_channel_is_good(uint8_t channel)
{
    return rf_channel_get_quality(channel) >= 60;
}

void rf_channel_generate_hop_sequence(uint32_t seed)
{
    // 使用种子生成伪随机跳频序列
    // 避免黑名单信道
    
    uint8_t available[CHANNEL_COUNT];
    uint8_t count = 0;
    
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        if (!channels[i].blacklisted) {
            available[count++] = i;
        }
    }
    
    // 如果可用信道太少，重置黑名单
    if (count < 8) {
        for (int i = 0; i < CHANNEL_COUNT; i++) {
            channels[i].blacklisted = false;
        }
        count = CHANNEL_COUNT;
        for (int i = 0; i < CHANNEL_COUNT; i++) {
            available[i] = i;
        }
    }
    
    // 生成跳频序列 (简单 LFSR)
    uint32_t lfsr = seed;
    for (int i = 0; i < 8; i++) {
        lfsr = ((lfsr >> 1) ^ (-(lfsr & 1) & 0xB400u));
        hop_sequence[i] = available[lfsr % count];
    }
}

uint8_t rf_channel_get_next(void)
{
    uint8_t ch = hop_sequence[hop_index];
    hop_index = (hop_index + 1) % 8;
    return ch;
}

/*============================================================================
 * RSSI 管理
 *============================================================================*/

void rf_rssi_update(int8_t rssi)
{
    rssi_history[rssi_index] = rssi;
    rssi_index = (rssi_index + 1) % 16;
    
    // 计算平均
    int32_t sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += rssi_history[i];
    }
    link.rssi_avg = sum / 16;
    link.rssi = rssi;
    
    // 更新链路质量
    if (link.rssi_avg >= RSSI_GOOD) {
        link.link_quality = 100;
    } else if (link.rssi_avg >= RSSI_FAIR) {
        link.link_quality = 70 + (link.rssi_avg - RSSI_FAIR) * 2;
    } else if (link.rssi_avg >= RSSI_POOR) {
        link.link_quality = 30 + (link.rssi_avg - RSSI_POOR) * 2;
    } else {
        link.link_quality = 10;
    }
}

int8_t rf_rssi_get_current(void)
{
    return link.rssi;
}

int8_t rf_rssi_get_average(void)
{
    return link.rssi_avg;
}

uint8_t rf_get_link_quality(void)
{
    return link.link_quality;
}

/*============================================================================
 * 带重传的发送
 *============================================================================*/

int rf_transmit_with_ack(const uint8_t *data, uint8_t len, 
                         uint8_t *ack_buf, uint8_t ack_len,
                         uint8_t max_retries)
{
    int8_t rssi;
    stats.tx_count++;
    
    for (int retry = 0; retry <= max_retries; retry++) {
        if (retry > 0) {
            stats.tx_retry++;
            // 退避延时
            hal_delay_us(RETRY_DELAY_US + retry * 50);
        }
        
        // 发送
        rf_hw_set_mode(RF_MODE_TX);
        if (rf_hw_transmit(data, len) != 0) {
            continue;
        }
        
        // 等待 ACK
        rf_hw_set_mode(RF_MODE_RX);
        uint32_t start = hal_get_tick_us();
        
        while ((hal_get_tick_us() - start) < ACK_TIMEOUT_US) {
            if (rf_hw_data_ready()) {
                int ack_received = rf_hw_receive(ack_buf, ack_len, &rssi);
                if (ack_received > 0) {
                    // 更新统计
                    stats.tx_success++;
                    rf_rssi_update(rssi);
                    rf_channel_update(rf_hw_get_channel(), true, rssi);
                    
                    return ack_received;
                }
            }
        }
        
        // ACK 超时
        stats.ack_timeout++;
        rf_channel_update(rf_hw_get_channel(), false, -100);
    }
    
    // 所有重试失败
    link.packet_loss++;
    return -1;
}

/*============================================================================
 * 统计接口
 *============================================================================*/

void rf_get_stats(uint32_t *tx, uint32_t *success, uint32_t *retry, uint32_t *timeout)
{
    if (tx) *tx = stats.tx_count;
    if (success) *success = stats.tx_success;
    if (retry) *retry = stats.tx_retry;
    if (timeout) *timeout = stats.ack_timeout;
}

void rf_reset_stats(void)
{
    memset(&stats, 0, sizeof(stats));
}

uint8_t rf_get_packet_loss(void)
{
    if (stats.tx_count == 0) return 0;
    return 100 - (stats.tx_success * 100 / stats.tx_count);
}

/*============================================================================
 * 时间同步
 *============================================================================*/

// 同步信标结构
typedef struct {
    uint8_t marker;             // 0xBE
    uint8_t frame_num;          // 帧号
    uint32_t timestamp;         // 发送时间戳
    uint8_t network_key[4];     // 网络密钥
    uint8_t hop_sequence[8];    // 下一帧跳频序列
    uint8_t tracker_mask;       // 已连接追踪器位图
} sync_beacon_t;

static uint8_t frame_number = 0;
static uint32_t frame_start_time = 0;

void rf_sync_send_beacon(const uint8_t *network_key, uint8_t tracker_mask)
{
    sync_beacon_t beacon;
    
    beacon.marker = 0xBE;
    beacon.frame_num = frame_number++;
    beacon.timestamp = hal_get_tick_us();
    memcpy(beacon.network_key, network_key, 4);
    memcpy(beacon.hop_sequence, hop_sequence, 8);
    beacon.tracker_mask = tracker_mask;
    
    rf_hw_set_mode(RF_MODE_TX);
    rf_hw_transmit((uint8_t*)&beacon, sizeof(beacon));
    
    frame_start_time = beacon.timestamp;
}

bool rf_sync_receive_beacon(void *beacon_out, int8_t *rssi, uint32_t timeout_ms)
{
    rf_hw_set_mode(RF_MODE_RX);
    sync_beacon_t *beacon = (sync_beacon_t*)beacon_out;
    
    uint32_t start = hal_get_tick_ms();
    while ((hal_get_tick_ms() - start) < timeout_ms) {
        if (rf_hw_data_ready()) {
            uint8_t data[32];
            int len = rf_hw_receive(data, sizeof(data), rssi);
            
            if (len >= (int)sizeof(sync_beacon_t) && data[0] == 0xBE) {
                memcpy(beacon, data, sizeof(sync_beacon_t));
                return true;
            }
        }
    }
    
    return false;
}

uint32_t rf_sync_get_slot_time(uint8_t slot_id)
{
    // 计算 slot 开始时间
    // Beacon (250us) + Guard (50us) + Slot * (400us + 50us)
    return frame_start_time + 300 + slot_id * 450;
}

void rf_sync_wait_for_slot(uint8_t slot_id)
{
    uint32_t slot_time = rf_sync_get_slot_time(slot_id);
    
    while (hal_get_tick_us() < slot_time) {
        // 自旋等待
    }
}

/*============================================================================
 * ACK 包构建
 *============================================================================*/

void rf_build_ack_packet(uint8_t *packet, uint8_t tracker_id, uint8_t status)
{
    packet[0] = 0xAC;           // ACK marker
    packet[1] = tracker_id;
    packet[2] = status;         // 0=OK, 1=Resend, 2=Error
    packet[3] = frame_number;
}

bool rf_parse_ack_packet(const uint8_t *packet, uint8_t len, 
                         uint8_t expected_tracker_id, uint8_t *status)
{
    if (len < 4) return false;
    if (packet[0] != 0xAC) return false;
    if (packet[1] != expected_tracker_id) return false;
    
    if (status) *status = packet[2];
    return true;
}
