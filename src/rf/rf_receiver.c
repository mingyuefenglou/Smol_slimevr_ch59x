/**
 * @file rf_receiver.c
 * @brief SlimeVR RF Receiver Implementation
 * 
 * Implements the receiver (dongle) side of the private RF protocol.
 * Manages TDMA scheduling, tracker synchronization, and data aggregation.
 * 
 * v0.6.2: 支持RF Ultra高效数据包格式
 */

#include "rf_protocol.h"
#include "rf_hw.h"
#include "hal.h"
#include "config.h"

// v0.6.2: RF Ultra支持
#if defined(USE_RF_ULTRA) && USE_RF_ULTRA
#include "rf_ultra.h"
#endif

#include <string.h>

// 中断控制宏 (避免与其他头文件冲突)
#ifndef __disable_irq
#define __disable_irq()  __asm__ volatile ("csrci mstatus, 0x08")
#endif
#ifndef __enable_irq
#define __enable_irq()   __asm__ volatile ("csrsi mstatus, 0x08")
#endif

/*============================================================================
 * Configuration
 *============================================================================*/

#define CHANNEL_EVAL_INTERVAL_MS    10000   // Evaluate channels every 10s
#define TRACKER_TIMEOUT_MS          500     // Mark tracker disconnected
#define MAX_CONSECUTIVE_LOSS        5       // Max missed packets before disconnect

/*============================================================================
 * Static Variables
 *============================================================================*/

static rf_receiver_ctx_t *rx_ctx = NULL;
static rf_rx_data_callback_t data_callback = NULL;
static rf_rx_connect_callback_t connect_callback = NULL;

// Slot timing
static volatile uint8_t current_slot = 0;
static volatile bool slot_active = false;
static volatile bool sync_sent = false;

// Pending command for tracker
static struct {
    uint8_t tracker_id;
    rf_command_t command;
    uint8_t param;
    bool pending;
} pending_cmd = {0};

// Statistics
static uint32_t slot_start_time_us;

/*============================================================================
 * Channel Hopping
 * 注: rf_calc_crc16和rf_get_hop_channel已移到rf_common.c
 *============================================================================*/

uint32_t rf_get_channel_freq(uint8_t channel)
{
    return (RF_BASE_FREQ_MHZ + channel * RF_CHANNEL_STEP_MHZ) * 1000000;
}

static bool is_channel_blacklisted(rf_receiver_ctx_t *ctx, uint8_t channel)
{
    uint8_t byte_idx = channel / 8;
    uint8_t bit_idx = channel % 8;
    return (ctx->channel_blacklist[byte_idx] & (1 << bit_idx)) != 0;
}

static uint8_t get_next_good_channel(rf_receiver_ctx_t *ctx, uint16_t frame)
{
    uint8_t channel;
    uint8_t attempts = 0;
    
    do {
        channel = rf_get_hop_channel(frame + attempts, ctx->network_key);
        attempts++;
    } while (is_channel_blacklisted(ctx, channel) && attempts < 10);
    
    return channel;
}

/*============================================================================
 * Packet Building
 *============================================================================*/

static void build_sync_beacon(rf_receiver_ctx_t *ctx, rf_sync_packet_t *pkt)
{
    memset(pkt, 0, sizeof(rf_sync_packet_t));
    
    pkt->header.type = (ctx->state == RX_STATE_PAIRING) ? 
                        RF_PKT_SYNC_PAIRING : RF_PKT_SYNC_BEACON;
    pkt->header.length = sizeof(rf_sync_packet_t) - sizeof(rf_header_t);
    pkt->frame_number = ctx->frame_number;
    
    // Build active tracker mask
    pkt->active_mask[0] = 0;
    pkt->active_mask[1] = 0;
    for (int i = 0; i < RF_MAX_TRACKERS; i++) {
        if (ctx->trackers[i].active) {
            if (i < 8)
                pkt->active_mask[0] |= (1 << i);
            else
                pkt->active_mask[1] |= (1 << (i - 8));
        }
    }
    
    // Build channel map for next 5 frames
    for (int i = 0; i < 5; i++) {
        pkt->channel_map[i] = get_next_good_channel(ctx, ctx->frame_number + i + 1);
    }
    
    pkt->tx_power = 7;  // Max power
    
    pkt->crc = rf_calc_crc16(pkt, sizeof(rf_sync_packet_t) - 2);
}

