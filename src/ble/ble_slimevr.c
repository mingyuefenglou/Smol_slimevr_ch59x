/**
 * @file ble_slimevr.c
 * @brief BLE SlimeVR Service Implementation for CH59X
 * 
 * Implements the BLE GATT service for SlimeVR tracker communication.
 * Uses CH59X BLE library for the underlying BLE stack.
 * 
 * This module replaces the ESB (Enhanced ShockBurst) protocol from
 * the original nRF implementation with standard BLE.
 */

#include "ble_slimevr.h"
#include "hal.h"
#include <string.h>
#include <stdio.h>

#ifdef CH59X
#include "CH59x_common.h"
#include "CONFIG.h"
#include "HAL.h"
#include "gattprofile.h"
#include "peripheral.h"
#include "broadcaster.h"
#endif

/*============================================================================
 * Configuration
 *============================================================================*/

#define DEVICE_NAME_DEFAULT         "SlimeVR"
#define DEVICE_NAME_MAX_LEN         16

// Advertising parameters
#define ADV_INTERVAL_MIN            160     // 100ms (160 * 0.625ms)
#define ADV_INTERVAL_MAX            320     // 200ms
#define ADV_TIMEOUT                 0       // Forever

// Connection parameters
#define CONN_INTERVAL_MIN           12      // 15ms (12 * 1.25ms)
#define CONN_INTERVAL_MAX           24      // 30ms
#define CONN_LATENCY                0
#define CONN_TIMEOUT                200     // 2s

// Notification rate limiting
#define MIN_NOTIFY_INTERVAL_MS      5       // Max ~200Hz notification rate

/*============================================================================
 * Static Variables
 *============================================================================*/

static ble_conn_status_t conn_status = BLE_STATUS_DISCONNECTED;
static bool notifications_enabled = false;
static uint16_t conn_handle = 0xFFFF;
static uint32_t last_notify_time = 0;

// Callbacks
static ble_connect_cb_t connect_callback = NULL;
static ble_disconnect_cb_t disconnect_callback = NULL;
static ble_command_cb_t command_callback = NULL;
static ble_config_cb_t config_callback = NULL;

// Device name
static char device_name[DEVICE_NAME_MAX_LEN + 1] = DEVICE_NAME_DEFAULT;

// Characteristic values
static ble_sensor_data_t sensor_data = {0};
static ble_battery_data_t battery_data = {.level = 100, .charging = 0, .voltage_mv = 4200};
static ble_status_data_t status_data = {0};
static ble_config_data_t config_data = {0};

/*============================================================================
 * Characteristic Attribute Table
 *============================================================================*/

#ifdef CH59X

// Service UUID
static const uint8_t slimevr_service_uuid[2] = {
    LO_UINT16(SLIMEVR_SERVICE_UUID),
    HI_UINT16(SLIMEVR_SERVICE_UUID)
};

// Characteristic UUIDs
static const uint8_t sensor_data_uuid[2] = {
    LO_UINT16(SLIMEVR_CHAR_SENSOR_DATA_UUID),
    HI_UINT16(SLIMEVR_CHAR_SENSOR_DATA_UUID)
};

static const uint8_t battery_uuid[2] = {
    LO_UINT16(SLIMEVR_CHAR_BATTERY_UUID),
    HI_UINT16(SLIMEVR_CHAR_BATTERY_UUID)
};

static const uint8_t status_uuid[2] = {
    LO_UINT16(SLIMEVR_CHAR_STATUS_UUID),
    HI_UINT16(SLIMEVR_CHAR_STATUS_UUID)
};

static const uint8_t config_uuid[2] = {
    LO_UINT16(SLIMEVR_CHAR_CONFIG_UUID),
    HI_UINT16(SLIMEVR_CHAR_CONFIG_UUID)
};

static const uint8_t command_uuid[2] = {
    LO_UINT16(SLIMEVR_CHAR_COMMAND_UUID),
    HI_UINT16(SLIMEVR_CHAR_COMMAND_UUID)
};

