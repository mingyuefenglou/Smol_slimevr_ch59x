/**
 * @file ble_slimevr.h
 * @brief BLE SlimeVR Service Header
 * 
 * Defines the BLE GATT service for SlimeVR tracker communication.
 * This replaces the ESB protocol from the original nRF implementation.
 */

#ifndef __BLE_SLIMEVR_H__
#define __BLE_SLIMEVR_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Service UUIDs
 *============================================================================*/

// SlimeVR Service: 0000FFF0-0000-1000-8000-00805F9B34FB
#define SLIMEVR_SERVICE_UUID                0xFFF0

// Characteristics
#define SLIMEVR_CHAR_SENSOR_DATA_UUID       0xFFF1  // Quaternion + Accel (Notify)
#define SLIMEVR_CHAR_BATTERY_UUID           0xFFF2  // Battery level (Read, Notify)
#define SLIMEVR_CHAR_STATUS_UUID            0xFFF3  // Tracker status (Read, Notify)
#define SLIMEVR_CHAR_CONFIG_UUID            0xFFF4  // Configuration (Read, Write)
#define SLIMEVR_CHAR_COMMAND_UUID           0xFFF5  // Commands (Write)
#define SLIMEVR_CHAR_MAG_DATA_UUID          0xFFF6  // Magnetometer data (Notify)

/*============================================================================
 * Data Structures
 *============================================================================*/

// Sensor data packet (20 bytes - fits in single BLE notification)
typedef struct __attribute__((packed)) {
    int16_t quat_w;     // Quaternion W * 32767
    int16_t quat_x;     // Quaternion X * 32767
    int16_t quat_y;     // Quaternion Y * 32767
    int16_t quat_z;     // Quaternion Z * 32767
    int16_t accel_x;    // Accel X * 1000 (mg)
    int16_t accel_y;    // Accel Y * 1000 (mg)
    int16_t accel_z;    // Accel Z * 1000 (mg)
    uint16_t timestamp; // Timestamp (ms, wrapping)
    uint8_t flags;      // Status flags
    uint8_t reserved;
} ble_sensor_data_t;

// Battery data packet (4 bytes)
typedef struct __attribute__((packed)) {
    uint8_t level;          // Battery percentage (0-100)
    uint8_t charging;       // Charging status
    uint16_t voltage_mv;    // Battery voltage in mV
} ble_battery_data_t;

// Status packet (8 bytes)
typedef struct __attribute__((packed)) {
    uint8_t tracker_id;
    uint8_t imu_type;
    uint8_t mag_type;
    uint8_t fusion_type;
    int8_t temperature;     // IMU temperature in Celsius
    uint8_t error_code;
    uint16_t uptime_sec;
} ble_status_data_t;

// Configuration packet (16 bytes)
typedef struct __attribute__((packed)) {
    uint8_t tracker_id;
    uint8_t fusion_type;
    uint8_t imu_odr;        // ODR index
    uint8_t mag_enabled;
    int8_t tx_power;        // dBm
    uint8_t led_mode;
    uint16_t sleep_timeout;
    uint8_t reserved[8];
} ble_config_data_t;

// Command codes
typedef enum {
    BLE_CMD_RESET           = 0x01,
    BLE_CMD_CALIBRATE_GYRO  = 0x02,
    BLE_CMD_CALIBRATE_ACCEL = 0x03,
    BLE_CMD_CALIBRATE_MAG   = 0x04,
    BLE_CMD_TARE            = 0x05,
    BLE_CMD_SET_MOUNTING    = 0x06,
    BLE_CMD_ENTER_DFU       = 0x10,
    BLE_CMD_FACTORY_RESET   = 0xFF,
} ble_command_t;

// Connection status
typedef enum {
    BLE_STATUS_DISCONNECTED,
    BLE_STATUS_ADVERTISING,
    BLE_STATUS_CONNECTED,
    BLE_STATUS_BONDED,
} ble_conn_status_t;

/*============================================================================
 * Callback Definitions
 *============================================================================*/

typedef void (*ble_connect_cb_t)(void);
typedef void (*ble_disconnect_cb_t)(void);
typedef void (*ble_command_cb_t)(uint8_t cmd, const uint8_t *data, uint8_t len);
typedef void (*ble_config_cb_t)(const ble_config_data_t *config);

/*============================================================================
 * Public API
 *============================================================================*/

/**
 * @brief Initialize BLE stack and SlimeVR service
 * @return 0 on success, negative on error
 */
int ble_slimevr_init(void);

/**
 * @brief Start BLE advertising
 * @return 0 on success, negative on error
 */
int ble_slimevr_start_advertising(void);

/**
 * @brief Stop BLE advertising
 */
void ble_slimevr_stop_advertising(void);

/**
 * @brief Disconnect current connection
 */
void ble_slimevr_disconnect(void);

/**
 * @brief Get connection status
 * @return Current connection status
 */
ble_conn_status_t ble_slimevr_get_status(void);

/**
 * @brief Check if notifications are enabled
 * @return true if connected and notifications enabled
 */
bool ble_slimevr_can_notify(void);

/**
 * @brief Send sensor data notification
 * @param quat Quaternion [w, x, y, z]
 * @param accel Linear acceleration [x, y, z] in g
 * @return 0 on success, negative on error
 */
int ble_slimevr_send_sensor_data(const float quat[4], const float accel[3]);

/**
 * @brief Send magnetometer data notification
 * @param mag Magnetometer reading [x, y, z] in uT
 * @return 0 on success, negative on error
 */
int ble_slimevr_send_mag_data(const float mag[3]);

/**
 * @brief Update battery level
 * @param level Battery percentage (0-100)
 * @param charging true if charging
 * @param voltage_mv Battery voltage in millivolts
 */
void ble_slimevr_update_battery(uint8_t level, bool charging, uint16_t voltage_mv);

/**
 * @brief Update tracker status
 * @param status Pointer to status data
 */
void ble_slimevr_update_status(const ble_status_data_t *status);

/**
 * @brief Register connection callback
 */
void ble_slimevr_set_connect_callback(ble_connect_cb_t cb);

/**
 * @brief Register disconnection callback
 */
void ble_slimevr_set_disconnect_callback(ble_disconnect_cb_t cb);

/**
 * @brief Register command callback
 */
void ble_slimevr_set_command_callback(ble_command_cb_t cb);

/**
 * @brief Register configuration change callback
 */
void ble_slimevr_set_config_callback(ble_config_cb_t cb);

/**
 * @brief Set device name
 * @param name Device name (max 16 chars)
 */
void ble_slimevr_set_device_name(const char *name);

/**
 * @brief Set TX power
 * @param dbm TX power in dBm
 */
void ble_slimevr_set_tx_power(int8_t dbm);

/**
 * @brief Process BLE events (call from main loop)
 */
void ble_slimevr_process(void);

#ifdef __cplusplus
}
#endif

#endif /* __BLE_SLIMEVR_H__ */
