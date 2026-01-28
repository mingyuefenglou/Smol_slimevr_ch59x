/**
 * @file rf_ultra.h
 * @brief Ultra-Optimized RF Communication for CH592 SlimeVR
 * 
 * Features:
 * - 12-byte minimal packets (vs 21 bytes standard)
 * - CRC8 fast checksum
 * - Zero-copy packet handling
 * - Adaptive transmission rate
 * 
 * Packet Types:
 * - Type 0: Quaternion + Accel Z + Battery (main data)
 * - Type 1: Device info (periodic)
 * - Type 2: Status (periodic)
 */

#ifndef __RF_ULTRA_H__
#define __RF_ULTRA_H__

#include "optimize.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define RF_ULTRA_PACKET_SIZE    12
#define RF_ULTRA_MAX_TRACKERS   24

/*============================================================================
 * Parsed Packet Structure
 *============================================================================*/

typedef struct {
    // Common
    uint8_t type;
    uint8_t tracker_id;
    int8_t  rssi;
    
    // Type 0: Quaternion data
    q15_t   quat[4];        // Quaternion [w,x,y,z]
    int16_t accel_z_mg;     // Vertical acceleration in mg
    uint8_t battery_pct;    // Battery percentage
    
    // Type 1: Device info
    uint8_t  imu_type;
    uint8_t  fw_major;
    uint8_t  fw_minor;
    uint16_t fw_patch;
    uint32_t unique_id;
    
    // Type 2: Status
    uint8_t  status_flags;
    uint16_t battery_mv;
    int8_t   temperature;
} rf_ultra_parsed_t;

/*============================================================================
 * Packet Building Functions
 *============================================================================*/

/**
 * @brief Build quaternion packet (type 0)
 * @param pkt Output packet buffer (12 bytes)
 * @param tracker_id Tracker ID (0-63)
 * @param quat Quaternion in Q15 format
 * @param accel_z_mg Vertical acceleration in mg
 * @param battery_pct Battery percentage (0-100)
 */
void rf_ultra_build_quat_packet(uint8_t *pkt,
                                 uint8_t tracker_id,
                                 const q15_t quat[4],
                                 int16_t accel_z_mg,
                                 uint8_t battery_pct);

/**
 * @brief Build info packet (type 1)
 */
void rf_ultra_build_info_packet(uint8_t *pkt,
                                 uint8_t tracker_id,
                                 uint8_t imu_type,
                                 uint8_t fw_major,
                                 uint8_t fw_minor,
                                 uint16_t fw_patch);

/**
 * @brief Build status packet (type 2)
 */
void rf_ultra_build_status_packet(uint8_t *pkt,
                                   uint8_t tracker_id,
                                   uint8_t status_flags,
                                   uint16_t battery_mv,
                                   int8_t temperature_c);

/*============================================================================
 * Packet Parsing
 *============================================================================*/

/**
 * @brief Parse received packet
 * @param pkt Input packet (12 bytes)
 * @param out Parsed output
 * @return true if valid packet, false if CRC error
 */
bool rf_ultra_parse_packet(const uint8_t *pkt, rf_ultra_parsed_t *out);

/*============================================================================
 * Transmitter API
 *============================================================================*/

/**
 * @brief Initialize transmitter
 * @param rate_divider Rate divider (1 = full rate, 2 = half rate, etc.)
 */
void rf_ultra_tx_init(uint8_t rate_divider);

/**
 * @brief Update transmitter and get packet to send
 * @param tracker_id This tracker's ID
 * @param quat Current quaternion
 * @param accel_z_mg Vertical acceleration
 * @param battery_pct Battery percentage
 * @param battery_mv Battery voltage
 * @param temperature Temperature in Celsius
 * @return Pointer to packet buffer (12 bytes), or NULL if rate-limited
 */
const uint8_t* rf_ultra_tx_update(uint8_t tracker_id,
                                   const q15_t quat[4],
                                   int16_t accel_z_mg,
                                   uint8_t battery_pct,
                                   uint16_t battery_mv,
                                   int8_t temperature);

/*============================================================================
 * Receiver API
 *============================================================================*/

/**
 * @brief Initialize receiver
 */
void rf_ultra_rx_init(void);

/**
 * @brief Process received packet
 * @param pkt Received packet (12 bytes)
 * @param rssi RSSI value
 * @return true if valid
 */
bool rf_ultra_rx_process(const uint8_t *pkt, int8_t rssi);

/**
 * @brief Get tracker data
 * @param id Tracker ID
 * @param out Output data
 * @return true if tracker is active
 */
bool rf_ultra_rx_get_tracker(uint8_t id, rf_ultra_parsed_t *out);

/**
 * @brief Get number of active trackers
 */
uint8_t rf_ultra_rx_get_active_count(void);

/**
 * @brief Convert to SlimeVR USB HID packet format
 * @param in Parsed RF ultra packet
 * @param out Output 16-byte SlimeVR packet
 */
void rf_ultra_to_slimevr_packet(const rf_ultra_parsed_t *in, uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* __RF_ULTRA_H__ */
