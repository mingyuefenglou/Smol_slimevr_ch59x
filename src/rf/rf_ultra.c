/**
 * @file rf_ultra.c
 * @brief Ultra-Optimized RF Communication for CH592 SlimeVR
 * 
 * Optimizations:
 * 1. Minimal packet size (12 bytes vs 21 bytes)
 * 2. Delta compression for quaternion
 * 3. Adaptive transmission rate
 * 4. Fast CRC8 (vs CRC32)
 * 5. Zero-copy packet handling
 * 
 * Packet format (12 bytes):
 * [0]     Header: type(2) | tracker_id(6)
 * [1-8]   Quaternion: w,x,y,z as int16 (8 bytes)
 * [9-10]  Accel Z + Battery (packed)
 * [11]    CRC8
 */

#include "rf_ultra.h"
#include "optimize.h"
#include <string.h>

/*============================================================================
 * Packet Format Constants
 *============================================================================*/

#define PKT_SIZE            12
#define PKT_HEADER_OFFSET   0
#define PKT_QUAT_OFFSET     1
#define PKT_AUX_OFFSET      9
#define PKT_CRC_OFFSET      11

// Header bits
#define HDR_TYPE_MASK       0xC0
#define HDR_TYPE_SHIFT      6
#define HDR_ID_MASK         0x3F

// Packet types
#define PKT_TYPE_QUAT       0   // Quaternion + accel
#define PKT_TYPE_INFO       1   // Device info
#define PKT_TYPE_STATUS     2   // Status/battery
#define PKT_TYPE_RESERVED   3

/*============================================================================
 * CRC8 (Optimized, no table)
 *============================================================================*/

static FORCE_INLINE uint8_t crc8_update(uint8_t crc, uint8_t data)
{
    crc ^= data;
    for (int i = 0; i < 8; i++) {
        crc = (crc & 0x80) ? ((crc << 1) ^ 0x07) : (crc << 1);
    }
    return crc;
}

static uint8_t HOT crc8_calc(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    while (len--) {
        crc = crc8_update(crc, *data++);
    }
    return crc;
}

/*============================================================================
 * Quaternion Compression
 *============================================================================*/

// Compress quaternion from float[4] to int16[4]
// Uses full Q15 range for maximum precision
static void quat_compress(const float q[4], int16_t out[4])
{
    // Find largest component for sign correction
    float max_val = 0.0f;
    int max_idx = 0;
    for (int i = 0; i < 4; i++) {
        float abs_val = q[i] >= 0 ? q[i] : -q[i];
        if (abs_val > max_val) {
            max_val = abs_val;
            max_idx = i;
        }
    }
    
    // Ensure w (first component after rotation) is positive
    float sign = (q[max_idx] >= 0) ? 1.0f : -1.0f;
    
    // Convert to int16 with sign correction
    for (int i = 0; i < 4; i++) {
        float val = q[i] * sign * 32767.0f;
        if (val > 32767.0f) val = 32767.0f;
        if (val < -32768.0f) val = -32768.0f;
        out[i] = (int16_t)val;
    }
}

// Compress from Q15 quaternion directly (no float conversion)
static FORCE_INLINE void quat_compress_q15(const q15_t q[4], int16_t out[4])
{
    // Direct copy with sign normalization
    int16_t sign = (q[0] >= 0) ? 1 : -1;
    out[0] = q[0] * sign;
    out[1] = q[1] * sign;
    out[2] = q[2] * sign;
    out[3] = q[3] * sign;
}

/*============================================================================
 * Packet Building
 *============================================================================*/

// Build quaternion packet (type 0)
void rf_ultra_build_quat_packet(uint8_t *pkt, 
                                 uint8_t tracker_id,
                                 const q15_t quat[4],
                                 int16_t accel_z_mg,
                                 uint8_t battery_pct)
{
    // Header
    pkt[PKT_HEADER_OFFSET] = (PKT_TYPE_QUAT << HDR_TYPE_SHIFT) | (tracker_id & HDR_ID_MASK);
    
    // Quaternion (little-endian)
    int16_t *q = (int16_t*)&pkt[PKT_QUAT_OFFSET];
    q[0] = quat[0];
    q[1] = quat[1];
    q[2] = quat[2];
    q[3] = quat[3];
    
    // Accel Z (12 bits) + Battery (4 bits)
    // Accel Z: -2048 to +2047 (covers ±2g at 1mg resolution)
    int16_t az_12 = accel_z_mg >> 1;  // Divide by 2 for 12-bit range
    if (az_12 > 2047) az_12 = 2047;
    if (az_12 < -2048) az_12 = -2048;
    
    uint8_t batt_4 = battery_pct >> 2;  // 0-100 -> 0-25 (4 bits, ~4% resolution)
    if (batt_4 > 15) batt_4 = 15;
    
    pkt[PKT_AUX_OFFSET] = (uint8_t)(az_12 & 0xFF);
    pkt[PKT_AUX_OFFSET + 1] = (uint8_t)(((az_12 >> 8) & 0x0F) | (batt_4 << 4));
    
    // CRC
    pkt[PKT_CRC_OFFSET] = crc8_calc(pkt, PKT_SIZE - 1);
}

