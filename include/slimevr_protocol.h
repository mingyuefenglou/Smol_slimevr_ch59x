/**
 * @file slimevr_protocol.h
 * @brief SlimeVR RF Protocol Compatible with nRF Implementation
 * 
 * Packet format from SlimeVR-Tracker-nRF-Receiver:
 * |b0      |b1      |b2-b15  |
 * |type    |id      |data    |
 * 
 * Type 0: Info packet
 * Type 1: Full precision quat + accel
 * Type 2: Compressed data packet (most common)
 * Type 3: Server status
 * Type 4: Full precision quat + mag
 * Type 255: Device registration (receiver->host)
 */

#ifndef __SLIMEVR_PROTOCOL_H__
#define __SLIMEVR_PROTOCOL_H__

#include "optimize.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Configuration
 *============================================================================*/

#include "config.h"  // 使用统一的 MAX_TRACKERS

#define SLIME_MAX_TRACKERS          MAX_TRACKERS  // 使用 config.h 中定义
#define SLIME_DETECTION_THRESHOLD   25      // 有效追踪器检测阈值
#define SLIME_RESET_THRESHOLD       75      // 异常旋转重置阈值

/*============================================================================
 * RF Address Configuration (Must match nRF)
 *============================================================================*/

// Discovery addresses (fixed, for pairing)
#define SLIME_DISCOVERY_ADDR0       {0x62, 0x39, 0x8A, 0xF2}
#define SLIME_DISCOVERY_ADDR1       {0x28, 0xFF, 0x50, 0xB8}
#define SLIME_DISCOVERY_PREFIX      {0xFE, 0xFF, 0x29, 0x27, 0x09, 0x02, 0xB2, 0xD6}

/*============================================================================
 * Packet Types
 *============================================================================*/

typedef enum : u8 {
    PKT_TYPE_INFO       = 0,    // Device info
    PKT_TYPE_QUAT_ACCEL = 1,    // Full precision quat + accel (16 bytes)
    PKT_TYPE_COMPACT    = 2,    // Compressed quat + accel + battery
    PKT_TYPE_STATUS     = 3,    // Server status
    PKT_TYPE_QUAT_MAG   = 4,    // Full precision quat + mag
    PKT_TYPE_RESERVED   = 224,  // 224-254 reserved
    PKT_TYPE_REGISTER   = 255,  // Device registration (receiver only)
} pkt_type_t;

/*============================================================================
 * Packet Structures
 *============================================================================*/

// Type 0: Info packet
// |type|id|proto|batt|batt_v|temp|brd_id|mcu_id|imu_id|mag_id|fw_date|major|minor|patch|rssi|
typedef struct PACKED {
    u8  type;           // 0
    u8  id;
    u8  proto;          // Protocol version
    u8  batt;           // Battery %
    u8  batt_v;         // Battery voltage
    u8  temp;           // Temperature
    u8  board_id;
    u8  mcu_id;
    u8  imu_id;
    u8  mag_id;
    u16 fw_date;        // Firmware date
    u8  fw_major;
    u8  fw_minor;
    u8  fw_patch;
    u8  rssi;
} pkt_info_t;

// Type 1: Full precision quat + accel (no rssi field)
// |type|id|q0|q0|q1|q1|q2|q2|q3|q3|a0|a0|a1|a1|a2|a2|
typedef struct PACKED {
    u8  type;           // 1
    u8  id;
    s16 quat[4];        // Q15 format
    s16 accel[3];       // 0.01g units
} pkt_quat_accel_t;

// Type 2: Compact packet (most common)
// |type|id|batt|batt_v|temp|q_buf[7]|a0|a0|a1|a1|a2|a2|rssi|
typedef struct PACKED {
    u8  type;           // 2
    u8  id;
    u8  batt;           // Battery %
    u8  batt_v;         // Battery voltage
    u8  temp;           // Temperature
    u8  q_buf[7];       // Compressed quaternion
    s16 accel[3];       // 0.01g units
    u8  rssi;
} pkt_compact_t;

// Type 3: Status packet
// |type|id|svr_stat|status|reserved[11]|rssi|
typedef struct PACKED {
    u8  type;           // 3
    u8  id;
    u8  svr_status;
    u8  status;
    u8  reserved[11];
    u8  rssi;
} pkt_status_t;

// Type 4: Full precision quat + mag
// |type|id|q0|q0|q1|q1|q2|q2|q3|q3|m0|m0|m1|m1|m2|m2|
typedef struct PACKED {
    u8  type;           // 4
    u8  id;
    s16 quat[4];        // Q15 format
    s16 mag[3];         // Magnetometer
} pkt_quat_mag_t;

// Type 255: Registration packet (receiver -> host)
// |type|id|addr[6]|reserved[8]|
typedef struct PACKED {
    u8  type;           // 255
    u8  id;
    u8  addr[6];        // Tracker MAC address
    u8  reserved[8];
} pkt_register_t;

// Generic 16-byte packet
typedef union {
    u8 raw[16];
    struct PACKED {
        u8 type;
        u8 id;
        u8 data[14];
    };
    pkt_info_t       info;
    pkt_quat_accel_t quat_accel;
    pkt_compact_t    compact;
    pkt_status_t     status;
    pkt_quat_mag_t   quat_mag;
    pkt_register_t   reg;
} slime_packet_t;

/*============================================================================
 * Extended Packet (with CRC and sequence)
 *============================================================================*/