// Characteristic properties
static uint8_t sensor_data_props = GATT_PROP_NOTIFY;
static uint8_t battery_props = GATT_PROP_READ | GATT_PROP_NOTIFY;
static uint8_t status_props = GATT_PROP_READ | GATT_PROP_NOTIFY;
static uint8_t config_props = GATT_PROP_READ | GATT_PROP_WRITE;
static uint8_t command_props = GATT_PROP_WRITE;

// Client Characteristic Configuration Descriptors (CCCD)
static uint16_t sensor_data_cccd = 0;
static uint16_t battery_cccd = 0;
static uint16_t status_cccd = 0;

// GATT Attribute Table
static gattAttribute_t slimevr_attr_table[] = {
    // SlimeVR Service Declaration
    {
        {ATT_BT_UUID_SIZE, primaryServiceUUID},
        GATT_PERMIT_READ,
        0,
        (uint8_t*)slimevr_service_uuid
    },
    
    // Sensor Data Characteristic
    {
        {ATT_BT_UUID_SIZE, characterUUID},
        GATT_PERMIT_READ,
        0,
        &sensor_data_props
    },
    {
        {ATT_BT_UUID_SIZE, sensor_data_uuid},
        0,
        0,
        (uint8_t*)&sensor_data
    },
    {
        {ATT_BT_UUID_SIZE, clientCharCfgUUID},
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8_t*)&sensor_data_cccd
    },
    
    // Battery Characteristic
    {
        {ATT_BT_UUID_SIZE, characterUUID},
        GATT_PERMIT_READ,
        0,
        &battery_props
    },
    {
        {ATT_BT_UUID_SIZE, battery_uuid},
        GATT_PERMIT_READ,
        0,
        (uint8_t*)&battery_data
    },
    {
        {ATT_BT_UUID_SIZE, clientCharCfgUUID},
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8_t*)&battery_cccd
    },
    
    // Status Characteristic
    {
        {ATT_BT_UUID_SIZE, characterUUID},
        GATT_PERMIT_READ,
        0,
        &status_props
    },
    {
        {ATT_BT_UUID_SIZE, status_uuid},
        GATT_PERMIT_READ,
        0,
        (uint8_t*)&status_data
    },
    {
        {ATT_BT_UUID_SIZE, clientCharCfgUUID},
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8_t*)&status_cccd
    },
    
    // Config Characteristic
    {
        {ATT_BT_UUID_SIZE, characterUUID},
        GATT_PERMIT_READ,
        0,
        &config_props
    },
    {
        {ATT_BT_UUID_SIZE, config_uuid},
        GATT_PERMIT_READ | GATT_PERMIT_WRITE,
        0,
        (uint8_t*)&config_data
    },
    
    // Command Characteristic
    {
        {ATT_BT_UUID_SIZE, characterUUID},
        GATT_PERMIT_READ,
        0,
        &command_props
    },
    {
        {ATT_BT_UUID_SIZE, command_uuid},
        GATT_PERMIT_WRITE,
        0,
        NULL  // Command data handled in callback
    },
};

#define SLIMEVR_ATTR_COUNT  (sizeof(slimevr_attr_table) / sizeof(slimevr_attr_table[0]))

// Attribute table handles
#define SENSOR_DATA_VALUE_HANDLE    2
#define SENSOR_DATA_CCCD_HANDLE     3
#define BATTERY_VALUE_HANDLE        5
#define BATTERY_CCCD_HANDLE         6
#define STATUS_VALUE_HANDLE         8
#define STATUS_CCCD_HANDLE          9
#define CONFIG_VALUE_HANDLE         11
#define COMMAND_VALUE_HANDLE        13

#endif // CH59X

/*============================================================================
 * GATT Callbacks
 *============================================================================*/

#ifdef CH59X