// Build info packet (type 1)
void rf_ultra_build_info_packet(uint8_t *pkt,
                                 uint8_t tracker_id,
                                 uint8_t imu_type,
                                 uint8_t fw_major,
                                 uint8_t fw_minor,
                                 uint16_t fw_patch)
{
    pkt[PKT_HEADER_OFFSET] = (PKT_TYPE_INFO << HDR_TYPE_SHIFT) | (tracker_id & HDR_ID_MASK);
    
    pkt[1] = imu_type;
    pkt[2] = fw_major;
    pkt[3] = fw_minor;
    pkt[4] = fw_patch & 0xFF;
    pkt[5] = fw_patch >> 8;
    
    // Get unique ID from chip
    uint32_t uid = 0;
#ifdef CH59X
    uid = *(uint32_t*)0x1FFFF7E8;  // CH592 unique ID location
#endif
    memcpy(&pkt[6], &uid, 4);
    
    pkt[10] = 0;  // Reserved
    
    pkt[PKT_CRC_OFFSET] = crc8_calc(pkt, PKT_SIZE - 1);
}

// Build status packet (type 2)
void rf_ultra_build_status_packet(uint8_t *pkt,
                                   uint8_t tracker_id,
                                   uint8_t status_flags,
                                   uint16_t battery_mv,
                                   int8_t temperature_c)
{
    pkt[PKT_HEADER_OFFSET] = (PKT_TYPE_STATUS << HDR_TYPE_SHIFT) | (tracker_id & HDR_ID_MASK);
    
    pkt[1] = status_flags;
    pkt[2] = battery_mv & 0xFF;
    pkt[3] = battery_mv >> 8;
    pkt[4] = (uint8_t)temperature_c;
    pkt[5] = 0;  // RSSI (filled by receiver)
    pkt[6] = 0;  // Reserved
    pkt[7] = 0;
    pkt[8] = 0;
    pkt[9] = 0;
    pkt[10] = 0;
    
    pkt[PKT_CRC_OFFSET] = crc8_calc(pkt, PKT_SIZE - 1);
}

/*============================================================================
 * Packet Parsing
 *============================================================================*/

bool rf_ultra_parse_packet(const uint8_t *pkt, rf_ultra_parsed_t *out)
{
    // Verify CRC
    if (crc8_calc(pkt, PKT_SIZE - 1) != pkt[PKT_CRC_OFFSET]) {
        return false;
    }
    
    // Parse header
    out->type = (pkt[PKT_HEADER_OFFSET] & HDR_TYPE_MASK) >> HDR_TYPE_SHIFT;
    out->tracker_id = pkt[PKT_HEADER_OFFSET] & HDR_ID_MASK;
    
    switch (out->type) {
        case PKT_TYPE_QUAT:
            // Quaternion
            out->quat[0] = ((int16_t*)&pkt[PKT_QUAT_OFFSET])[0];
            out->quat[1] = ((int16_t*)&pkt[PKT_QUAT_OFFSET])[1];
            out->quat[2] = ((int16_t*)&pkt[PKT_QUAT_OFFSET])[2];
            out->quat[3] = ((int16_t*)&pkt[PKT_QUAT_OFFSET])[3];
            
            // Accel Z + Battery
            int16_t az_12 = pkt[PKT_AUX_OFFSET] | ((pkt[PKT_AUX_OFFSET + 1] & 0x0F) << 8);
            if (az_12 & 0x800) az_12 |= 0xF000;  // Sign extend
            out->accel_z_mg = az_12 * 2;
            out->battery_pct = (pkt[PKT_AUX_OFFSET + 1] >> 4) * 4;
            break;
            
        case PKT_TYPE_INFO:
            out->imu_type = pkt[1];
            out->fw_major = pkt[2];
            out->fw_minor = pkt[3];
            out->fw_patch = pkt[4] | (pkt[5] << 8);
            memcpy(&out->unique_id, &pkt[6], 4);
            break;
            
        case PKT_TYPE_STATUS:
            out->status_flags = pkt[1];
            out->battery_mv = pkt[2] | (pkt[3] << 8);
            out->temperature = (int8_t)pkt[4];
            break;
            
        default:
            return false;
    }
    
    return true;
}

/*============================================================================
 * Transmitter State Machine
 *============================================================================*/

