/**
 * @file rf_protocol_opt.h
 * @brief Optimized RF Protocol Definitions for CH592
 * 
 * Memory optimizations:
 * - Packed structures
 * - Fixed-point instead of float
 * - Reduced buffer sizes
 * - Bit fields for flags
 */

#ifndef __RF_PROTOCOL_OPT_H__
#define __RF_PROTOCOL_OPT_H__

#include <stdint.h>
#include <stdbool.h>
#include "optimize.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Optimized Constants
 *============================================================================*/

#define RF_MAX_TRACKERS             MAX_TRACKERS
#define RF_CHANNEL_COUNT            40
#define RF_SYNC_WORD                0x534C5652

// Reduced timing for tighter scheduling
#define RF_SUPERFRAME_US            5000
#define RF_SYNC_SLOT_US             200     // Reduced from 250
#define RF_DATA_SLOT_US             380     // Reduced from 400

/*============================================================================
 * Optimized Packet Structures (Packed, Minimal)
 *============================================================================*/

// Sync beacon - 12 bytes (was 15)
typedef struct PACKED {
    u8  type;                       // Packet type
    u16 frame_number;               // Frame counter
    u16 active_mask;                // 10 trackers fit in 16 bits
    u8  channel_map[4];             // Next 4 channels (was 5)
    u8  flags;                      // TX power + status flags
    u16 crc;
} rf_sync_opt_t;

// Tracker data - 18 bytes (was 22)
typedef struct PACKED {
    u8  type;
    u8  id_seq;                     // Tracker ID (4 bits) + Sequence (4 bits)
    q15_t quat[4];                  // Quaternion as Q15 (8 bytes)
    s16 accel[3];                   // Accel in 0.01g units (6 bytes)
    u8  batt_flags;                 // Battery (6 bits) + Flags (2 bits)
    u16 crc;
} rf_tracker_opt_t;

// ACK - 6 bytes (was 9)
typedef struct PACKED {
    u8  type;
    u8  id_seq;                     // ID + ack sequence
    u8  cmd_param;                  // Command (4 bits) + Param (4 bits)
    u16 crc;
} rf_ack_opt_t;

// Pairing request - 10 bytes (was 13)
typedef struct PACKED {
    u8  type;
    u8  mac[6];
    u8  device_ver;                 // Type (4 bits) + Version (4 bits)
    u16 crc;
} rf_pair_req_opt_t;

// Pairing response - 14 bytes (was 18)
typedef struct PACKED {
    u8  type;
    u8  mac[6];
    u8  tracker_id;
    u32 network_key;
    u16 crc;
} rf_pair_resp_opt_t;

/*============================================================================
 * Optimized Tracker Info (RAM Savings)
 *============================================================================*/

// Compact tracker info - 24 bytes per tracker (was 48+)
typedef struct PACKED {
    u8  mac[6];                     // MAC address
    q15_t quat[4];                  // Quaternion (8 bytes)
    s16 accel[3];                   // Acceleration (6 bytes)
    u8  last_seq;                   // Last sequence
    u8  rssi;                       // Signal strength
    u8  batt_loss;                  // Battery (6 bits) + Loss (2 bits)
    u8  flags;                      // Status flags + active/connected
} tracker_info_opt_t;

// Flag bits in tracker_info_opt_t.flags
#define TRK_FLAG_ACTIVE         BIT(0)
#define TRK_FLAG_CONNECTED      BIT(1)
#define TRK_FLAG_CHARGING       BIT(2)
#define TRK_FLAG_LOW_BATT       BIT(3)
#define TRK_FLAG_CALIBRATING    BIT(4)
#define TRK_FLAG_ERROR          BIT(7)

/*============================================================================
 * Optimized Receiver Context - ~280 bytes (was ~500+)
 *============================================================================*/