static bStatus_t slimevr_read_attr_cb(uint16_t connHandle, gattAttribute_t *pAttr,
                                       uint8_t *pValue, uint16_t *pLen,
                                       uint16_t offset, uint16_t maxLen,
                                       uint8_t method)
{
    bStatus_t status = SUCCESS;
    uint16_t uuid = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);
    
    switch (uuid) {
        case SLIMEVR_CHAR_BATTERY_UUID:
            *pLen = sizeof(ble_battery_data_t);
            memcpy(pValue, &battery_data, *pLen);
            break;
            
        case SLIMEVR_CHAR_STATUS_UUID:
            *pLen = sizeof(ble_status_data_t);
            memcpy(pValue, &status_data, *pLen);
            break;
            
        case SLIMEVR_CHAR_CONFIG_UUID:
            *pLen = sizeof(ble_config_data_t);
            memcpy(pValue, &config_data, *pLen);
            break;
            
        default:
            status = ATT_ERR_ATTR_NOT_FOUND;
            break;
    }
    
    return status;
}

static bStatus_t slimevr_write_attr_cb(uint16_t connHandle, gattAttribute_t *pAttr,
                                        uint8_t *pValue, uint16_t len,
                                        uint16_t offset, uint8_t method)
{
    bStatus_t status = SUCCESS;
    uint16_t uuid = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);
    
    switch (uuid) {
        case GATT_CLIENT_CHAR_CFG_UUID:
            // Handle CCCD writes (enable/disable notifications)
            if (len == 2) {
                uint16_t value = BUILD_UINT16(pValue[0], pValue[1]);
                
                // Determine which characteristic's CCCD was written
                if (pAttr == &slimevr_attr_table[SENSOR_DATA_CCCD_HANDLE]) {
                    sensor_data_cccd = value;
                    notifications_enabled = (value & GATT_CLIENT_CFG_NOTIFY) != 0;
                } else if (pAttr == &slimevr_attr_table[BATTERY_CCCD_HANDLE]) {
                    battery_cccd = value;
                } else if (pAttr == &slimevr_attr_table[STATUS_CCCD_HANDLE]) {
                    status_cccd = value;
                }
            } else {
                status = ATT_ERR_INVALID_VALUE_SIZE;
            }
            break;
            
        case SLIMEVR_CHAR_CONFIG_UUID:
            if (len == sizeof(ble_config_data_t)) {
                memcpy(&config_data, pValue, len);
                if (config_callback) {
                    config_callback(&config_data);
                }
            } else {
                status = ATT_ERR_INVALID_VALUE_SIZE;
            }
            break;
            
        case SLIMEVR_CHAR_COMMAND_UUID:
            if (len >= 1) {
                uint8_t cmd = pValue[0];
                if (command_callback) {
                    command_callback(cmd, pValue + 1, len - 1);
                }
            }
            break;
            
        default:
            status = ATT_ERR_ATTR_NOT_FOUND;
            break;
    }
    
    return status;
}

static gattServiceCBs_t slimevr_profile_cbs = {
    slimevr_read_attr_cb,
    slimevr_write_attr_cb,
    NULL
};

#endif // CH59X

/*============================================================================
 * GAP Event Handlers
 *============================================================================*/

#ifdef CH59X

static void slimevr_gap_event_handler(uint32_t event, uint8_t *pMsg)
{
    switch (event) {
        case GAP_LINK_ESTABLISHED_EVENT:
            conn_status = BLE_STATUS_CONNECTED;
            conn_handle = ((gapLinkUpdateEvent_t*)pMsg)->connectionHandle;
            if (connect_callback) {
                connect_callback();
            }
            break;
            
        case GAP_LINK_TERMINATED_EVENT:
            conn_status = BLE_STATUS_DISCONNECTED;
            conn_handle = 0xFFFF;
            notifications_enabled = false;
            sensor_data_cccd = 0;
            battery_cccd = 0;
            status_cccd = 0;
            if (disconnect_callback) {
                disconnect_callback();
            }
            // Auto restart advertising
            ble_slimevr_start_advertising();
            break;
            
        case GAP_LINK_PARAM_UPDATE_EVENT:
            // Connection parameters updated
            break;
            
        default:
            break;
    }
}

