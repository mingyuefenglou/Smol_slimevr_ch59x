/**
 * @file rf_transmitter.c
 * @brief SlimeVR RF Transmitter (Tracker) Implementation
 * 
 * Implements the transmitter side of the private RF protocol for
 * individual body trackers. Handles synchronization, data transmission,
 * and power management.
 */

#include "rf_protocol.h"
#include "rf_hw.h"
#include "hal.h"
#include "config.h"
#include "gyro_noise_filter.h"  // v0.4.22: 用于静止检测

// v0.6.2: RF优化模块 (条件编译)
#if defined(USE_CHANNEL_MANAGER) && USE_CHANNEL_MANAGER
#include "channel_manager.h"
#endif

#if defined(USE_RF_RECOVERY) && USE_RF_RECOVERY
#include "rf_recovery.h"
#endif

#if defined(USE_RF_TIMING_OPT) && USE_RF_TIMING_OPT
#include "rf_timing_opt.h"
#endif

#if defined(USE_RF_SLOT_OPTIMIZER) && USE_RF_SLOT_OPTIMIZER
#include "rf_slot_optimizer.h"
#endif

#if defined(USE_RF_ULTRA) && USE_RF_ULTRA
#include "rf_ultra.h"
#endif

#include <string.h>

/*============================================================================
 * Configuration
 *============================================================================*/

#define SYNC_SEARCH_TIMEOUT_MS      5000    // Search before giving up
#define SYNC_LOST_THRESHOLD         10      // Missed beacons before resync
#define TX_RETRY_COUNT              2       // Retries per slot
#define SLEEP_INACTIVITY_MS         30000   // Sleep after 30s no sync

/*============================================================================
 * v0.4.22 P1: 静止降速配置
 * 静止时降低发送频率以节省功耗，同时保持RF同步
 *============================================================================*/
#define STATIONARY_TX_DIVIDER       4       // 静止时每4帧发送1次(50Hz)
#define MOVING_TX_DIVIDER           1       // 运动时每帧发送(200Hz)
#define MICRO_MOTION_TX_DIVIDER     2       // 微动时每2帧发送(100Hz)

/*============================================================================
 * v0.6.2: RF自适应功率控制
 * 根据RSSI动态调整发射功率，信号强时降低功率节电
 *============================================================================*/
#define RSSI_HIGH_THRESHOLD         -45     // dBm, 信号很强
#define RSSI_MED_THRESHOLD          -60     // dBm, 信号中等
#define RSSI_LOW_THRESHOLD          -75     // dBm, 信号较弱
#define TX_POWER_HIGH               7       // +7dBm (最大)
#define TX_POWER_MED                4       // +4dBm
#define TX_POWER_LOW                0       // 0dBm (节电)
#define RSSI_HYSTERESIS             3       // dBm 滞后，防止频繁切换
#define RSSI_SAMPLE_COUNT           10      // RSSI采样数

/*============================================================================
 * Static Variables
 *============================================================================*/

static rf_transmitter_ctx_t *tx_ctx = NULL;
static rf_tx_sync_callback_t sync_callback = NULL;
static rf_tx_ack_callback_t ack_callback = NULL;

// Sync tracking
static uint8_t missed_sync_count = 0;
static uint8_t channel_map[5] = {0};
static uint8_t channel_map_idx = 0;

// Timing
static uint32_t slot_start_time_us = 0;
static bool in_my_slot = false;

// v0.4.22 P1: 静止降速状态
static uint32_t frame_skip_counter = 0;  // 帧跳过计数器
static uint8_t current_tx_divider = MOVING_TX_DIVIDER;  // 当前发送分频

// v0.6.2: RF自适应功率状态
static int8_t rssi_history[RSSI_SAMPLE_COUNT] = {-50};
static uint8_t rssi_history_idx = 0;
static uint8_t current_tx_power = TX_POWER_MED;

/*============================================================================
 * v0.6.2: RF自适应功率控制函数
 *============================================================================*/

/**
 * @brief 更新RSSI历史并计算平均值
 */
static int8_t update_rssi_avg(int8_t new_rssi)
{
    rssi_history[rssi_history_idx] = new_rssi;
    rssi_history_idx = (rssi_history_idx + 1) % RSSI_SAMPLE_COUNT;
    
    int32_t sum = 0;
    for (int i = 0; i < RSSI_SAMPLE_COUNT; i++) {
        sum += rssi_history[i];
    }
    return (int8_t)(sum / RSSI_SAMPLE_COUNT);
}