static void build_ack_packet(rf_ack_packet_t *pkt, uint8_t tracker_id, 
                              uint8_t sequence, rf_command_t cmd, uint8_t param)
{
    memset(pkt, 0, sizeof(rf_ack_packet_t));
    
    pkt->header.type = RF_PKT_ACK;
    pkt->header.length = sizeof(rf_ack_packet_t) - sizeof(rf_header_t);
    pkt->tracker_id = tracker_id;
    pkt->ack_sequence = sequence;
    pkt->command = cmd;
    pkt->command_data = param;
    
    pkt->crc = rf_calc_crc16(pkt, sizeof(rf_ack_packet_t) - 2);
}

/*============================================================================
 * Slot Timer Callback
 *============================================================================*/

static void slot_timer_callback(void)
{
    if (!rx_ctx) return;
    
    uint32_t now = rf_hw_get_time_us();
    uint32_t elapsed = now - rx_ctx->superframe_start_us;
    
    if (!sync_sent) {
        // Send sync beacon at start of superframe (non-blocking)
        rf_sync_packet_t sync_pkt;
        build_sync_beacon(rx_ctx, &sync_pkt);
        
        rf_hw_set_channel(rx_ctx->current_channel);
        rf_hw_tx_mode();
        // 使用非阻塞发送，避免在定时器回调中阻塞
        rf_hw_transmit_async((uint8_t *)&sync_pkt, sizeof(sync_pkt));
        
        sync_sent = true;
        current_slot = 0;
        slot_start_time_us = now + RF_SYNC_SLOT_US;
        return;
    }
    
    // Check if current slot is for an active tracker
    if (current_slot < RF_MAX_TRACKERS) {
        if (rx_ctx->trackers[current_slot].active) {
            // Switch to RX mode for this tracker's slot
            rf_hw_rx_mode();
            
            // Prepare ACK payload
            rf_ack_packet_t ack;
            rf_command_t cmd = RF_CMD_NONE;
            uint8_t param = 0;
            
            if (pending_cmd.pending && pending_cmd.tracker_id == current_slot) {
                cmd = pending_cmd.command;
                param = pending_cmd.param;
                pending_cmd.pending = false;
            }
            
            build_ack_packet(&ack, current_slot, 
                             rx_ctx->trackers[current_slot].last_sequence + 1,
                             cmd, param);
            rf_hw_set_ack_payload((uint8_t *)&ack, sizeof(ack));
        }
        
        // P0-1: 使用临界区保护slot递增操作
        __disable_irq();
        current_slot++;
        __enable_irq();
        
        // Schedule next slot
        rf_hw_start_timer(RF_DATA_SLOT_US, slot_timer_callback);
    } else {
        // End of frame - prepare for next superframe
        // P1-3: 使用临界区保护帧号递增
        __disable_irq();
        rx_ctx->frame_number++;
        __enable_irq();
        rx_ctx->current_channel = get_next_good_channel(rx_ctx, rx_ctx->frame_number);
        sync_sent = false;
        
        // v0.4.22 P0-2: 强制固定5000us超帧周期
        // 计算本帧实际用时，确保下一帧严格在5000us后开始
        uint32_t frame_elapsed = now - rx_ctx->superframe_start_us;
        uint32_t next_frame_delay;
        
        if (frame_elapsed < RF_SUPERFRAME_US) {
            // 还有剩余时间，等待到正好5000us
            next_frame_delay = RF_SUPERFRAME_US - frame_elapsed;
        } else {
            // 已超时（异常情况），立即开始下一帧
            // 但至少留出最小guard时间
            next_frame_delay = RF_GUARD_TIME_US;
        }
        
        rx_ctx->superframe_start_us = now + next_frame_delay;
        
        // Schedule next superframe with fixed timing
        rf_hw_start_timer(next_frame_delay, slot_timer_callback);
    }
}

/*============================================================================
 * Packet Reception Handler
 *============================================================================*/