typedef struct PACKED {
    u8  state;                      // rx_state_t
    u16 frame_number;
    u8  current_channel;
    u32 network_key;
    u8  paired_count;
    
    // Tracker array - 240 bytes for 10 trackers
    tracker_info_opt_t trackers[RF_MAX_TRACKERS];
    
    // Timing - 8 bytes
    u32 superframe_start_us;
    
    // Channel quality - 5 bytes (40 channels / 8)
    u8  channel_blacklist[5];
    
    // Statistics - 8 bytes
    u32 total_packets;
    u32 lost_packets;
} rf_receiver_opt_t;

/*============================================================================
 * Optimized Transmitter Context - ~48 bytes (was ~80+)
 *============================================================================*/

typedef struct PACKED {
    u8  state;                      // tx_state_t
    u8  tracker_id;
    u8  mac[6];
    u32 network_key;
    
    // Sync info - 8 bytes
    u16 frame_number;
    u8  current_channel;
    u8  sequence;
    u32 sync_time_us;
    
    // Sensor data - 14 bytes
    q15_t quat[4];
    s16 accel[3];
    
    // Status - 3 bytes
    u8  battery;
    u8  flags;
    u8  retry_count;
    
    // Flags packed as bits
    u8  status_bits;                // paired, pending_ack, etc.
} rf_transmitter_opt_t;

// Status bits
#define TX_BIT_PAIRED           BIT(0)
#define TX_BIT_PENDING_ACK      BIT(1)

/*============================================================================
 * Inline Helpers
 *============================================================================*/

// Pack tracker ID and sequence into one byte
static FORCE_INLINE u8 pack_id_seq(u8 id, u8 seq) {
    return ((id & 0x0F) << 4) | (seq & 0x0F);
}

static FORCE_INLINE void unpack_id_seq(u8 packed, u8 *id, u8 *seq) {
    *id = (packed >> 4) & 0x0F;
    *seq = packed & 0x0F;
}

// Pack battery and flags
static FORCE_INLINE u8 pack_batt_flags(u8 batt, u8 flags) {
    return ((batt & 0x3F) << 2) | (flags & 0x03);
}

static FORCE_INLINE void unpack_batt_flags(u8 packed, u8 *batt, u8 *flags) {
    *batt = (packed >> 2) & 0x3F;
    *flags = packed & 0x03;
}

// Convert float quaternion to Q15
static FORCE_INLINE void float_to_q15_quat(const float *f, q15_t *q) {
    q[0] = FLOAT_TO_Q15(f[0]);
    q[1] = FLOAT_TO_Q15(f[1]);
    q[2] = FLOAT_TO_Q15(f[2]);
    q[3] = FLOAT_TO_Q15(f[3]);
}

// Convert Q15 quaternion to float
static FORCE_INLINE void q15_to_float_quat(const q15_t *q, float *f) {
    f[0] = Q15_TO_FLOAT(q[0]);
    f[1] = Q15_TO_FLOAT(q[1]);
    f[2] = Q15_TO_FLOAT(q[2]);
    f[3] = Q15_TO_FLOAT(q[3]);
}

// Convert float accel (g) to int16 (0.001g units)
static FORCE_INLINE void float_to_mg_accel(const float *f, s16 *a) {
    a[0] = (s16)(f[0] * 1000.0f);
    a[1] = (s16)(f[1] * 1000.0f);
    a[2] = (s16)(f[2] * 1000.0f);
}

/*============================================================================
 * CRC16 (Optimized)
 *============================================================================*/

// Table-less CRC16 to save 512 bytes of Flash
static FORCE_INLINE u16 crc16_update(u16 crc, u8 data) {
    crc ^= data;
    for (u8 i = 0; i < 8; i++) {
        if (crc & 1)
            crc = (crc >> 1) ^ 0xA001;
        else
            crc >>= 1;
    }
    return crc;
}

static inline u16 crc16_calc(const void *data, u16 len) {
    const u8 *p = (const u8 *)data;
    u16 crc = 0xFFFF;
    while (len--) {
        crc = crc16_update(crc, *p++);
    }
    return crc;
}

#ifdef __cplusplus
}
#endif

#endif /* __RF_PROTOCOL_OPT_H__ */