// 20 bytes: packet + CRC32
typedef struct PACKED {
    slime_packet_t pkt;
    u32 crc;            // CRC32 (K=4, P=2)
} pkt_with_crc_t;

// 21 bytes: packet + CRC32 + sequence
typedef struct PACKED {
    slime_packet_t pkt;
    u32 crc;
    u8  seq;            // Sequence number
} pkt_with_seq_t;

/*============================================================================
 * Pairing Packet (8 bytes on pipe 0)
 *============================================================================*/

typedef struct PACKED {
    u8  checksum;       // CRC8 of addr[6]
    u8  stage;          // 0=request, 1=ack_sent, 2=ack_received
    u8  addr[6];        // Tracker MAC address
} pair_packet_t;

/*============================================================================
 * Tracker Info (Compact, 16 bytes per tracker) - SlimeVR协议专用
 *============================================================================*/

typedef struct PACKED {
    u8  addr[6];        // MAC address
    u8  last_seq;       // Last sequence number
    u8  rssi;           // Last RSSI
    u8  detect_cnt;     // Detection counter
    u8  loss_cnt;       // Packet loss counter
    u8  flags;          // Status flags
    u8  battery;        // Battery level
    u32 last_seen;      // Last packet timestamp (truncated)
} slime_tracker_info_t;

// Tracker flags
#define TRK_FLAG_VALID      BIT(0)  // Above detection threshold
#define TRK_FLAG_ACTIVE     BIT(1)  // Received packet recently
#define TRK_FLAG_CHARGING   BIT(2)
#define TRK_FLAG_LOW_BATT   BIT(3)

/*============================================================================
 * CRC Functions
 *============================================================================*/

// CRC32-K/4/2 (same as nRF implementation)
static inline u32 crc32_k_4_2(u32 init, const void *data, u32 len)
{
    const u8 *p = (const u8*)data;
    u32 crc = init;
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0x93A409EB : 0);
        }
    }
    return crc;
}

// CRC8-CCITT (for pairing checksum)
static inline u8 crc8_ccitt(u8 init, const void *data, u32 len)
{
    const u8 *p = (const u8*)data;
    u8 crc = init;
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) {
            crc = (crc & 0x80) ? ((crc << 1) ^ 0x07) : (crc << 1);
        }
    }
    return crc;
}

/*============================================================================
 * Quaternion Compression/Decompression
 *============================================================================*/

// Compress 4x float quaternion to 7 bytes
static inline void quat_compress(const float *q, u8 *out)
{
    // Find largest component
    int max_idx = 0;
    float max_val = q[0] < 0 ? -q[0] : q[0];
    for (int i = 1; i < 4; i++) {
        float v = q[i] < 0 ? -q[i] : q[i];
        if (v > max_val) {
            max_val = v;
            max_idx = i;
        }
    }
    
    // Get sign and remaining components
    float sign = q[max_idx] < 0 ? -1.0f : 1.0f;
    float a, b, c;
    switch (max_idx) {
        case 0: a = q[1]*sign; b = q[2]*sign; c = q[3]*sign; break;
        case 1: a = q[0]*sign; b = q[2]*sign; c = q[3]*sign; break;
        case 2: a = q[0]*sign; b = q[1]*sign; c = q[3]*sign; break;
        default: a = q[0]*sign; b = q[1]*sign; c = q[2]*sign; break;
    }
    
    // Encode to 18 bits each (range -1 to 1)
    s32 ia = (s32)(a * 131071.0f);  // 2^17 - 1
    s32 ib = (s32)(b * 131071.0f);
    s32 ic = (s32)(c * 131071.0f);
    
    // Pack: 2 bits index + 18*3 = 56 bits = 7 bytes
    u64 packed = ((u64)max_idx << 54);
    packed |= ((u64)(ia & 0x3FFFF) << 36);
    packed |= ((u64)(ib & 0x3FFFF) << 18);
    packed |= (ic & 0x3FFFF);
    
    for (int i = 0; i < 7; i++) {
        out[6-i] = packed & 0xFF;
        packed >>= 8;
    }
}

// Decompress 7 bytes to 4x float quaternion
static inline void quat_decompress(const u8 *in, float *q)
{
    u64 packed = 0;
    for (int i = 0; i < 7; i++) {
        packed = (packed << 8) | in[i];
    }
    
    int max_idx = (packed >> 54) & 3;
    s32 ia = (s32)((packed >> 36) & 0x3FFFF);
    s32 ib = (s32)((packed >> 18) & 0x3FFFF);
    s32 ic = (s32)(packed & 0x3FFFF);
    
    // Sign extend
    if (ia & 0x20000) ia |= 0xFFFC0000;
    if (ib & 0x20000) ib |= 0xFFFC0000;
    if (ic & 0x20000) ic |= 0xFFFC0000;
    
    float a = ia / 131071.0f;
    float b = ib / 131071.0f;
    float c = ic / 131071.0f;
    
    // Reconstruct largest component
    float d = 1.0f - a*a - b*b - c*c;
    d = d > 0 ? __builtin_sqrtf(d) : 0;
    
    switch (max_idx) {
        case 0: q[0] = d; q[1] = a; q[2] = b; q[3] = c; break;
        case 1: q[0] = a; q[1] = d; q[2] = b; q[3] = c; break;
        case 2: q[0] = a; q[1] = b; q[2] = d; q[3] = c; break;
        default: q[0] = a; q[1] = b; q[2] = c; q[3] = d; break;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* __SLIMEVR_PROTOCOL_H__ */