static void rx_packet_handler(const uint8_t *data, uint8_t len, int8_t rssi)
{
    if (!rx_ctx || len < 1) return;
    
    // v0.6.2: 检测RF Ultra数据包 (12字节，特殊格式)
    #if defined(USE_RF_ULTRA) && USE_RF_ULTRA
    if (len == RF_ULTRA_PACKET_SIZE) {
        // 尝试解析为RF Ultra数据包
        rf_ultra_parsed_t parsed;
        if (rf_ultra_parse_packet(data, &parsed)) {
            parsed.rssi = rssi;
            
            // 验证tracker_id
            if (parsed.tracker_id >= RF_MAX_TRACKERS) return;
            if (!rx_ctx->trackers[parsed.tracker_id].active) return;
            
            tracker_info_t *tracker = &rx_ctx->trackers[parsed.tracker_id];
            
            // 更新tracker信息
            tracker->last_seen_ms = hal_millis();
            tracker->rssi = (uint8_t)(rssi + 128);
            tracker->battery = parsed.battery_pct;
            
            // 转换Q15四元数到float
            tracker->quaternion[0] = parsed.quat[0] / 32767.0f;
            tracker->quaternion[1] = parsed.quat[1] / 32767.0f;
            tracker->quaternion[2] = parsed.quat[2] / 32767.0f;
            tracker->quaternion[3] = parsed.quat[3] / 32767.0f;
            
            // 垂直加速度 (其他轴为0)
            tracker->acceleration[0] = 0.0f;
            tracker->acceleration[1] = 0.0f;
            tracker->acceleration[2] = parsed.accel_z_mg / 1000.0f;
            
            // 标记为已连接
            if (!tracker->connected) {
                tracker->connected = true;
                if (connect_callback) {
                    connect_callback(parsed.tracker_id, true);
                }
            }
            
            rx_ctx->total_packets++;
            return;  // 处理完毕
        }
    }
    #endif
    
    // 标准数据包处理
    if (len < sizeof(rf_header_t)) return;
    
    rf_header_t *header = (rf_header_t *)data;
    
    switch (header->type) {
        case RF_PKT_TRACKER_DATA: {
            if (len < sizeof(rf_tracker_packet_t)) return;
            
            rf_tracker_packet_t *pkt = (rf_tracker_packet_t *)data;
            
            // Verify CRC
            uint16_t calc_crc = rf_calc_crc16(pkt, sizeof(rf_tracker_packet_t) - 2);
            if (calc_crc != pkt->crc) return;
            
            // Verify tracker ID
            if (pkt->tracker_id >= RF_MAX_TRACKERS) return;
            if (!rx_ctx->trackers[pkt->tracker_id].active) return;
            
            tracker_info_t *tracker = &rx_ctx->trackers[pkt->tracker_id];
            
            // Check sequence for packet loss
            uint8_t expected_seq = tracker->last_sequence + 1;
            if (pkt->sequence != expected_seq && tracker->connected) {
                uint8_t lost = pkt->sequence - expected_seq;
                rx_ctx->lost_packets += lost;
                tracker->packet_loss = (tracker->packet_loss * 7 + lost * 10) / 8;
            } else {
                tracker->packet_loss = (tracker->packet_loss * 7) / 8;
            }
            
            tracker->last_sequence = pkt->sequence;
            tracker->last_seen_ms = hal_millis();
            tracker->rssi = (uint8_t)(rssi + 128);
            tracker->battery = pkt->battery;
            tracker->flags = pkt->flags;
            
            // Convert sensor data
            tracker->quaternion[0] = pkt->quat_w / 32767.0f;
            tracker->quaternion[1] = pkt->quat_x / 32767.0f;
            tracker->quaternion[2] = pkt->quat_y / 32767.0f;
            tracker->quaternion[3] = pkt->quat_z / 32767.0f;
            tracker->acceleration[0] = pkt->accel_x / 1000.0f;
            tracker->acceleration[1] = pkt->accel_y / 1000.0f;
            tracker->acceleration[2] = pkt->accel_z / 1000.0f;
            
            // Mark as connected
            if (!tracker->connected) {
                tracker->connected = true;
                if (connect_callback) {
                    connect_callback(pkt->tracker_id, true);
                }
            }
            
            rx_ctx->total_packets++;
            
            // Call data callback
            if (data_callback) {
                data_callback(pkt->tracker_id, pkt);
            }
            break;
        }
        
        case RF_PKT_PAIR_REQUEST: {
            if (rx_ctx->state != RX_STATE_PAIRING) return;
            if (len < sizeof(rf_pair_request_t)) return;
            
            rf_pair_request_t *req = (rf_pair_request_t *)data;
            
            // Verify CRC
            uint16_t calc_crc = rf_calc_crc16(req, sizeof(rf_pair_request_t) - 2);
            if (calc_crc != req->crc) return;
            
            // Find free slot or existing entry
            int8_t slot = -1;
            for (int i = 0; i < RF_MAX_TRACKERS; i++) {
                if (!rx_ctx->trackers[i].active) {
                    if (slot < 0) slot = i;
                } else if (memcmp(rx_ctx->trackers[i].mac_address, 
                                  req->mac_address, 6) == 0) {
                    slot = i;  // Already paired
                    break;
                }
            }
            
            if (slot < 0) return;  // No free slots
            
            // Send pairing response
            rf_pair_response_t resp;
            memset(&resp, 0, sizeof(resp));
            resp.header.type = RF_PKT_PAIR_RESPONSE;
            resp.header.length = sizeof(rf_pair_response_t) - sizeof(rf_header_t);
            memcpy(resp.mac_address, req->mac_address, 6);
            resp.tracker_id = slot;
            rf_hw_get_mac_address(resp.receiver_mac);
            resp.network_key = rx_ctx->network_key;
            resp.crc = rf_calc_crc16(&resp, sizeof(resp) - 2);
            
            rf_hw_tx_mode();
            rf_hw_transmit((uint8_t *)&resp, sizeof(resp));
            rf_hw_rx_mode();
            break;
        }
        
        case RF_PKT_PAIR_CONFIRM: {
            if (rx_ctx->state != RX_STATE_PAIRING) return;
            if (len < sizeof(rf_pair_confirm_t)) return;
            
            rf_pair_confirm_t *conf = (rf_pair_confirm_t *)data;
            
            // Verify CRC
            uint16_t calc_crc = rf_calc_crc16(conf, sizeof(rf_pair_confirm_t) - 2);
            if (calc_crc != conf->crc) return;
            
            if (conf->tracker_id >= RF_MAX_TRACKERS) return;
            if (conf->status != 0) return;
            
            // Activate tracker
            tracker_info_t *tracker = &rx_ctx->trackers[conf->tracker_id];
            memcpy(tracker->mac_address, conf->mac_address, 6);
            tracker->active = true;
            tracker->connected = false;
            tracker->last_sequence = 0;
            tracker->packet_loss = 0;
            
            rx_ctx->paired_count++;
            break;
        }
        
        default:
            break;
    }
}

