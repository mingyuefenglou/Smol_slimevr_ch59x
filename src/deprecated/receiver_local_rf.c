/**
 * @file receiver_local_rf.c
 * @brief Deprecated local RF functions from main_receiver.c
 * 
 * 这些函数已被模块化的 rf_receiver.c 替代
 * 保留仅供参考，不参与编译
 */

#if 0  // 不编译

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*============================================================================
 * 同步信标广播 (被 rf_receiver 的 slot_timer_callback 替代)
 *============================================================================*/

static void broadcast_beacon(void)
{
    static uint32_t last_beacon = 0;
    uint32_t now_us = rf_hw_get_time_us();
    
    // 每个超帧开始时发送
    if (now_us - last_beacon < RF_SUPERFRAME_US) {
        return;
    }
    last_beacon = now_us;
    
    // 更新帧号
    frame_number++;
    if (frame_number >= 65536) frame_number = 0;
    
    // 信道跳频
    current_channel = rf_get_hop_channel(frame_number, network_key);
    rf_hw_set_channel(current_channel);
    
    // 构建同步信标
    uint8_t beacon[16];
    beacon[0] = 0x53;  // 'S' - Sync beacon
    beacon[1] = network_key & 0xFF;
    beacon[2] = (network_key >> 8) & 0xFF;
    beacon[3] = (network_key >> 16) & 0xFF;
    beacon[4] = (network_key >> 24) & 0xFF;
    beacon[5] = frame_number & 0xFF;
    beacon[6] = (frame_number >> 8) & 0xFF;
    beacon[7] = current_channel;
    beacon[8] = active_tracker_count;
    
    // CRC
    uint16_t crc = rf_calc_crc16(beacon, 9);
    beacon[9] = crc & 0xFF;
    beacon[10] = (crc >> 8) & 0xFF;
    
    // 发送
    rf_hw_tx_mode();
    rf_hw_transmit(beacon, 11);
    rf_hw_rx_mode();
    
    superframe_start_us = now_us;
}

/*============================================================================
 * Tracker 数据接收 (被 rf_receiver_process + rf_hw 中断替代)
 *============================================================================*/

static void receive_tracker_data(void)
{
    if (!rf_hw_rx_available()) return;
    
    uint8_t buf[64];
    int8_t rssi;
    int len = rf_hw_receive(buf, sizeof(buf), &rssi);
    
    if (len > 0) {
        process_tracker_packet(buf, len, rssi);
    }
}

/*============================================================================
 * Tracker 数据包处理 (被 rf_receiver 的 rx_packet_handler 替代)
 *============================================================================*/

static void process_tracker_packet(const uint8_t *data, uint8_t len, int8_t rssi)
{
    if (!data || len < 21) return;
    if (data[0] != 0x54) return;  // 'T' - Tracker data
    
    uint8_t tid = data[1];
    if (tid >= MAX_TRACKERS) return;
    
    tracker_info_t *tracker = &trackers[tid];
    if (!tracker->paired) return;
    
    // CRC 验证
    uint16_t calc_crc = rf_calc_crc16(data, 19);
    uint16_t recv_crc = data[19] | (data[20] << 8);
    if (calc_crc != recv_crc) return;
    
    // 序列号检查
    uint8_t seq = data[2];
    if (tracker->active) {
        uint8_t expected = (tracker->sequence + 1) & 0xFF;
        if (seq != expected) {
            tracker->lost_packets++;
        }
    }
    tracker->sequence = seq;
    
    // 解析四元数
    int16_t qw = data[3] | (data[4] << 8);
    int16_t qx = data[5] | (data[6] << 8);
    int16_t qy = data[7] | (data[8] << 8);
    int16_t qz = data[9] | (data[10] << 8);
    
    tracker->quat[0] = qw;
    tracker->quat[1] = qx;
    tracker->quat[2] = qy;
    tracker->quat[3] = qz;
    
    // 解析加速度
    tracker->accel[0] = data[11] | (data[12] << 8);
    tracker->accel[1] = data[13] | (data[14] << 8);
    tracker->accel[2] = data[15] | (data[16] << 8);
    
    // 电池和状态
    tracker->battery = data[17];
    tracker->status = data[18];
    
    // RSSI
    tracker->rssi = (int8_t)rssi;
    
    // 更新时间戳
    tracker->last_seen = hal_get_tick_ms();
    tracker->active = true;
    
    // 发送 ACK
    uint8_t ack[8];
    ack[0] = 0x41;  // 'A' - ACK
    ack[1] = tid;
    ack[2] = seq;
    ack[3] = 0;     // 命令 (无)
    
    rf_hw_tx_mode();
    rf_hw_transmit(ack, 4);
    rf_hw_rx_mode();
}

/*============================================================================
 * 配对请求处理 (被 rf_receiver 的 rx_packet_handler 替代)
 *============================================================================*/

static void process_pairing_request(const uint8_t *data, uint8_t len)
{
    if (!data || len < 7) return;
    if (data[0] != 0x50) return;  // 'P' - Pairing request
    if (state != STATE_PAIRING) return;
    
    // 提取 MAC 地址
    uint8_t mac[6];
    memcpy(mac, &data[1], 6);
    
    // 查找或创建 tracker 槽位
    int slot = -1;
    for (int i = 0; i < MAX_TRACKERS; i++) {
        if (trackers[i].paired && 
            memcmp(trackers[i].mac, mac, 6) == 0) {
            slot = i;  // 已配对
            break;
        }
    }
    
    if (slot < 0) {
        // 找空槽
        for (int i = 0; i < MAX_TRACKERS; i++) {
            if (!trackers[i].paired) {
                slot = i;
                break;
            }
        }
    }
    
    if (slot < 0) return;  // 没有空槽
    
    // 配对
    trackers[slot].paired = true;
    memcpy(trackers[slot].mac, mac, 6);
    trackers[slot].active = false;
    active_tracker_count++;
    
    // 发送配对响应
    uint8_t response[16];
    response[0] = 0x52;  // 'R' - Pairing response
    response[1] = slot;  // Tracker ID
    response[2] = network_key & 0xFF;
    response[3] = (network_key >> 8) & 0xFF;
    response[4] = (network_key >> 16) & 0xFF;
    response[5] = (network_key >> 24) & 0xFF;
    
    rf_hw_tx_mode();
    rf_hw_transmit(response, 6);
    rf_hw_rx_mode();
    
    // 保存配置
    save_config();
}

/*============================================================================
 * 配对流程主函数 (被 rf_receiver_start_pairing + process 替代)
 *============================================================================*/

static void process_pairing(void)
{
    if (state != STATE_PAIRING) return;
    
    // 切换到配对信道
    rf_hw_set_channel(RF_PAIRING_CHANNEL);
    rf_hw_rx_mode();
    
    // 检查配对请求
    if (rf_hw_rx_available()) {
        uint8_t buf[32];
        int8_t rssi;
        int len = rf_hw_receive(buf, sizeof(buf), &rssi);
        if (len > 0) {
            process_pairing_request(buf, len);
        }
    }
    
    // 超时
    uint32_t now = hal_get_tick_ms();
    if (now - pair_mode_start > PAIR_MODE_TIMEOUT_MS) {
        enter_state(STATE_RUNNING);
    }
}

#endif  // 不编译