/**
 * @brief 根据RSSI自适应调整发射功率
 */
static void adaptive_tx_power_adjust(int8_t rssi)
{
    int8_t avg_rssi = update_rssi_avg(rssi);
    uint8_t new_power = current_tx_power;
    
    // 使用滞后阈值防止频繁切换
    if (avg_rssi > RSSI_HIGH_THRESHOLD + RSSI_HYSTERESIS) {
        // 信号很强，可以降低功率
        new_power = TX_POWER_LOW;
    } else if (avg_rssi > RSSI_MED_THRESHOLD + RSSI_HYSTERESIS) {
        // 信号中等
        new_power = TX_POWER_MED;
    } else if (avg_rssi < RSSI_LOW_THRESHOLD - RSSI_HYSTERESIS) {
        // 信号弱，提高功率
        new_power = TX_POWER_HIGH;
    }
    
    // 只在功率变化时才设置
    if (new_power != current_tx_power) {
        current_tx_power = new_power;
        rf_hw_set_tx_power(new_power);
    }
}

/*============================================================================
 * Packet Building
 *============================================================================*/

static void build_data_packet(rf_transmitter_ctx_t *ctx, rf_tracker_packet_t *pkt)
{
    memset(pkt, 0, sizeof(rf_tracker_packet_t));
    
    pkt->header.type = RF_PKT_TRACKER_DATA;
    pkt->header.length = sizeof(rf_tracker_packet_t) - sizeof(rf_header_t);
    pkt->tracker_id = ctx->tracker_id;
    pkt->sequence = ctx->sequence++;
    
    // Convert quaternion to int16
    // 四元数范围钳位防止溢出
    float q0 = ctx->quaternion[0], q1 = ctx->quaternion[1];
    float q2 = ctx->quaternion[2], q3 = ctx->quaternion[3];
    if (q0 > 1.0f) q0 = 1.0f; else if (q0 < -1.0f) q0 = -1.0f;
    if (q1 > 1.0f) q1 = 1.0f; else if (q1 < -1.0f) q1 = -1.0f;
    if (q2 > 1.0f) q2 = 1.0f; else if (q2 < -1.0f) q2 = -1.0f;
    if (q3 > 1.0f) q3 = 1.0f; else if (q3 < -1.0f) q3 = -1.0f;
    pkt->quat_w = (int16_t)(q0 * 32767.0f);
    pkt->quat_x = (int16_t)(q1 * 32767.0f);
    pkt->quat_y = (int16_t)(q2 * 32767.0f);
    pkt->quat_z = (int16_t)(q3 * 32767.0f);
    
    // 加速度范围钳位防止int16_t溢出 (±32.767g范围)
    float ax = ctx->acceleration[0], ay = ctx->acceleration[1], az = ctx->acceleration[2];
    if (ax > 32.767f) ax = 32.767f; else if (ax < -32.768f) ax = -32.768f;
    if (ay > 32.767f) ay = 32.767f; else if (ay < -32.768f) ay = -32.768f;
    if (az > 32.767f) az = 32.767f; else if (az < -32.768f) az = -32.768f;
    pkt->accel_x = (int16_t)(ax * 1000.0f);
    pkt->accel_y = (int16_t)(ay * 1000.0f);
    pkt->accel_z = (int16_t)(az * 1000.0f);
    
    pkt->battery = ctx->battery;
    pkt->flags = ctx->flags;
    
    pkt->crc = rf_calc_crc16(pkt, sizeof(rf_tracker_packet_t) - 2);
}

static void build_pair_request(rf_transmitter_ctx_t *ctx, rf_pair_request_t *pkt)
{
    memset(pkt, 0, sizeof(rf_pair_request_t));
    
    pkt->header.type = RF_PKT_PAIR_REQUEST;
    pkt->header.length = sizeof(rf_pair_request_t) - sizeof(rf_header_t);
    memcpy(pkt->mac_address, ctx->mac_address, 6);
    pkt->device_type = 0x01;  // Tracker
    pkt->firmware_version[0] = 1;
    pkt->firmware_version[1] = 0;
    
    pkt->crc = rf_calc_crc16(pkt, sizeof(rf_pair_request_t) - 2);
}

