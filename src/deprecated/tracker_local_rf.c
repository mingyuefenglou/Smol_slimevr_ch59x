/**
 * @file tracker_local_rf.c
 * @brief Deprecated local RF functions from main_tracker.c
 * 
 * 这些函数已被模块化的 rf_transmitter.c 替代
 * 保留仅供参考，不参与编译
 */

#if 0  // 不编译

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*============================================================================
 * 数据包构建 (被 rf_transmitter 中的 build_data_packet 替代)
 *============================================================================*/

static void build_tracker_packet(uint8_t *packet)
{
    if (!packet) return;
    
    // 数据包头
    packet[0] = 0x54;  // 'T' - Tracker data
    packet[1] = tracker_id;
    packet[2] = ++tx_sequence;
    
    // 四元数 (Q15 格式, 8 bytes) - 添加范围钳位
    float q0 = quaternion[0], q1 = quaternion[1], q2 = quaternion[2], q3 = quaternion[3];
    if (q0 > 1.0f) q0 = 1.0f; else if (q0 < -1.0f) q0 = -1.0f;
    if (q1 > 1.0f) q1 = 1.0f; else if (q1 < -1.0f) q1 = -1.0f;
    if (q2 > 1.0f) q2 = 1.0f; else if (q2 < -1.0f) q2 = -1.0f;
    if (q3 > 1.0f) q3 = 1.0f; else if (q3 < -1.0f) q3 = -1.0f;
    int16_t qw = (int16_t)(q0 * 32767.0f);
    int16_t qx = (int16_t)(q1 * 32767.0f);
    int16_t qy = (int16_t)(q2 * 32767.0f);
    int16_t qz = (int16_t)(q3 * 32767.0f);
    
    packet[3] = qw & 0xFF;
    packet[4] = (qw >> 8) & 0xFF;
    packet[5] = qx & 0xFF;
    packet[6] = (qx >> 8) & 0xFF;
    packet[7] = qy & 0xFF;
    packet[8] = (qy >> 8) & 0xFF;
    packet[9] = qz & 0xFF;
    packet[10] = (qz >> 8) & 0xFF;
    
    // 线性加速度 (mg, 6 bytes) - 添加范围钳位
    float ax = accel_linear[0], ay = accel_linear[1], az = accel_linear[2];
    if (ax > 32.767f) ax = 32.767f; else if (ax < -32.768f) ax = -32.768f;
    if (ay > 32.767f) ay = 32.767f; else if (ay < -32.768f) ay = -32.768f;
    if (az > 32.767f) az = 32.767f; else if (az < -32.768f) az = -32.768f;
    int16_t iax = (int16_t)(ax * 1000.0f);
    int16_t iay = (int16_t)(ay * 1000.0f);
    int16_t iaz = (int16_t)(az * 1000.0f);
    
    packet[11] = iax & 0xFF;
    packet[12] = (iax >> 8) & 0xFF;
    packet[13] = iay & 0xFF;
    packet[14] = (iay >> 8) & 0xFF;
    packet[15] = iaz & 0xFF;
    packet[16] = (iaz >> 8) & 0xFF;
    
    // 电池和状态
    packet[17] = battery_percent;
    packet[18] = (is_charging ? 0x80 : 0) |
                 (battery_percent < 20 ? 0x40 : 0) |
                 (state == STATE_CALIBRATING ? 0x20 : 0);
    
    // CRC16
    uint16_t crc = rf_calc_crc16(packet, 19);
    packet[19] = crc & 0xFF;
    packet[20] = (crc >> 8) & 0xFF;
}

/*============================================================================
 * RF 发送任务 (被 rf_transmitter_process 的 TX_STATE_RUNNING 替代)
 *============================================================================*/

static void rf_transmit_task(void)
{
    if (state != STATE_RUNNING) return;
    
    uint32_t now_us = rf_hw_get_time_us();
    
    // 等待我的时隙
    uint32_t slot_start = sync_time_us + RF_SYNC_SLOT_US + 
                          (tracker_id * RF_TRACKER_SLOT_US);
    
    if (now_us < slot_start) {
        return;  // 还没到我的时隙
    }
    
    if (now_us > slot_start + RF_TRACKER_SLOT_US) {
        return;  // 错过了时隙
    }
    
    // 构建并发送数据包
    uint8_t packet[32];
    build_tracker_packet(packet);
    
    // 切换到当前信道
    rf_hw_set_channel(current_channel);
    rf_hw_tx_mode();
    
    // 发送
    int result = rf_hw_transmit(packet, 21);
    
    // 等待ACK
    rf_hw_rx_mode();
    uint32_t ack_start = rf_hw_get_time_us();
    
    while (rf_hw_get_time_us() - ack_start < RF_ACK_TIME_US) {
        if (rf_hw_rx_available()) {
            uint8_t ack[16];
            int8_t rssi;
            int len = rf_hw_receive(ack, sizeof(ack), &rssi);
            if (len > 0 && ack[0] == 0x41) {  // 'A' - ACK
                last_rssi = rssi;
                missed_sync_count = 0;
                // 处理ACK中的命令
                break;
            }
        }
    }
}