/*============================================================================
 * Public API - Receiver
 *============================================================================*/

int rf_receiver_init(rf_receiver_ctx_t *ctx)
{
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(rf_receiver_ctx_t));
    ctx->state = RX_STATE_INIT;
    
    // 从存储加载或生成新的网络密钥
    if (!hal_storage_load_network_key(&ctx->network_key)) {
        // 使用硬件随机数或伪随机生成
        ctx->network_key = hal_get_random_u32();
        if (ctx->network_key == 0) {
            // 如果随机数为0，使用多熵源混合
            uint32_t entropy = hal_get_tick_ms();
            entropy ^= (entropy << 13);
            entropy ^= (entropy >> 17);
            entropy *= 0x9E3779B9;  // 黄金比例常数
            // 再次混入随机数
            entropy ^= hal_get_random_u32();
            ctx->network_key = (entropy != 0) ? entropy : 0xCAFEBABE;
        }
        // 保存到存储
        hal_storage_save_network_key(ctx->network_key);
    }
    
    // Initialize RF hardware
    rf_hw_config_t rf_cfg = {
        .mode = RF_MODE_2MBPS,
        .tx_power = RF_TX_POWER_4DBM,
        .addr_width = RF_ADDR_WIDTH_4,
        .crc_mode = RF_CRC_16BIT,
        .sync_word = RF_SYNC_WORD,
        .channel = 0,
        .auto_ack = true,
        .ack_timeout = 4,   // 1ms
        .retry_count = 0,
    };
    
    int err = rf_hw_init(&rf_cfg);
    if (err) return err;
    
    rf_hw_set_rx_callback(rx_packet_handler);
    
    rx_ctx = ctx;
    ctx->state = RX_STATE_IDLE;
    
    return 0;
}