static void build_pair_confirm(rf_transmitter_ctx_t *ctx, rf_pair_confirm_t *pkt)
{
    memset(pkt, 0, sizeof(rf_pair_confirm_t));
    
    pkt->header.type = RF_PKT_PAIR_CONFIRM;
    pkt->header.length = sizeof(rf_pair_confirm_t) - sizeof(rf_header_t);
    pkt->tracker_id = ctx->tracker_id;
    memcpy(pkt->mac_address, ctx->mac_address, 6);
    pkt->status = 0;  // Success
    
    pkt->crc = rf_calc_crc16(pkt, sizeof(rf_pair_confirm_t) - 2);
}

/*============================================================================
 * v0.4.22 P1: 静止降速辅助函数
 * 根据运动状态动态调整发送频率
 *============================================================================*/

/**
 * @brief 更新发送分频器
 * 根据静止检测结果调整发送频率
 */
static void update_tx_divider(void)
{
    // 使用gyro_noise_filter的静止检测
    bool is_stationary = gyro_filter_is_stationary();
    
    if (is_stationary) {
        // 静止：每4帧发送1次 (50Hz)
        current_tx_divider = STATIONARY_TX_DIVIDER;
    } else {
        // 运动：每帧发送 (200Hz)
        current_tx_divider = MOVING_TX_DIVIDER;
    }
}

/**
 * @brief 检查本帧是否应该发送数据
 * @return true 应该发送, false 跳过本帧
 */
static bool should_transmit_this_frame(void)
{
    frame_skip_counter++;
    
    if (frame_skip_counter >= current_tx_divider) {
        frame_skip_counter = 0;
        return true;
    }
    
    return false;
}

/*============================================================================
 * Sync Beacon Processing
 *============================================================================*/

static void process_sync_beacon(rf_transmitter_ctx_t *ctx, 
                                 const rf_sync_packet_t *sync)
{
    // Verify CRC
    uint16_t calc_crc = rf_calc_crc16(sync, sizeof(rf_sync_packet_t) - 2);
    if (calc_crc != sync->crc) return;
    
    // Update frame number and timing
    ctx->frame_number = sync->frame_number;
    ctx->sync_time_us = rf_hw_get_time_us();
    ctx->last_sync_ms = hal_millis();
    
    // Store channel map
    memcpy(channel_map, sync->channel_map, 5);
    channel_map_idx = 0;
    
    missed_sync_count = 0;
    
    // v0.6.2: 通知时序优化模块同步成功
    #if defined(USE_RF_TIMING_OPT) && USE_RF_TIMING_OPT
    rf_timing_on_sync(ctx->sync_time_us, ctx->frame_number, channel_map[0]);
    #endif
    
    // v0.6.2: 通知RF自愈模块同步成功
    #if defined(USE_RF_RECOVERY) && USE_RF_RECOVERY
    extern rf_recovery_state_t rf_recovery_state;
    rf_recovery_report_sync_ok(&rf_recovery_state);
    #endif
    
    // Update state
    if (ctx->state == TX_STATE_SEARCHING) {
        ctx->state = TX_STATE_SYNCED;
    }
    
    // Check if we're in the active mask
    bool am_active = false;
    if (ctx->tracker_id < 8) {
        am_active = (sync->active_mask[0] & (1 << ctx->tracker_id)) != 0;
    } else if (ctx->tracker_id < 16) {
        am_active = (sync->active_mask[1] & (1 << (ctx->tracker_id - 8))) != 0;
    }
    
    if (!am_active && ctx->paired) {
        // We've been unpaired
        ctx->paired = false;
        ctx->state = TX_STATE_UNPAIRED;
    }
    
    // Call sync callback
    if (sync_callback) {
        sync_callback(ctx->frame_number);
    }
}

/*============================================================================
 * ACK Processing
 *============================================================================*/

