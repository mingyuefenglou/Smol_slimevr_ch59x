/**
 * @file rf_protocol.h
 * @brief SlimeVR Private RF Protocol Definitions
 * 
 * This file defines the private RF communication protocol for SlimeVR
 * trackers using CH59X proprietary RF mode. The protocol implements
 * TDMA-based multi-device communication with frequency hopping.
 */

#ifndef __RF_PROTOCOL_H__
#define __RF_PROTOCOL_H__

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Protocol Constants
 *============================================================================*/

// System limits
#define RF_MAX_TRACKERS             MAX_TRACKERS
#define RF_MIN_TRACKERS             6
#define RF_CHANNEL_COUNT            40
#define RF_SYNC_WORD                0x534C5652  // "SLVR"

// Timing (in microseconds)
#define RF_SUPERFRAME_US            5000    // 5ms superframe = 200Hz
#define RF_SYNC_SLOT_US             250     // Sync beacon duration
#define RF_DATA_SLOT_US             400     // Per-tracker slot
#define RF_GUARD_TIME_US            200     // Guard interval
#define RF_HOP_TIME_US              50      // Channel switch time
#define RF_TX_TIME_US               300     // Data transmission
#define RF_ACK_TIME_US              50      // ACK response

// Packet sizes
#define RF_PREAMBLE_SIZE            1
#define RF_SYNCWORD_SIZE            4
#define RF_CRC_SIZE                 2
#define RF_MAX_PAYLOAD_SIZE         32

// Channels
#define RF_PAIRING_CHANNEL          37      // Fixed pairing channel
#define RF_BASE_FREQ_MHZ            2402    // Base frequency
#define RF_CHANNEL_STEP_MHZ         2       // Channel spacing

// Timeouts
#define RF_SYNC_TIMEOUT_MS          100     // Sync loss timeout
#define RF_PAIRING_TIMEOUT_MS       30000   // 30 second pairing window
#define RF_INACTIVE_TIMEOUT_MS      30000   // Sleep after 30s inactive

/*============================================================================
 * Packet Types
 *============================================================================*/

typedef enum {
    RF_PKT_SYNC_BEACON      = 0x01,     // Receiver sync broadcast
    RF_PKT_SYNC_PAIRING     = 0x02,     // Receiver in pairing mode
    RF_PKT_TRACKER_DATA     = 0x10,     // Tracker sensor data
    RF_PKT_TRACKER_STATUS   = 0x11,     // Tracker status only
    RF_PKT_PAIR_REQUEST     = 0x20,     // Tracker pairing request
    RF_PKT_PAIR_RESPONSE    = 0x21,     // Receiver pairing response
    RF_PKT_PAIR_CONFIRM     = 0x22,     // Tracker pairing confirm
    RF_PKT_ACK              = 0x30,     // Acknowledgment
    RF_PKT_COMMAND          = 0x40,     // Command to tracker
} rf_packet_type_t;

/*============================================================================
 * Packet Structures
 *============================================================================*/

// Common packet header
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
} rf_header_t;

// Sync beacon packet (Receiver → All)
typedef struct __attribute__((packed)) {
    rf_header_t header;
    uint16_t frame_number;          // Frame counter for sync
    uint8_t active_mask[2];         // Bitmask of active trackers
    uint8_t channel_map[5];         // Next 5 channels for hopping
    uint8_t tx_power;               // Current TX power level
    uint16_t crc;
} rf_sync_packet_t;

// Tracker data packet (Tracker → Receiver)
typedef struct __attribute__((packed)) {
    rf_header_t header;
    uint8_t tracker_id;             // Assigned tracker ID (0-9)
    uint8_t sequence;               // Sequence number
    int16_t quat_w;                 // Quaternion W * 32767
    int16_t quat_x;                 // Quaternion X * 32767
    int16_t quat_y;                 // Quaternion Y * 32767
    int16_t quat_z;                 // Quaternion Z * 32767
    int16_t accel_x;                // Accel X in mg
    int16_t accel_y;                // Accel Y in mg
    int16_t accel_z;                // Accel Z in mg
    uint8_t battery;                // Battery level 0-100
    uint8_t flags;                  // Status flags
    uint16_t crc;
} rf_tracker_packet_t;

// ACK packet (Receiver → Tracker)
typedef struct __attribute__((packed)) {
    rf_header_t header;
    uint8_t tracker_id;
    uint8_t ack_sequence;
    uint8_t command;                // Optional command (0 = none)
    uint8_t command_data;           // Command parameter
    uint16_t crc;
} rf_ack_packet_t;

// Pairing request (Tracker → Receiver)
typedef struct __attribute__((packed)) {
    rf_header_t header;
    uint8_t mac_address[6];         // Tracker MAC address
    uint8_t device_type;            // Device type identifier
    uint8_t firmware_version[2];    // Major.Minor
    uint16_t crc;
} rf_pair_request_t;

