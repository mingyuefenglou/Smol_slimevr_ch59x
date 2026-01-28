/**
 * @file slime_packet.c
 * @brief SlimeVR Packet Translation Layer (CH59X → nRF packet0~4 语义)
 * 
 * v0.4.22 P1-4: Receiver端协议兼容翻译层
 * 
 * 将CH59X私有RF帧转换为nRF Smol Slime兼容的packet格式，
 * 便于SlimeVR Server理解电量/状态/错误/版本信息。
 * 
 * Packet格式 (每个16字节):
 * - packet0: 设备信息 (版本/传感器类型/电压/温度)
 * - packet1: 全精度姿态 (quat + accel)
 * - packet2: 压缩姿态 + 电量/温度/RSSI
 * - packet3: 状态包 (错误/校准/电量状态)
 * - packet4: 磁力计数据 (可选)
 */

#include "slime_packet.h"
#include "rf_protocol.h"
#include <string.h>

/*============================================================================
 * 工具函数
 *============================================================================*/

/**
 * @brief 写入little-endian int16
 */
static inline void wr_i16_le(uint8_t *p, int16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

/**
 * @brief accel mg → fixed7 (近似)
 * fixed7 ≈ mg * 1.25525 (= mg * 0.00980665 * 128)
 */
static inline int16_t accel_mg_to_fixed7(int16_t mg)
{
    int32_t v = (int32_t)mg * 125525 / 100000;
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    return (int16_t)v;
}

/*============================================================================
 * 状态映射
 *============================================================================*/

/**
 * @brief 映射CH flags到tracker_status bitfield
 */
static inline uint8_t map_tracker_status(uint8_t ch_flags)
{
    uint8_t s = 0;
    if (ch_flags & RF_FLAG_LOW_BATTERY)  s |= SLIME_STATUS_LOW_BATT;
    if (ch_flags & RF_FLAG_CHARGING)     s |= SLIME_STATUS_CHARGING;
    if (ch_flags & RF_FLAG_CALIBRATING)  s |= SLIME_STATUS_CALIBRATING;
    if (ch_flags & RF_FLAG_IMU_ERROR)    s |= SLIME_STATUS_IMU_ERROR;
    if (ch_flags & RF_FLAG_RF_LOST)      s |= SLIME_STATUS_RF_LOST;
    return s;
}

/**
 * @brief 推断server_status (OK/CALIB/ERROR)
 */
static inline uint8_t map_server_status(uint8_t ch_flags)
{
    if (ch_flags & (RF_FLAG_IMU_ERROR | RF_FLAG_ERROR)) {
        return SLIME_SERVER_ERROR;
    }
    if (ch_flags & RF_FLAG_CALIBRATING) {
        return SLIME_SERVER_CALIB;
    }
    return SLIME_SERVER_OK;
}

/*============================================================================
 * Packet 生成函数
 *============================================================================*/

void slime_make_packet1(uint8_t out[SLIME_PACKET_SIZE], const slime_tracker_data_t *data)
{
    memset(out, 0, SLIME_PACKET_SIZE);
    
    out[0] = SLIME_PKT_TYPE_QUAT_ACCEL;  // 0x01
    out[1] = data->tracker_id;
    
    // nRF语义: qx, qy, qz, qw (注意CH通常是wxyz，需要转换)
    // CH: quat_w, quat_x, quat_y, quat_z
    // nRF: quat_x, quat_y, quat_z, quat_w
    wr_i16_le(&out[2], data->quat_x);   // qx
    wr_i16_le(&out[4], data->quat_y);   // qy
    wr_i16_le(&out[6], data->quat_z);   // qz
    wr_i16_le(&out[8], data->quat_w);   // qw
    
    // Accel (转换为fixed7格式)
    wr_i16_le(&out[10], accel_mg_to_fixed7(data->accel_x));
    wr_i16_le(&out[12], accel_mg_to_fixed7(data->accel_y));
    wr_i16_le(&out[14], accel_mg_to_fixed7(data->accel_z));
}

void slime_make_packet2(uint8_t out[SLIME_PACKET_SIZE], const slime_tracker_data_t *data)
{
    memset(out, 0, SLIME_PACKET_SIZE);
    
    out[0] = SLIME_PKT_TYPE_COMPRESSED;  // 0x02
    out[1] = data->tracker_id;
    
    // Battery (bit7=valid flag)
    uint8_t batt_pct = data->battery_pct;
    if (batt_pct > 100) batt_pct = 100;
    out[2] = 0x80 | batt_pct;
    
    // Battery voltage (待扩展，暂填0)
    out[3] = 0;
    
    // Temperature (待扩展，暂填0)
    out[4] = 0;
    
    // 压缩四元数 (32-bit) - v0.6.2 smallest-three压缩
    uint32_t q_compressed = compress_quat_simple(
        data->quat_w, data->quat_x, data->quat_y, data->quat_z);
    out[5] = (uint8_t)(q_compressed & 0xFF);
    out[6] = (uint8_t)((q_compressed >> 8) & 0xFF);
    out[7] = (uint8_t)((q_compressed >> 16) & 0xFF);
    out[8] = (uint8_t)((q_compressed >> 24) & 0xFF);
    
    // Accel
    wr_i16_le(&out[9], accel_mg_to_fixed7(data->accel_x));
    wr_i16_le(&out[11], accel_mg_to_fixed7(data->accel_y));
    wr_i16_le(&out[13], accel_mg_to_fixed7(data->accel_z));
    
    // RSSI
    out[15] = (uint8_t)data->rssi;
}

void slime_make_packet3(uint8_t out[SLIME_PACKET_SIZE], const slime_tracker_data_t *data)
{
    memset(out, 0, SLIME_PACKET_SIZE);
    
    out[0] = SLIME_PKT_TYPE_STATUS;      // 0x03
    out[1] = data->tracker_id;
    out[2] = map_server_status(data->flags);   // server_status
    out[3] = map_tracker_status(data->flags);  // tracker_status bitfield
    // byte 4..15 保留/填0
}

void slime_make_packet0(uint8_t out[SLIME_PACKET_SIZE], const slime_device_info_t *info)
{
    memset(out, 0, SLIME_PACKET_SIZE);
    
    out[0] = SLIME_PKT_TYPE_INFO;        // 0x00
    out[1] = info->tracker_id;
    out[2] = info->fw_version_major;
    out[3] = info->fw_version_minor;
    out[4] = info->board_id;
    out[5] = info->mcu_id;
    out[6] = info->imu_id;
    out[7] = info->mag_id;
    
    // Battery
    out[8] = info->battery_pct;
    wr_i16_le(&out[9], info->battery_mv);
    
    // Temperature
    out[11] = (uint8_t)info->imu_temp;
    
    // byte 12..15 保留
}

/*============================================================================
 * 简化四元数压缩 (占位实现)
 *============================================================================*/

uint32_t compress_quat_simple(int16_t qw, int16_t qx, int16_t qy, int16_t qz)
{
    // v0.6.2: 实现完整的smallest-three压缩
    // 原理: 四元数满足 w^2+x^2+y^2+z^2=1，只需传3个分量
    // 找出绝对值最大的分量，传输另外3个
    
    int16_t q[4] = {qw, qx, qy, qz};
    uint8_t largest_idx = 0;
    int16_t largest_val = (qw < 0) ? -qw : qw;
    
    for (int i = 1; i < 4; i++) {
        int16_t abs_val = (q[i] < 0) ? -q[i] : q[i];
        if (abs_val > largest_val) {
            largest_val = abs_val;
            largest_idx = i;
        }
    }
    
    // 如果最大分量为负，翻转整个四元数（保持等价）
    int16_t sign = (q[largest_idx] < 0) ? -1 : 1;
    
    // 提取3个较小分量 (每个10bit, 范围-512~511)
    int16_t small[3];
    int j = 0;
    for (int i = 0; i < 4; i++) {
        if (i != largest_idx) {
            // 缩放到10bit: q[i] 范围是 -32768~32767, 压缩到 -512~511
            int32_t scaled = ((int32_t)(q[i] * sign) * 512) / 32768;
            if (scaled > 511) scaled = 511;
            if (scaled < -512) scaled = -512;
            small[j++] = (int16_t)scaled;
        }
    }
    
    // 打包: [2bit index][10bit a][10bit b][10bit c] = 32bit
    uint32_t packed = 0;
    packed |= ((uint32_t)largest_idx & 0x3) << 30;
    packed |= ((uint32_t)(small[0] + 512) & 0x3FF) << 20;
    packed |= ((uint32_t)(small[1] + 512) & 0x3FF) << 10;
    packed |= ((uint32_t)(small[2] + 512) & 0x3FF);
    
    return packed;
}

/*============================================================================
 * 从RF包转换
 *============================================================================*/

void slime_convert_from_rf_packet(slime_tracker_data_t *out, 
                                   const rf_tracker_packet_t *rf_pkt,
                                   int8_t rssi)
{
    out->tracker_id = rf_pkt->tracker_id;
    out->sequence = rf_pkt->sequence;
    
    // 四元数 (CH格式: w,x,y,z)
    out->quat_w = rf_pkt->quat_w;
    out->quat_x = rf_pkt->quat_x;
    out->quat_y = rf_pkt->quat_y;
    out->quat_z = rf_pkt->quat_z;
    
    // 加速度 (mg)
    out->accel_x = rf_pkt->accel_x;
    out->accel_y = rf_pkt->accel_y;
    out->accel_z = rf_pkt->accel_z;
    
    // 状态
    out->battery_pct = rf_pkt->battery;
    out->flags = rf_pkt->flags;
    out->rssi = rssi;
}

/*============================================================================
 * Packet发送节奏建议
 *============================================================================*/

/**
 * 推荐的包发送节奏:
 * - packet1: 高频 (100~200Hz)，运动时
 * - packet2: 可选高频，替代packet1
 * - packet3: 低频 (1~5Hz) 或状态变化时立即发
 * - packet0: 低频 (0.2~1Hz) 或连接建立时发一次
 */

// 状态包计数器 (用于低频发送)
static uint8_t status_send_counter = 0;

bool slime_should_send_status(void)
{
    status_send_counter++;
    // 每40帧发送一次状态包 (200Hz下约5Hz)
    if (status_send_counter >= 40) {
        status_send_counter = 0;
        return true;
    }
    return false;
}

// 设备信息包计数器
static uint16_t info_send_counter = 0;

bool slime_should_send_info(void)
{
    info_send_counter++;
    // 每200帧发送一次设备信息 (200Hz下约1Hz)
    if (info_send_counter >= 200) {
        info_send_counter = 0;
        return true;
    }
    return false;
}