static void process_ack(rf_transmitter_ctx_t *ctx, const rf_ack_packet_t *ack)
{
    // Verify CRC
    uint16_t calc_crc = rf_calc_crc16(ack, sizeof(rf_ack_packet_t) - 2);
    if (calc_crc != ack->crc) {
        // v0.6.2: CRC错误反馈给信道管理器
        #if defined(USE_CHANNEL_MANAGER) && USE_CHANNEL_MANAGER
        extern channel_manager_t ch_manager;
        ch_mgr_record_crc_error(&ch_manager, channel_map[channel_map_idx]);
        #endif
        return;
    }
    
    // Verify it's for us
    if (ack->tracker_id != ctx->tracker_id) return;
    
    // Mark ACK received
    ctx->pending_ack = 0;
    ctx->retry_count = 0;
    
    // v0.6.2: 成功ACK反馈给信道管理器
    #if defined(USE_CHANNEL_MANAGER) && USE_CHANNEL_MANAGER
    extern channel_manager_t ch_manager;
    // RSSI信息在ACK包中不可用，使用0作为默认值
    ch_mgr_record_tx(&ch_manager, channel_map[channel_map_idx], true, 0);
    #endif
    
    // v0.6.2: 时序反馈
    #if defined(USE_RF_TIMING_OPT) && USE_RF_TIMING_OPT
    rf_timing_slot_feedback(true, 0);
    #endif
    
    // Process command if present
    if (ack->command != RF_CMD_NONE) {
        switch (ack->command) {
            case RF_CMD_CALIBRATE_GYRO:
                ctx->flags |= RF_FLAG_CALIBRATING;
                // Trigger calibration in main app
                break;
                
            case RF_CMD_TARE:
                // Trigger tare in main app
                break;
                
            case RF_CMD_SLEEP:
                rf_transmitter_sleep(ctx);
                break;
                
            case RF_CMD_UNPAIR:
                ctx->paired = false;
                ctx->state = TX_STATE_UNPAIRED;
                // Clear stored pairing data
                break;
                
            default:
                break;
        }
    }
    
    // Call ACK callback
    if (ack_callback) {
        ack_callback(ack->ack_sequence, true);
    }
}

/*============================================================================
 * Pairing Response Processing
 *============================================================================*/

static void process_pair_response(rf_transmitter_ctx_t *ctx,
                                   const rf_pair_response_t *resp)
{
    // Verify CRC
    uint16_t calc_crc = rf_calc_crc16(resp, sizeof(rf_pair_response_t) - 2);
    if (calc_crc != resp->crc) return;
    
    // Verify it's for us
    if (memcmp(resp->mac_address, ctx->mac_address, 6) != 0) return;
    
    // Store pairing info
    ctx->tracker_id = resp->tracker_id;
    memcpy(ctx->receiver_mac, resp->receiver_mac, 6);
    ctx->network_key = resp->network_key;
    ctx->paired = true;
    
    // Send confirmation
    rf_pair_confirm_t conf;
    build_pair_confirm(ctx, &conf);
    
    rf_hw_tx_mode();
    rf_hw_transmit((uint8_t *)&conf, sizeof(conf));
    
    // Save to storage
    // hal_storage_save_pairing(...)
    
    ctx->state = TX_STATE_SEARCHING;
}

/*============================================================================
 * Slot Timer
 *============================================================================*/

static void calculate_my_slot_time(rf_transmitter_ctx_t *ctx)
{
    // Calculate when our slot starts relative to sync beacon
    uint32_t slot_offset = RF_SYNC_SLOT_US + (ctx->tracker_id * RF_DATA_SLOT_US);
    slot_start_time_us = ctx->sync_time_us + slot_offset;
}

static void wait_for_my_slot(rf_transmitter_ctx_t *ctx)
{
    uint32_t now = rf_hw_get_time_us();
    uint32_t target = slot_start_time_us;
    
    // Handle wraparound
    if (target > now) {
        uint32_t wait_us = target - now;
        
        // v0.4.22 P0修复: 使用低功耗等待代替忙等
        if (wait_us > 1000) {
            // 等待时间长，使用低功耗睡眠
            // 提前500us唤醒以确保准时
            hal_delay_us(wait_us - 500);
        }
        
        // 最后的精确等待使用WFI（低功耗等待中断）
        while (rf_hw_get_time_us() < target) {
#ifdef CH59X
            // __WFI 在 core_riscv.h 中定义，通过 hal.h 间接包含
            __asm__ volatile ("wfi");  // Wait For Interrupt - 低功耗等待
#endif
        }
    }
    
    in_my_slot = true;
}

/*============================================================================
 * RX Callback
 *============================================================================*/