// Pairing response (Receiver → Tracker)
typedef struct __attribute__((packed)) {
    rf_header_t header;
    uint8_t mac_address[6];         // Target tracker MAC
    uint8_t tracker_id;             // Assigned ID (0-9)
    uint8_t receiver_mac[6];        // Receiver MAC for verification
    uint32_t network_key;           // Network key for hopping
    uint16_t crc;
} rf_pair_response_t;

// Pairing confirmation (Tracker → Receiver)
typedef struct __attribute__((packed)) {
    rf_header_t header;
    uint8_t tracker_id;
    uint8_t mac_address[6];
    uint8_t status;                 // 0 = success
    uint16_t crc;
} rf_pair_confirm_t;

/*============================================================================
 * Status Flags
 * v0.4.22: 扩展flags以对齐nRF Smol Slime packet3语义
 *============================================================================*/

#define RF_FLAG_CHARGING        (1 << 0)  // 正在充电
#define RF_FLAG_LOW_BATTERY     (1 << 1)  // 电量低
#define RF_FLAG_CALIBRATING     (1 << 2)  // 校准中
#define RF_FLAG_MAG_ENABLED     (1 << 3)  // 磁力计已启用
#define RF_FLAG_IMU_ERROR       (1 << 4)  // IMU错误 (v0.4.22新增)
#define RF_FLAG_RF_LOST         (1 << 5)  // RF连接丢失 (v0.4.22新增)
#define RF_FLAG_STATIONARY      (1 << 6)  // 静止状态 (v0.4.22新增)
#define RF_FLAG_ERROR           (1 << 7)  // 通用错误

/*============================================================================
 * Server Status (对齐nRF packet3.server_status)
 *============================================================================*/
typedef enum {
    SERVER_STATUS_OK        = 0,    // 正常
    SERVER_STATUS_CALIB     = 1,    // 校准中
    SERVER_STATUS_ERROR     = 2,    // 错误
} server_status_t;

/*============================================================================
 * Command Codes
 *============================================================================*/

typedef enum {
    RF_CMD_NONE             = 0x00,
    RF_CMD_CALIBRATE_GYRO   = 0x01,
    RF_CMD_CALIBRATE_ACCEL  = 0x02,
    RF_CMD_CALIBRATE_MAG    = 0x03,
    RF_CMD_TARE             = 0x04,
    RF_CMD_RESET            = 0x05,
    RF_CMD_SLEEP            = 0x06,
    RF_CMD_WAKE             = 0x07,
    RF_CMD_SET_POWER        = 0x10,
    RF_CMD_UNPAIR           = 0xFF,
} rf_command_t;

/*============================================================================
 * State Definitions
 *============================================================================*/

// Receiver states
typedef enum {
    RX_STATE_INIT,
    RX_STATE_IDLE,
    RX_STATE_PAIRING,
    RX_STATE_RUNNING,
    RX_STATE_ERROR,
} rx_state_t;

// Tracker states
typedef enum {
    TX_STATE_INIT,
    TX_STATE_UNPAIRED,
    TX_STATE_PAIRING,
    TX_STATE_SEARCHING,
    TX_STATE_SYNCED,
    TX_STATE_RUNNING,
    TX_STATE_SLEEP,
    TX_STATE_ERROR,
} tx_state_t;

/*============================================================================
 * Tracker Info Structure
 *============================================================================*/

typedef struct {
    bool active;                    // Tracker is paired and active
    bool connected;                 // Currently receiving data
    uint8_t mac_address[6];         // MAC address
    uint8_t last_sequence;          // Last received sequence
    uint32_t last_seen_ms;          // Last packet timestamp
    uint8_t rssi;                   // Signal strength
    uint8_t packet_loss;            // Packet loss percentage
    uint8_t battery;                // Battery level
    uint8_t flags;                  // Status flags
    
    // Latest sensor data
    float quaternion[4];
    float acceleration[3];
    
    // v0.4.23: 详细统计信息 (丢包/重传/超时)
    uint32_t total_packets;         // 总接收包数
    uint32_t lost_packets;          // 丢失包数
    uint32_t retransmit_count;      // 重传次数
    uint32_t timeout_count;         // 超时次数
    uint32_t crc_error_count;       // CRC错误次数
    uint8_t loss_rate_pct;          // 丢包率百分比 (滑动平均)
} tracker_info_t;

/*============================================================================
 * Receiver Context
 *============================================================================*/

typedef struct {
    rx_state_t state;
    uint16_t frame_number;
    uint8_t current_channel;
    uint32_t network_key;
    uint8_t paired_count;
    tracker_info_t trackers[RF_MAX_TRACKERS];
    
    // Timing
    uint32_t superframe_start_us;
    uint32_t last_sync_ms;
    
    // Channel quality
    uint8_t channel_quality[RF_CHANNEL_COUNT];
    uint8_t channel_blacklist[RF_CHANNEL_COUNT / 8 + 1];
    
    // Statistics
    uint32_t total_packets;
    uint32_t lost_packets;
} rf_receiver_ctx_t;