int rf_receiver_start(rf_receiver_ctx_t *ctx)
{
    if (!ctx || ctx->state == RX_STATE_ERROR) return -1;
    
    ctx->state = RX_STATE_RUNNING;
    ctx->frame_number = 0;
    ctx->current_channel = get_next_good_channel(ctx, 0);
    ctx->superframe_start_us = rf_hw_get_time_us();
    
    sync_sent = false;
    current_slot = 0;
    
    // Start superframe timer
    rf_hw_start_timer(100, slot_timer_callback);  // Start immediately
    
    return 0;
}

int rf_receiver_start_pairing(rf_receiver_ctx_t *ctx)
{
    if (!ctx) return -1;
    
    // Stop normal operation
    rf_hw_stop_timer();
    
    ctx->state = RX_STATE_PAIRING;
    ctx->last_sync_ms = hal_millis();
    
    // Switch to fixed pairing channel
    rf_hw_set_channel(RF_PAIRING_CHANNEL);
    rf_hw_rx_mode();
    
    return 0;
}

void rf_receiver_stop_pairing(rf_receiver_ctx_t *ctx)
{
    if (!ctx) return;
    
    // Return to normal operation
    rf_receiver_start(ctx);
}

void rf_receiver_process(rf_receiver_ctx_t *ctx)
{
    if (!ctx) return;
    
    uint32_t now = hal_millis();
    
    // Check for tracker timeouts
    for (int i = 0; i < RF_MAX_TRACKERS; i++) {
        tracker_info_t *tracker = &ctx->trackers[i];
        
        if (tracker->active && tracker->connected) {
            if (now - tracker->last_seen_ms > TRACKER_TIMEOUT_MS) {
                tracker->connected = false;
                if (connect_callback) {
                    connect_callback(i, false);
                }
            }
        }
    }
    
    // Pairing timeout
    if (ctx->state == RX_STATE_PAIRING) {
        if (now - ctx->last_sync_ms > RF_PAIRING_TIMEOUT_MS) {
            rf_receiver_stop_pairing(ctx);
        }
        
        // Send periodic sync beacons during pairing
        if (now - ctx->last_sync_ms > 100) {
            ctx->last_sync_ms = now;
            
            rf_sync_packet_t sync_pkt;
            build_sync_beacon(ctx, &sync_pkt);
            
            rf_hw_tx_mode();
            rf_hw_transmit((uint8_t *)&sync_pkt, sizeof(sync_pkt));
            rf_hw_rx_mode();
        }
    }
}

int rf_receiver_send_command(rf_receiver_ctx_t *ctx, uint8_t tracker_id,
                              rf_command_t cmd, uint8_t param)
{
    if (!ctx || tracker_id >= RF_MAX_TRACKERS) return -1;
    if (!ctx->trackers[tracker_id].active) return -2;
    
    pending_cmd.tracker_id = tracker_id;
    pending_cmd.command = cmd;
    pending_cmd.param = param;
    pending_cmd.pending = true;
    
    return 0;
}

int rf_receiver_unpair(rf_receiver_ctx_t *ctx, uint8_t tracker_id)
{
    if (!ctx || tracker_id >= RF_MAX_TRACKERS) return -1;
    
    // 检查tracker是否已配对
    if (!ctx->trackers[tracker_id].active) return -2;
    
    // Send unpair command
    rf_receiver_send_command(ctx, tracker_id, RF_CMD_UNPAIR, 0);
    
    // Clear tracker info
    memset(&ctx->trackers[tracker_id], 0, sizeof(tracker_info_t));
    
    // 安全递减paired_count（防止下溢）
    if (ctx->paired_count > 0) {
        ctx->paired_count--;
    }
    
    return 0;
}

void rf_receiver_unpair_all(rf_receiver_ctx_t *ctx)
{
    if (!ctx) return;
    
    for (int i = 0; i < RF_MAX_TRACKERS; i++) {
        if (ctx->trackers[i].active) {
            rf_receiver_unpair(ctx, i);
        }
    }
}

void rf_receiver_set_data_callback(rf_rx_data_callback_t cb)
{
    data_callback = cb;
}

void rf_receiver_set_connect_callback(rf_rx_connect_callback_t cb)
{
    connect_callback = cb;
}