static void rx_handler(const uint8_t *data, uint8_t len, int8_t rssi)
{
    (void)rssi;
    
    if (!tx_ctx || len < sizeof(rf_header_t)) return;
    
    rf_header_t *header = (rf_header_t *)data;
    
    switch (header->type) {
        case RF_PKT_SYNC_BEACON:
        case RF_PKT_SYNC_PAIRING:
            if (len >= sizeof(rf_sync_packet_t)) {
                process_sync_beacon(tx_ctx, (rf_sync_packet_t *)data);
            }
            break;
            
        case RF_PKT_PAIR_RESPONSE:
            if (tx_ctx->state == TX_STATE_PAIRING && 
                len >= sizeof(rf_pair_response_t)) {
                process_pair_response(tx_ctx, (rf_pair_response_t *)data);
            }
            break;
            
        case RF_PKT_ACK:
            if (len >= sizeof(rf_ack_packet_t)) {
                process_ack(tx_ctx, (rf_ack_packet_t *)data);
            }
            break;
            
        default:
            break;
    }
}

/*============================================================================
 * Public API - Transmitter
 *============================================================================*/

int rf_transmitter_init(rf_transmitter_ctx_t *ctx)
{
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(rf_transmitter_ctx_t));
    ctx->state = TX_STATE_INIT;
    
    // Get our MAC address
    rf_hw_get_mac_address(ctx->mac_address);
    
    // Initialize quaternion to identity
    ctx->quaternion[0] = 1.0f;
    ctx->quaternion[1] = 0.0f;
    ctx->quaternion[2] = 0.0f;
    ctx->quaternion[3] = 0.0f;
    
    // Load pairing info from storage
    // if (hal_storage_load_pairing(...) == 0) {
    //     ctx->paired = true;
    // }
    
    // Initialize RF hardware
    rf_hw_config_t rf_cfg = {
        .mode = RF_MODE_2MBPS,
        .tx_power = RF_TX_POWER_4DBM,
        .addr_width = RF_ADDR_WIDTH_4,
        .crc_mode = RF_CRC_16BIT,
        .sync_word = RF_SYNC_WORD,
        .channel = 0,
        .auto_ack = false,  // We handle ACK manually
        .ack_timeout = 0,
        .retry_count = 0,
    };
    
    int err = rf_hw_init(&rf_cfg);
    if (err) return err;
    
    rf_hw_set_rx_callback(rx_handler);
    
    tx_ctx = ctx;
    
    // v0.6.2: 注册到时隙优化器
    #if defined(USE_RF_SLOT_OPTIMIZER) && USE_RF_SLOT_OPTIMIZER
    // 注册时使用NORMAL优先级，后续可根据运动状态调整
    slot_optimizer_register(ctx->tracker_id, SLOT_PRIORITY_NORMAL);
    #endif
    
    if (ctx->paired) {
        ctx->state = TX_STATE_SEARCHING;
    } else {
        ctx->state = TX_STATE_UNPAIRED;
    }
    
    return 0;
}

int rf_transmitter_start(rf_transmitter_ctx_t *ctx)
{
    if (!ctx) return -1;
    
    if (ctx->paired) {
        ctx->state = TX_STATE_SEARCHING;
        ctx->last_sync_ms = hal_millis();
        
        // Start scanning for sync beacons
        rf_hw_set_channel(rf_get_hop_channel(0, ctx->network_key));
        rf_hw_rx_mode();
    } else {
        ctx->state = TX_STATE_UNPAIRED;
    }
    
    return 0;
}

int rf_transmitter_request_pairing(rf_transmitter_ctx_t *ctx)
{
    if (!ctx) return -1;
    
    ctx->state = TX_STATE_PAIRING;
    
    // Switch to pairing channel
    rf_hw_set_channel(RF_PAIRING_CHANNEL);
    rf_hw_rx_mode();
    
    // Wait for pairing sync beacon, then send request
    uint32_t start = hal_millis();
    bool got_sync = false;
    
    while (hal_millis() - start < 5000) {
        // Check for sync beacon
        if (rf_hw_rx_available()) {
            uint8_t buf[64];
            int8_t rssi;
            int len = rf_hw_receive(buf, sizeof(buf), &rssi);
            
            if (len > 0) {
                rf_header_t *header = (rf_header_t *)buf;
                if (header->type == RF_PKT_SYNC_PAIRING) {
                    got_sync = true;
                    break;
                }
            }
        }
        hal_delay_ms(10);
    }
    
    if (!got_sync) {
        ctx->state = TX_STATE_UNPAIRED;
        return -2;  // No receiver found
    }
    
    // Send pairing request
    rf_pair_request_t req;
    build_pair_request(ctx, &req);
    
    rf_hw_tx_mode();
    rf_hw_transmit((uint8_t *)&req, sizeof(req));
    rf_hw_rx_mode();
    
    // Wait for response
    start = hal_millis();
    while (hal_millis() - start < 1000) {
        if (rf_hw_rx_available()) {
            uint8_t buf[64];
            int8_t rssi;
            int len = rf_hw_receive(buf, sizeof(buf), &rssi);
            
            if (len > 0) {
                rx_handler(buf, len, rssi);
                
                if (ctx->paired) {
                    return 0;  // Success
                }
            }
        }
        hal_delay_ms(1);
    }
    
    ctx->state = TX_STATE_UNPAIRED;
    return -3;  // Pairing failed
}