/*============================================================================
 * Transmitter Context
 *============================================================================*/

typedef struct {
    tx_state_t state;
    uint8_t tracker_id;
    uint8_t mac_address[6];
    uint8_t receiver_mac[6];
    uint32_t network_key;
    bool paired;
    
    // Sync
    uint16_t frame_number;
    uint8_t current_channel;
    uint32_t sync_time_us;
    uint32_t last_sync_ms;
    
    // Transmission
    uint8_t sequence;
    uint8_t pending_ack;
    uint8_t retry_count;
    
    // Sensor data
    float quaternion[4];
    float acceleration[3];
    uint8_t battery;
    uint8_t flags;
} rf_transmitter_ctx_t;

/*============================================================================
 * Callback Types
 *============================================================================*/

typedef void (*rf_rx_data_callback_t)(uint8_t tracker_id, const rf_tracker_packet_t *data);
typedef void (*rf_rx_connect_callback_t)(uint8_t tracker_id, bool connected);
typedef void (*rf_tx_sync_callback_t)(uint16_t frame_number);
typedef void (*rf_tx_ack_callback_t)(uint8_t sequence, bool success);

/*============================================================================
 * API Functions - Common
 *============================================================================*/

/**
 * @brief Calculate CRC16 for packet
 */
uint16_t rf_calc_crc16(const void *data, uint16_t len);

/**
 * @brief Get channel frequency for channel index
 */
uint32_t rf_get_channel_freq(uint8_t channel);

/**
 * @brief Calculate next hop channel
 */
uint8_t rf_get_hop_channel(uint16_t frame_number, uint32_t network_key);

/*============================================================================
 * API Functions - Receiver
 *============================================================================*/

/**
 * @brief Initialize receiver
 */
int rf_receiver_init(rf_receiver_ctx_t *ctx);

/**
 * @brief Start normal operation
 */
int rf_receiver_start(rf_receiver_ctx_t *ctx);

/**
 * @brief Enter pairing mode
 */
int rf_receiver_start_pairing(rf_receiver_ctx_t *ctx);

/**
 * @brief Exit pairing mode
 */
void rf_receiver_stop_pairing(rf_receiver_ctx_t *ctx);

/**
 * @brief Process receiver (call from main loop or timer ISR)
 */
void rf_receiver_process(rf_receiver_ctx_t *ctx);

/**
 * @brief Send command to tracker
 */
int rf_receiver_send_command(rf_receiver_ctx_t *ctx, uint8_t tracker_id, 
                              rf_command_t cmd, uint8_t param);

/**
 * @brief Unpair specific tracker
 */
int rf_receiver_unpair(rf_receiver_ctx_t *ctx, uint8_t tracker_id);

/**
 * @brief Unpair all trackers
 */
void rf_receiver_unpair_all(rf_receiver_ctx_t *ctx);

/**
 * @brief Set data callback
 */
void rf_receiver_set_data_callback(rf_rx_data_callback_t cb);

/**
 * @brief Set connection callback
 */
void rf_receiver_set_connect_callback(rf_rx_connect_callback_t cb);

/*============================================================================
 * API Functions - Transmitter
 *============================================================================*/

/**
 * @brief Initialize transmitter
 */
int rf_transmitter_init(rf_transmitter_ctx_t *ctx);

/**
 * @brief Start transmitter (search and sync)
 */
int rf_transmitter_start(rf_transmitter_ctx_t *ctx);

/**
 * @brief Request pairing
 */
int rf_transmitter_request_pairing(rf_transmitter_ctx_t *ctx);

/**
 * @brief Update sensor data
 */
void rf_transmitter_set_data(rf_transmitter_ctx_t *ctx,
                              const float quat[4],
                              const float accel[3],
                              uint8_t battery,
                              uint8_t flags);

/**
 * @brief Process transmitter (call from main loop or timer ISR)
 */
void rf_transmitter_process(rf_transmitter_ctx_t *ctx);

/**
 * @brief Enter sleep mode
 */
void rf_transmitter_sleep(rf_transmitter_ctx_t *ctx);

/**
 * @brief Wake from sleep
 */
void rf_transmitter_wake(rf_transmitter_ctx_t *ctx);

/**
 * @brief Set sync callback
 */
void rf_transmitter_set_sync_callback(rf_tx_sync_callback_t cb);

/**
 * @brief Set ACK callback
 */
void rf_transmitter_set_ack_callback(rf_tx_ack_callback_t cb);

#ifdef __cplusplus
}
#endif

#endif /* __RF_PROTOCOL_H__ */