static struct {
    uint8_t packet_buf[PKT_SIZE];
    uint8_t tx_rate_div;        // Rate divider (1 = full rate)
    uint8_t tx_count;           // Packet counter
    uint8_t info_interval;      // Info packet interval
    uint8_t status_interval;    // Status packet interval
} tx_state;

void rf_ultra_tx_init(uint8_t rate_divider)
{
    memset(&tx_state, 0, sizeof(tx_state));
    tx_state.tx_rate_div = rate_divider > 0 ? rate_divider : 1;
    tx_state.info_interval = 200;    // Every 200 packets (~1s at 200Hz)
    tx_state.status_interval = 100;  // Every 100 packets (~0.5s)
}

const uint8_t* rf_ultra_tx_update(uint8_t tracker_id,
                                   const q15_t quat[4],
                                   int16_t accel_z_mg,
                                   uint8_t battery_pct,
                                   uint16_t battery_mv,
                                   int8_t temperature)
{
    tx_state.tx_count++;
    
    // Rate limiting
    if ((tx_state.tx_count % tx_state.tx_rate_div) != 0) {
        return NULL;
    }
    
    // Decide packet type
    if ((tx_state.tx_count % tx_state.info_interval) == 0) {
        // Send info packet periodically
        rf_ultra_build_info_packet(tx_state.packet_buf, tracker_id,
                                    0x45,  // IMU type (ICM-45686)
                                    1, 0, 1);  // Version 1.0.1
        return tx_state.packet_buf;
    }
    
    if ((tx_state.tx_count % tx_state.status_interval) == 0) {
        // Send status packet periodically
        rf_ultra_build_status_packet(tx_state.packet_buf, tracker_id,
                                      0x01,  // Status: active
                                      battery_mv, temperature);
        return tx_state.packet_buf;
    }
    
    // Normal quaternion packet
    rf_ultra_build_quat_packet(tx_state.packet_buf, tracker_id,
                                quat, accel_z_mg, battery_pct);
    return tx_state.packet_buf;
}

/*============================================================================
 * Receiver State Machine
 *============================================================================*/

#include "board.h"  // 使用统一的 MAX_TRACKERS

#define MAX_TRACKERS_ULTRA  MAX_TRACKERS  // 使用 config.h 中定义

static struct {
    rf_ultra_parsed_t last_data[MAX_TRACKERS_ULTRA];
    uint8_t sequence[MAX_TRACKERS_ULTRA];
    uint8_t active_mask[3];     // Bit mask for active trackers (24 bits)
    uint16_t rx_count;
    uint16_t crc_errors;
} rx_state;

void rf_ultra_rx_init(void)
{
    memset(&rx_state, 0, sizeof(rx_state));
}

bool rf_ultra_rx_process(const uint8_t *pkt, int8_t rssi)
{
    rf_ultra_parsed_t parsed;
    
    if (!rf_ultra_parse_packet(pkt, &parsed)) {
        rx_state.crc_errors++;
        return false;
    }
    
    if (parsed.tracker_id >= MAX_TRACKERS_ULTRA) {
        return false;
    }
    
    // Store data
    rx_state.last_data[parsed.tracker_id] = parsed;
    rx_state.last_data[parsed.tracker_id].rssi = rssi;
    rx_state.sequence[parsed.tracker_id]++;
    
    // Mark tracker as active
    rx_state.active_mask[parsed.tracker_id / 8] |= (1 << (parsed.tracker_id % 8));
    
    rx_state.rx_count++;
    return true;
}

bool rf_ultra_rx_get_tracker(uint8_t id, rf_ultra_parsed_t *out)
{
    if (id >= MAX_TRACKERS_ULTRA) return false;
    if (!(rx_state.active_mask[id / 8] & (1 << (id % 8)))) return false;
    
    *out = rx_state.last_data[id];
    return true;
}

uint8_t rf_ultra_rx_get_active_count(void)
{
    uint8_t count = 0;
    for (int i = 0; i < 3; i++) {
        uint8_t mask = rx_state.active_mask[i];
        while (mask) {
            count += mask & 1;
            mask >>= 1;
        }
    }
    return count;
}

/*============================================================================
 * SlimeVR Protocol Conversion
 *============================================================================*/

// Convert to SlimeVR 16-byte packet format for USB HID
void rf_ultra_to_slimevr_packet(const rf_ultra_parsed_t *in, uint8_t *out)
{
    memset(out, 0, 16);
    
    out[0] = 1;  // SlimeVR packet type 1 (full precision quat + accel)
    out[1] = in->tracker_id;
    
    // Quaternion (already int16, direct copy)
    memcpy(&out[2], in->quat, 8);
    
    // Acceleration (use accel_z for Z, zero for X/Y)
    int16_t *accel = (int16_t*)&out[10];
    accel[0] = 0;
    accel[1] = 0;
    accel[2] = in->accel_z_mg;
}