void rf_transmitter_set_data(rf_transmitter_ctx_t *ctx,
                              const float quat[4],
                              const float accel[3],
                              uint8_t battery,
                              uint8_t flags)
{
    if (!ctx || !quat || !accel) return;
    
    memcpy(ctx->quaternion, quat, sizeof(float) * 4);
    memcpy(ctx->acceleration, accel, sizeof(float) * 3);
    ctx->battery = battery;
    ctx->flags = flags;
}

void rf_transmitter_process(rf_transmitter_ctx_t *ctx)
{
    if (!ctx) return;
    
    uint32_t now_ms = hal_millis();
    
    switch (ctx->state) {
        case TX_STATE_UNPAIRED:
            // Waiting for user to initiate pairing
            break;
            
        case TX_STATE_PAIRING:
            // Handled in rf_transmitter_request_pairing
            break;
            
        case TX_STATE_SEARCHING: {
            // Look for sync beacon
            rf_hw_rx_mode();
            
            if (rf_hw_rx_available()) {
                uint8_t buf[64];
                int8_t rssi;
                int len = rf_hw_receive(buf, sizeof(buf), &rssi);
                if (len > 0) {
                    rx_handler(buf, len, rssi);
                }
            }
            
            // Channel hop while searching
            static uint32_t last_hop = 0;
            if (now_ms - last_hop > 10) {
                last_hop = now_ms;
                uint8_t ch = rf_get_hop_channel((now_ms / 10) % 1000, ctx->network_key);
                rf_hw_set_channel(ch);
            }
            
            // Timeout
            if (now_ms - ctx->last_sync_ms > SYNC_SEARCH_TIMEOUT_MS) {
                rf_transmitter_sleep(ctx);
            }
            break;
        }
            
        case TX_STATE_SYNCED:
            // We have sync, transition to running
            ctx->state = TX_STATE_RUNNING;
            calculate_my_slot_time(ctx);
            break;
            
        case TX_STATE_RUNNING: {
            // Normal operation
            
            // Wait for sync beacon at frame start
            rf_hw_rx_mode();
            
            uint32_t frame_start = ctx->sync_time_us;
            uint32_t now_us = rf_hw_get_time_us();
            
            // Check if we're in sync window
            if (now_us < frame_start + RF_SYNC_SLOT_US) {
                // Wait for sync
                if (rf_hw_rx_available()) {
                    uint8_t buf[64];
                    int8_t rssi;
                    int len = rf_hw_receive(buf, sizeof(buf), &rssi);
                    if (len > 0) {
                        rx_handler(buf, len, rssi);
                    }
                }
            } else {
                // Sync window passed, check if we got beacon
                missed_sync_count++;
                
                // v0.6.2: 报告同步丢失给RF自愈模块
                #if defined(USE_RF_RECOVERY) && USE_RF_RECOVERY
                extern rf_recovery_state_t rf_recovery_state;
                recovery_action_t action = rf_recovery_report_miss_sync(&rf_recovery_state);
                if (action == RECOVERY_RESYNC) {
                    ctx->state = TX_STATE_SEARCHING;
                    break;
                } else if (action == RECOVERY_ABORT) {
                    // 重新初始化RF - 使用默认配置重新初始化
                    rf_hw_config_t rf_cfg = {
                        .mode = RF_MODE_2MBPS,
                        .tx_power = RF_TX_POWER_4DBM,
                        .addr_width = RF_ADDR_WIDTH_4,
                        .crc_mode = RF_CRC_16BIT,
                        .sync_word = RF_SYNC_WORD,
                        .channel = 0,
                        .auto_ack = false,
                        .ack_timeout = 0,
                        .retry_count = 0,
                    };
                    rf_hw_init(&rf_cfg);
                    ctx->state = TX_STATE_SEARCHING;
                    break;
                }
                #endif
                
                if (missed_sync_count > SYNC_LOST_THRESHOLD) {
                    ctx->state = TX_STATE_SEARCHING;
                    break;
                }
                
                // Use predicted timing
                ctx->frame_number++;
                ctx->sync_time_us += RF_SUPERFRAME_US;
            }
            
            // Wait for our slot
            calculate_my_slot_time(ctx);
            
            // v0.6.2: 使用时隙优化器获取动态时隙
            #if defined(USE_RF_SLOT_OPTIMIZER) && USE_RF_SLOT_OPTIMIZER
            uint8_t opt_tracker_id;
            uint16_t opt_slot_duration_us;
            if (slot_optimizer_get_current(&opt_tracker_id, &opt_slot_duration_us) == 0) {
                // 使用优化后的时隙参数 - 计算时隙开始时间（仅用于日志，不存储）
                // uint32_t slot_start = ctx->sync_time_us + RF_SYNC_SLOT_US + 
                //                       opt_tracker_id * opt_slot_duration_us;
                (void)opt_tracker_id;
                (void)opt_slot_duration_us;
            }
            #endif
            
            // v0.6.2: 使用时序优化模块等待时隙
            #if defined(USE_RF_TIMING_OPT) && USE_RF_TIMING_OPT
            rf_timing_set_slot(ctx->tracker_id, MAX_TRACKERS);
            rf_timing_wait_slot();
            #else
            wait_for_my_slot(ctx);
            #endif
            
            // v0.4.22 P1: 更新发送分频器（根据静止状态）
            update_tx_divider();
            
            // v0.4.22 P1: 静止降速 - 检查本帧是否需要发送
            if (!should_transmit_this_frame()) {
                // 跳过本帧发送，但保持同步
                // 进入低功耗等待直到下一帧
                in_my_slot = false;
                break;
            }
            
            // v0.6.2: 帧开始，通知时隙优化器
            #if defined(USE_RF_SLOT_OPTIMIZER) && USE_RF_SLOT_OPTIMIZER
            slot_optimizer_frame_start();
            #endif
            
            // Switch to next channel
            if (channel_map_idx < 5) {
                ctx->current_channel = channel_map[channel_map_idx++];
            } else {
                ctx->current_channel = rf_get_hop_channel(ctx->frame_number, ctx->network_key);
            }
            
            // v0.6.2: 使用信道管理器检查信道
            #if defined(USE_CHANNEL_MANAGER) && USE_CHANNEL_MANAGER
            extern channel_manager_t ch_manager;
            if (!ch_mgr_is_channel_clear(ctx->current_channel, -65)) {
                // 信道繁忙，尝试获取替代信道
                uint8_t alt_ch = ch_mgr_get_clear_channel(&ch_manager, 2);
                if (alt_ch != 0xFF) {
                    ctx->current_channel = alt_ch;
                }
            }
            #endif
            
            rf_hw_set_channel(ctx->current_channel);
            
            // v0.6.2: 预备信道切换
            #if defined(USE_RF_TIMING_OPT) && USE_RF_TIMING_OPT
            rf_timing_prepare_channel(ctx->current_channel);
            #endif
            
            // v0.6.2: 构建并发送数据包 - 使用RF Ultra或标准格式
            uint32_t tx_start_us = rf_hw_get_time_us();
            
            #if defined(USE_RF_ULTRA) && USE_RF_ULTRA
            // 使用RF Ultra高效数据包格式 (12字节 vs 21字节标准格式)
            {
                uint8_t ultra_pkt[RF_ULTRA_PACKET_SIZE];
                
                // 转换float四元数到Q15格式
                q15_t quat_q15[4];
                quat_q15[0] = (q15_t)(ctx->quaternion[0] * 32767.0f);
                quat_q15[1] = (q15_t)(ctx->quaternion[1] * 32767.0f);
                quat_q15[2] = (q15_t)(ctx->quaternion[2] * 32767.0f);
                quat_q15[3] = (q15_t)(ctx->quaternion[3] * 32767.0f);
                
                // 提取垂直加速度 (mg单位) - 使用acceleration而非accel_linear
                int16_t accel_z_mg = (int16_t)(ctx->acceleration[2] * 1000.0f);
                
                rf_ultra_build_quat_packet(ultra_pkt, 
                                           ctx->tracker_id,
                                           quat_q15,
                                           accel_z_mg,
                                           ctx->battery);  // 使用battery而非battery_level
                
                rf_hw_tx_mode();
                int result = rf_hw_transmit(ultra_pkt, RF_ULTRA_PACKET_SIZE);
                (void)result;  // 忽略警告
            }
            ctx->sequence++;  // 序列号自增
            #else
            // 使用标准数据包格式
            rf_tracker_packet_t pkt;
            build_data_packet(ctx, &pkt);
            
            rf_hw_tx_mode();
            int result = rf_hw_transmit((uint8_t *)&pkt, sizeof(pkt));
            (void)result;  // 忽略警告
            #endif
            
            // Wait for ACK
            rf_hw_rx_mode();
            uint32_t ack_start = rf_hw_get_time_us();
            bool got_ack = false;
            
            while (rf_hw_get_time_us() - ack_start < RF_ACK_TIME_US * 2) {
                if (rf_hw_rx_available()) {
                    uint8_t buf[32];
                    int8_t rssi;
                    int len = rf_hw_receive(buf, sizeof(buf), &rssi);
                    if (len > 0) {
                        rx_handler(buf, len, rssi);
                        got_ack = true;
                        break;
                    }
                }
            }
            
            // v0.6.2: 计算传输延迟
            uint32_t tx_latency_us = rf_hw_get_time_us() - tx_start_us;
            
            // v0.6.2: 传输结果反馈
            if (!got_ack) {
                // 未收到ACK
                #if defined(USE_CHANNEL_MANAGER) && USE_CHANNEL_MANAGER
                extern channel_manager_t ch_manager;
                ch_mgr_record_tx(&ch_manager, ctx->current_channel, false, -127);
                #endif
                
                #if defined(USE_RF_TIMING_OPT) && USE_RF_TIMING_OPT
                rf_timing_slot_feedback(false, 0);
                #endif
                
                #if defined(USE_RF_SLOT_OPTIMIZER) && USE_RF_SLOT_OPTIMIZER
                slot_optimizer_report_result(ctx->tracker_id, false, tx_latency_us);
                #endif
            } else {
                // 收到ACK - 报告成功
                #if defined(USE_RF_SLOT_OPTIMIZER) && USE_RF_SLOT_OPTIMIZER
                slot_optimizer_report_result(ctx->tracker_id, true, tx_latency_us);
                #endif
            }
            
            in_my_slot = false;
            
            // Call ACK callback
            if (ack_callback) {
                #if defined(USE_RF_ULTRA) && USE_RF_ULTRA
                ack_callback(ctx->sequence - 1, got_ack);  // sequence已经自增
                #else
                ack_callback(pkt.sequence, got_ack);
                #endif
            }
            
            // Enter low power until next frame
            rf_hw_standby();
            break;
        }
            
        case TX_STATE_SLEEP:
            // Low power mode
            break;
            
        case TX_STATE_ERROR:
            // Try to recover
            ctx->state = TX_STATE_SEARCHING;
            break;
            
        default:
            break;
    }
}

void rf_transmitter_sleep(rf_transmitter_ctx_t *ctx)
{
    if (!ctx) return;
    
    ctx->state = TX_STATE_SLEEP;
    rf_hw_sleep();
}

void rf_transmitter_wake(rf_transmitter_ctx_t *ctx)
{
    if (!ctx) return;
    
    // Reinitialize RF
    rf_hw_config_t rf_cfg = {
        .mode = RF_MODE_2MBPS,
        .tx_power = RF_TX_POWER_4DBM,
        .addr_width = RF_ADDR_WIDTH_4,
        .crc_mode = RF_CRC_16BIT,
        .sync_word = RF_SYNC_WORD,
        .channel = 0,
        .auto_ack = false,
        .ack_timeout = 0,
        .retry_count = 0,
    };
    rf_hw_init(&rf_cfg);
    rf_hw_set_rx_callback(rx_handler);
    
    ctx->state = TX_STATE_SEARCHING;
    ctx->last_sync_ms = hal_millis();
}

void rf_transmitter_set_sync_callback(rf_tx_sync_callback_t cb)
{
    sync_callback = cb;
}

void rf_transmitter_set_ack_callback(rf_tx_ack_callback_t cb)
{
    ack_callback = cb;
}