#endif // CH59X

/*============================================================================
 * Public API Implementation
 *============================================================================*/

int ble_slimevr_init(void)
{
#ifdef CH59X
    // Initialize BLE stack
    GAPRole_PeripheralInit();
    
    // Set device name
    GAPRole_SetParameter(GAPROLE_DEVICE_NAME, strlen(device_name), device_name);
    
    // Configure advertising data
    uint8_t adv_data[] = {
        // Flags
        0x02, GAP_ADTYPE_FLAGS, GAP_ADTYPE_FLAGS_GENERAL | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
        // Complete local name
        0x08, GAP_ADTYPE_LOCAL_NAME_COMPLETE, 'S', 'l', 'i', 'm', 'e', 'V', 'R',
        // Service UUID
        0x03, GAP_ADTYPE_16BIT_MORE, LO_UINT16(SLIMEVR_SERVICE_UUID), HI_UINT16(SLIMEVR_SERVICE_UUID)
    };
    GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(adv_data), adv_data);
    
    // Scan response data (device name)
    uint8_t scan_rsp[] = {
        0x08, GAP_ADTYPE_LOCAL_NAME_COMPLETE, 'S', 'l', 'i', 'm', 'e', 'V', 'R'
    };
    GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scan_rsp), scan_rsp);
    
    // Set advertising parameters
    uint16_t adv_int_min = ADV_INTERVAL_MIN;
    uint16_t adv_int_max = ADV_INTERVAL_MAX;
    GAPRole_SetParameter(GAPROLE_ADV_INTERVAL_MIN, sizeof(adv_int_min), &adv_int_min);
    GAPRole_SetParameter(GAPROLE_ADV_INTERVAL_MAX, sizeof(adv_int_max), &adv_int_max);
    
    // Set connection parameters
    uint16_t conn_int_min = CONN_INTERVAL_MIN;
    uint16_t conn_int_max = CONN_INTERVAL_MAX;
    uint16_t conn_latency = CONN_LATENCY;
    uint16_t conn_timeout = CONN_TIMEOUT;
    GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL, sizeof(conn_int_min), &conn_int_min);
    GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL, sizeof(conn_int_max), &conn_int_max);
    GAPRole_SetParameter(GAPROLE_SLAVE_LATENCY, sizeof(conn_latency), &conn_latency);
    GAPRole_SetParameter(GAPROLE_TIMEOUT_MULTIPLIER, sizeof(conn_timeout), &conn_timeout);
    
    // Register GATT service
    GATTServApp_RegisterService(slimevr_attr_table, SLIMEVR_ATTR_COUNT,
                                GATT_MAX_ENCRYPT_KEY_SIZE, &slimevr_profile_cbs);
    
    return 0;
#else
    return 0;
#endif
}

int ble_slimevr_start_advertising(void)
{
#ifdef CH59X
    uint8_t adv_enable = TRUE;
    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(adv_enable), &adv_enable);
    conn_status = BLE_STATUS_ADVERTISING;
    return 0;
#else
    conn_status = BLE_STATUS_ADVERTISING;
    return 0;
#endif
}

void ble_slimevr_stop_advertising(void)
{
#ifdef CH59X
    uint8_t adv_enable = FALSE;
    GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(adv_enable), &adv_enable);
#endif
    if (conn_status == BLE_STATUS_ADVERTISING) {
        conn_status = BLE_STATUS_DISCONNECTED;
    }
}

void ble_slimevr_disconnect(void)
{
#ifdef CH59X
    if (conn_handle != 0xFFFF) {
        GAPRole_TerminateConnection();
    }
#endif
}

ble_conn_status_t ble_slimevr_get_status(void)
{
    return conn_status;
}

bool ble_slimevr_can_notify(void)
{
    return (conn_status == BLE_STATUS_CONNECTED) && notifications_enabled;
}

