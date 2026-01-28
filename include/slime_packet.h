/**
 * @file slime_packet.h
 * @brief SlimeVR Packet Translation Layer Header
 * 
 * v0.4.22 P1-4: Receiver端协议兼容翻译层
 * 
 * 定义nRF Smol Slime兼容的packet格式和转换函数
 */

#ifndef SLIME_PACKET_H
#define SLIME_PACKET_H

#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * Packet类型定义 (对齐nRF packet0~4)
 *============================================================================*/

#define SLIME_PACKET_SIZE       16      // 每个packet固定16字节

// Packet类型
#define SLIME_PKT_TYPE_INFO         0x00    // packet0: 设备信息
#define SLIME_PKT_TYPE_QUAT_ACCEL   0x01    // packet1: 全精度姿态
#define SLIME_PKT_TYPE_COMPRESSED   0x02    // packet2: 压缩姿态
#define SLIME_PKT_TYPE_STATUS       0x03    // packet3: 状态
#define SLIME_PKT_TYPE_MAG          0x04    // packet4: 磁力计

/*============================================================================
 * Server Status (packet3.byte2)
 *============================================================================*/

typedef enum {
    SLIME_SERVER_OK     = 0,    // 正常
    SLIME_SERVER_CALIB  = 1,    // 校准中
    SLIME_SERVER_ERROR  = 2,    // 错误
} slime_server_status_t;

/*============================================================================
 * Tracker Status Bitfield (packet3.byte3)
 *============================================================================*/

#define SLIME_STATUS_LOW_BATT       (1 << 0)    // 电量低
#define SLIME_STATUS_CHARGING       (1 << 1)    // 充电中
#define SLIME_STATUS_CALIBRATING    (1 << 2)    // 校准中
#define SLIME_STATUS_IMU_ERROR      (1 << 3)    // IMU错误
#define SLIME_STATUS_RF_LOST        (1 << 4)    // RF连接丢失

/*============================================================================
 * 数据结构
 *============================================================================*/

/**
 * @brief Tracker数据 (用于packet1/2/3转换)
 */
typedef struct {
    uint8_t tracker_id;
    uint8_t sequence;
    
    // 四元数 (±32767映射到±1.0)
    int16_t quat_w;
    int16_t quat_x;
    int16_t quat_y;
    int16_t quat_z;
    
    // 加速度 (mg)
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    
    // 状态
    uint8_t battery_pct;    // 0~100
    uint8_t flags;          // RF_FLAG_* bitfield
    int8_t rssi;            // Receiver测量
} slime_tracker_data_t;

/**
 * @brief 设备信息 (用于packet0转换)
 */
typedef struct {
    uint8_t tracker_id;
    
    // 固件版本
    uint8_t fw_version_major;
    uint8_t fw_version_minor;
    
    // 硬件信息
    uint8_t board_id;       // 板型ID
    uint8_t mcu_id;         // MCU类型 (CH591=0x91, CH592=0x92)
    uint8_t imu_id;         // IMU类型
    uint8_t mag_id;         // 磁力计类型 (0=无)
    
    // 电源状态
    uint8_t battery_pct;
    uint16_t battery_mv;
    
    // 温度
    int8_t imu_temp;        // 摄氏度
} slime_device_info_t;

/*============================================================================
 * Packet生成函数
 *============================================================================*/

// 包含rf_protocol.h以使用rf_tracker_packet_t
#include "rf_protocol.h"

/**
 * @brief 生成packet1 (全精度姿态包)
 * @param out 输出缓冲区 (16字节)
 * @param data Tracker数据
 */
void slime_make_packet1(uint8_t out[SLIME_PACKET_SIZE], const slime_tracker_data_t *data);

/**
 * @brief 生成packet2 (压缩姿态包 + 电量/温度/RSSI)
 * @param out 输出缓冲区 (16字节)
 * @param data Tracker数据
 */
void slime_make_packet2(uint8_t out[SLIME_PACKET_SIZE], const slime_tracker_data_t *data);

/**
 * @brief 生成packet3 (状态包)
 * @param out 输出缓冲区 (16字节)
 * @param data Tracker数据
 */
void slime_make_packet3(uint8_t out[SLIME_PACKET_SIZE], const slime_tracker_data_t *data);

/**
 * @brief 生成packet0 (设备信息包)
 * @param out 输出缓冲区 (16字节)
 * @param info 设备信息
 */
void slime_make_packet0(uint8_t out[SLIME_PACKET_SIZE], const slime_device_info_t *info);

/*============================================================================
 * 转换辅助函数
 *============================================================================*/

/**
 * @brief 从RF包转换到slime_tracker_data_t
 * @param out 输出数据
 * @param rf_pkt RF包指针
 * @param rssi 接收信号强度
 */
void slime_convert_from_rf_packet(slime_tracker_data_t *out, 
                                   const rf_tracker_packet_t *rf_pkt,
                                   int8_t rssi);

/**
 * @brief 简化四元数压缩
 * @return 32位压缩四元数
 */
uint32_t compress_quat_simple(int16_t qw, int16_t qx, int16_t qy, int16_t qz);

/*============================================================================
 * 发送节奏控制
 *============================================================================*/

/**
 * @brief 检查是否应该发送状态包 (低频)
 * @return true 应该发送
 */
bool slime_should_send_status(void);

/**
 * @brief 检查是否应该发送设备信息包 (低频)
 * @return true 应该发送
 */
bool slime_should_send_info(void);

#endif // SLIME_PACKET_H