/*============================================================================
 * 同步搜索任务 (被 rf_transmitter 的 TX_STATE_SEARCHING 替代)
 *============================================================================*/

static void sync_search_task(void)
{
    if (state != STATE_SEARCH_SYNC) return;
    
    static uint32_t last_hop = 0;
    uint32_t now_ms = hal_get_tick_ms();
    
    // 信道跳频
    if (now_ms - last_hop > 10) {
        last_hop = now_ms;
        uint8_t ch = rf_get_hop_channel((now_ms / 10) % 1000, network_key);
        rf_hw_set_channel(ch);
    }
    
    // 检查是否收到同步信标
    rf_hw_rx_mode();
    if (rf_hw_rx_available()) {
        uint8_t buf[64];
        int8_t rssi;
        int len = rf_hw_receive(buf, sizeof(buf), &rssi);
        if (len > 0) {
            process_sync_beacon(buf, len);
        }
    }
    
    // 超时检查
    static uint32_t search_start = 0;
    if (search_start == 0) search_start = now_ms;
    if (now_ms - search_start > SYNC_SEARCH_TIMEOUT_MS) {
        // 进入低功耗模式
        enter_sleep_mode();
        search_start = 0;
    }
}

/*============================================================================
 * 同步信标处理 (被 rf_transmitter 内部的 rx_handler 替代)
 *============================================================================*/

static void process_sync_beacon(const uint8_t *data, uint8_t len)
{
    if (!data || len < 12) return;
    if (data[0] != 0x53) return;  // 'S' - Sync beacon
    
    // 解析信标
    uint32_t beacon_key = data[1] | (data[2] << 8) | 
                          (data[3] << 16) | (data[4] << 24);
    
    if (beacon_key != network_key) return;  // 密钥不匹配
    
    // 提取时序信息
    sync_time_us = rf_hw_get_time_us();
    frame_number = data[5] | (data[6] << 8);
    current_channel = data[7];
    
    // 进入正常运行状态
    enter_state(STATE_RUNNING);
}

/*============================================================================
 * 配对任务 (被 rf_transmitter_request_pairing 替代)
 *============================================================================*/

static void pairing_task(void)
{
    if (state != STATE_PAIRING) return;
    
    static uint32_t last_request = 0;
    uint32_t now_ms = hal_get_tick_ms();
    
    // 切换到配对信道
    rf_hw_set_channel(RF_PAIRING_CHANNEL);
    
    // 周期性发送配对请求
    if (now_ms - last_request > 100) {
        last_request = now_ms;
        
        uint8_t request[16];
        request[0] = 0x50;  // 'P' - Pairing request
        
        // MAC 地址
        uint8_t mac[6];
        rf_hw_get_mac_address(mac);
        memcpy(&request[1], mac, 6);
        
        // 发送
        rf_hw_tx_mode();
        rf_hw_transmit(request, 7);
        rf_hw_rx_mode();
    }
    
    // 检查配对响应
    if (rf_hw_rx_available()) {
        uint8_t buf[32];
        int8_t rssi;
        int len = rf_hw_receive(buf, sizeof(buf), &rssi);
        if (len > 0) {
            process_pairing_response(buf, len);
        }
    }
    
    // 超时
    if (now_ms - pair_mode_start > PAIRING_TIMEOUT_MS) {
        enter_state(is_paired ? STATE_RUNNING : STATE_IDLE);
    }
}

/*============================================================================
 * 配对响应处理 (被 rf_transmitter 内部逻辑替代)
 *============================================================================*/

static void process_pairing_response(const uint8_t *data, uint8_t len)
{
    if (!data || len < 10) return;
    if (data[0] != 0x52) return;  // 'R' - Pairing response
    
    // 提取配对信息
    tracker_id = data[1];
    network_key = data[2] | (data[3] << 8) | 
                  (data[4] << 16) | (data[5] << 24);
    
    // 保存配对数据
    is_paired = true;
    save_pairing_data();
    
    // 进入同步搜索
    enter_state(STATE_SEARCH_SYNC);
}

#endif  // 不编译