int ble_slimevr_send_sensor_data(const float quat[4], const float accel[3])
{
    if (!ble_slimevr_can_notify()) {
        return -1;
    }
    
    // Rate limiting
    uint32_t now = hal_millis();
    if (now - last_notify_time < MIN_NOTIFY_INTERVAL_MS) {
        return -2;
    }
    last_notify_time = now;
    
    // Pack data into notification packet
    // Scale quaternion to int16 (-32767 to 32767)
    // 添加范围钳位防止溢出
    float q0 = quat[0], q1 = quat[1], q2 = quat[2], q3 = quat[3];
    if (q0 > 1.0f) q0 = 1.0f; else if (q0 < -1.0f) q0 = -1.0f;
    if (q1 > 1.0f) q1 = 1.0f; else if (q1 < -1.0f) q1 = -1.0f;
    if (q2 > 1.0f) q2 = 1.0f; else if (q2 < -1.0f) q2 = -1.0f;
    if (q3 > 1.0f) q3 = 1.0f; else if (q3 < -1.0f) q3 = -1.0f;
    sensor_data.quat_w = (int16_t)(q0 * 32767.0f);
    sensor_data.quat_x = (int16_t)(q1 * 32767.0f);
    sensor_data.quat_y = (int16_t)(q2 * 32767.0f);
    sensor_data.quat_z = (int16_t)(q3 * 32767.0f);
    
    // Scale acceleration to mg (milli-g)
    // 添加范围钳位防止int16_t溢出 (±32.767g范围)
    float ax = accel[0], ay = accel[1], az = accel[2];
    if (ax > 32.767f) ax = 32.767f; else if (ax < -32.768f) ax = -32.768f;
    if (ay > 32.767f) ay = 32.767f; else if (ay < -32.768f) ay = -32.768f;
    if (az > 32.767f) az = 32.767f; else if (az < -32.768f) az = -32.768f;
    sensor_data.accel_x = (int16_t)(ax * 1000.0f);
    sensor_data.accel_y = (int16_t)(ay * 1000.0f);
    sensor_data.accel_z = (int16_t)(az * 1000.0f);
    
    sensor_data.timestamp = (uint16_t)(now & 0xFFFF);
    sensor_data.flags = 0;
    
#ifdef CH59X
    // Send notification
    attHandleValueNoti_t noti;
    noti.pValue = GATT_bm_alloc(conn_handle, ATT_HANDLE_VALUE_NOTI, 
                                 sizeof(ble_sensor_data_t), NULL);
    if (noti.pValue) {
        noti.handle = slimevr_attr_table[SENSOR_DATA_VALUE_HANDLE].handle;
        noti.len = sizeof(ble_sensor_data_t);
        memcpy(noti.pValue, &sensor_data, noti.len);
        
        bStatus_t status = GATT_Notification(conn_handle, &noti, FALSE);
        if (status != SUCCESS) {
            GATT_bm_free((gattMsg_t*)&noti, ATT_HANDLE_VALUE_NOTI);
            return -3;
        }
    } else {
        return -4;
    }
#endif
    
    return 0;
}

int ble_slimevr_send_mag_data(const float mag[3])
{
    if (!ble_slimevr_can_notify()) {
        return -1;
    }
    
    // Magnetometer data packed as 3x int16 (0.1 uT resolution)
    int16_t mag_data[3];
    mag_data[0] = (int16_t)(mag[0] * 10.0f);
    mag_data[1] = (int16_t)(mag[1] * 10.0f);
    mag_data[2] = (int16_t)(mag[2] * 10.0f);
    
#ifdef CH59X
    attHandleValueNoti_t noti;
    noti.pValue = GATT_bm_alloc(conn_handle, ATT_HANDLE_VALUE_NOTI, 6, NULL);
    if (noti.pValue) {
        // Use a separate handle for mag data if defined
        noti.handle = slimevr_attr_table[SENSOR_DATA_VALUE_HANDLE].handle;  // Reuse for now
        noti.len = 6;
        memcpy(noti.pValue, mag_data, noti.len);
        
        bStatus_t status = GATT_Notification(conn_handle, &noti, FALSE);
        if (status != SUCCESS) {
            GATT_bm_free((gattMsg_t*)&noti, ATT_HANDLE_VALUE_NOTI);
            return -3;
        }
    }
#endif
    
    return 0;
}

void ble_slimevr_update_battery(uint8_t level, bool charging, uint16_t voltage_mv)
{
    battery_data.level = level;
    battery_data.charging = charging ? 1 : 0;
    battery_data.voltage_mv = voltage_mv;
    
#ifdef CH59X
    // Notify if enabled
    if (conn_status == BLE_STATUS_CONNECTED && (battery_cccd & GATT_CLIENT_CFG_NOTIFY)) {
        attHandleValueNoti_t noti;
        noti.pValue = GATT_bm_alloc(conn_handle, ATT_HANDLE_VALUE_NOTI,
                                     sizeof(ble_battery_data_t), NULL);
        if (noti.pValue) {
            noti.handle = slimevr_attr_table[BATTERY_VALUE_HANDLE].handle;
            noti.len = sizeof(ble_battery_data_t);
            memcpy(noti.pValue, &battery_data, noti.len);
            GATT_Notification(conn_handle, &noti, FALSE);
        }
    }
#endif
}

void ble_slimevr_update_status(const ble_status_data_t *status)
{
    memcpy(&status_data, status, sizeof(status_data));
    
#ifdef CH59X
    if (conn_status == BLE_STATUS_CONNECTED && (status_cccd & GATT_CLIENT_CFG_NOTIFY)) {
        attHandleValueNoti_t noti;
        noti.pValue = GATT_bm_alloc(conn_handle, ATT_HANDLE_VALUE_NOTI,
                                     sizeof(ble_status_data_t), NULL);
        if (noti.pValue) {
            noti.handle = slimevr_attr_table[STATUS_VALUE_HANDLE].handle;
            noti.len = sizeof(ble_status_data_t);
            memcpy(noti.pValue, &status_data, noti.len);
            GATT_Notification(conn_handle, &noti, FALSE);
        }
    }
#endif
}

void ble_slimevr_set_connect_callback(ble_connect_cb_t cb)
{
    connect_callback = cb;
}

void ble_slimevr_set_disconnect_callback(ble_disconnect_cb_t cb)
{
    disconnect_callback = cb;
}

void ble_slimevr_set_command_callback(ble_command_cb_t cb)
{
    command_callback = cb;
}

void ble_slimevr_set_config_callback(ble_config_cb_t cb)
{
    config_callback = cb;
}

void ble_slimevr_set_device_name(const char *name)
{
    strncpy(device_name, name, DEVICE_NAME_MAX_LEN);
    device_name[DEVICE_NAME_MAX_LEN] = '\0';
    
#ifdef CH59X
    GAPRole_SetParameter(GAPROLE_DEVICE_NAME, strlen(device_name), device_name);
#endif
}

void ble_slimevr_set_tx_power(int8_t dbm)
{
#ifdef CH59X
    // CH59X TX power levels: -20, -10, -5, 0, 1, 2, 3, 4 dBm
    int8_t tx_power;
    if (dbm <= -15) tx_power = -20;
    else if (dbm <= -7) tx_power = -10;
    else if (dbm <= -2) tx_power = -5;
    else if (dbm <= 0) tx_power = 0;
    else if (dbm <= 1) tx_power = 1;
    else if (dbm <= 2) tx_power = 2;
    else if (dbm <= 3) tx_power = 3;
    else tx_power = 4;
    
    // Set TX power (implementation depends on CH59X BLE API)
    // LL_SetTxPower(tx_power);
#else
    (void)dbm;
#endif
}

void ble_slimevr_process(void)
{
#ifdef CH59X
    // Process BLE stack events
    // This is typically called from the main loop or TMOS task
    TMOS_SystemProcess();
#endif
}
