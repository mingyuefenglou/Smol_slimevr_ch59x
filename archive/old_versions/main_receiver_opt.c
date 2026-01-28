/**
 * @file main_receiver_opt.c
 * @brief SlimeVR CH592 Receiver - Optimized Implementation
 * 
 * Based on SlimeVR nRF Receiver protocol analysis:
 * - ESB-compatible 2.4GHz proprietary protocol
 * - CRC32 packet validation
 * - Sequence number tracking for packet loss detection
 * - USB HID output (16-byte reports)
 * - Up to 24 trackers (CH592 RAM limited)
 * 
 * RAM Usage: ~1.2KB
 * Flash Usage: ~35KB
 */

#include "optimize.h"
#include "hal.h"
#include "rf_protocol_opt.h"

#ifdef CH59X
#include "CH59x_common.h"
#endif

/*============================================================================
 * Configuration
 *============================================================================*/

#include "config.h"  // 使用统一的 MAX_TRACKERS 定义

// MAX_TRACKERS 在 config.h 中定义: CH591=16, CH592=24
#define DETECTION_THRESHOLD     25      // Packets before tracker is valid
#define REPORT_INTERVAL_MS      1       // USB HID report interval

// Packet types (SlimeVR compatible)
#define PKT_TYPE_INFO           0       // Device info
#define PKT_TYPE_QUAT_ACCEL     1       // Full precision quat + accel
#define PKT_TYPE_COMPACT        2       // Compact quat + accel + battery
#define PKT_TYPE_STATUS         3       // Status packet
#define PKT_TYPE_QUAT_MAG       4       // Quat + magnetometer
#define PKT_TYPE_RECEIVER       255     // Receiver packet (device address)

/*============================================================================
 * SlimeVR Packet Structures (Compatible with nRF)
 *============================================================================*/

// Packet format analysis from SlimeVR nRF:
// |b0      |b1      |b2-15   |
// |type    |id      |data... |

// Type 0: Device Info
typedef struct PACKED {
    u8  type;           // 0
    u8  id;
    u8  proto;
    u8  batt;
    u8  batt_v;
    u8  temp;
    u8  brd_id;
    u8  mcu_id;
    u8  imu_id;
    u8  mag_id;
    u16 fw_date;
    u8  major;
    u8  minor;
    u8  patch;
    u8  rssi;
} pkt_info_t;

// Type 1: Full precision quaternion + acceleration
typedef struct PACKED {
    u8  type;           // 1
    u8  id;
    s16 q[4];           // Q15 quaternion
    s16 a[3];           // Acceleration in 0.01g
} pkt_quat_accel_t;

// Type 2: Compact packet
typedef struct PACKED {
    u8  type;           // 2
    u8  id;
    u8  batt;
    u8  batt_v;
    u8  temp;
    u8  q_buf[4];       // Compressed quaternion
    s16 a[3];           // Acceleration
    u8  rssi;
} pkt_compact_t;

// Type 3: Status
typedef struct PACKED {
    u8  type;           // 3
    u8  id;
    u8  svr_stat;
    u8  status;
    u8  reserved[11];
    u8  rssi;
} pkt_status_t;

// Type 255: Receiver packet (address registration)
typedef struct PACKED {
    u8  type;           // 255
    u8  id;
    u8  addr[6];
    u8  reserved[8];
} pkt_receiver_t;

// Generic 16-byte packet
typedef union {
    u8 raw[16];
    pkt_info_t info;
    pkt_quat_accel_t quat_accel;
    pkt_compact_t compact;
    pkt_status_t status;
    pkt_receiver_t receiver;
} tracker_packet_t;

// Extended packet (20 bytes with CRC)
typedef struct PACKED {
    tracker_packet_t pkt;
    u32 crc32;
} pkt_with_crc_t;

// Full packet (21 bytes with sequence)
typedef struct PACKED {
    tracker_packet_t pkt;
    u32 crc32;
    u8  sequence;
} pkt_full_t;

/*============================================================================
 * Tracker State (Compact)
 *============================================================================*/

typedef struct PACKED {
    u64 addr;           // 6-byte MAC stored in 8 bytes
    u8  sequence;       // Last sequence number
    u8  detect_cnt;     // Detection counter (threshold)
    u8  lost_cnt;       // Lost packet counter
    u8  flags;          // Status flags
} tracker_state_t;

#define TRK_DISCOVERED  BIT(0)
#define TRK_ACTIVE      BIT(1)

/*============================================================================
 * USB HID Report (SlimeVR compatible)
 *============================================================================*/

typedef struct PACKED {
    u8 data[16];
} hid_report_t;

// Report FIFO
#define REPORT_FIFO_SIZE    32
static hid_report_t report_fifo[REPORT_FIFO_SIZE];
static volatile u8 fifo_read_idx = 0;
static volatile u8 fifo_write_idx = 0;

/*============================================================================
 * Global State
 *============================================================================*/

static tracker_state_t trackers[MAX_TRACKERS];
static u8 stored_trackers = 0;
static u8 discovered_count[MAX_TRACKERS];
static u8 sequences[MAX_TRACKERS];

// Device address
static u8 device_addr[6];
static u8 base_addr_0[4];
static u8 base_addr_1[4];
static u8 addr_prefix[8];

// Pairing state
static bool pairing_mode = false;
static u8 pairing_buf[8];

// Statistics
static u32 total_packets = 0;
static u32 dropped_packets = 0;

/*============================================================================
 * CRC32 (SlimeVR compatible - CRC32-K/4.2)
 *============================================================================*/

// CRC32-K polynomial used by SlimeVR
#define CRC32_POLY  0x93a409eb

static u32 HOT crc32_k_update(u32 crc, const u8 *data, u32 len)
{
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ ((crc & 1) ? CRC32_POLY : 0);
        }
    }
    return crc;
}

/*============================================================================
 * CRC8 (for pairing checksum)
 *============================================================================*/

static u8 crc8_ccitt(u8 init, const u8 *data, u32 len)
{
    u8 crc = init;
    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            crc = (crc << 1) ^ ((crc & 0x80) ? 0x07 : 0);
        }
    }
    return crc;
}

/*============================================================================
 * Address Generation (SlimeVR compatible)
 *============================================================================*/

// Discovery address (fixed, matches SlimeVR)
static const u8 PROGMEM discovery_base_addr_0[4] = {0x62, 0x39, 0x8A, 0xF2};
static const u8 PROGMEM discovery_addr_prefix[8] = {0xFE, 0xFF, 0x29, 0x27, 0x09, 0x02, 0xB2, 0xD6};

static void generate_paired_addresses(void)
{
    // Generate from device unique ID (similar to nRF DEVICEADDR)
    u8 buf[6];
#ifdef CH59X
    // Use CH592 unique ID
    GetMACAddress(buf);
#else
    memset(buf, 0x12, 6);
#endif
    memcpy(device_addr, buf, 6);
    
    // Generate base_addr_1 from device address
    for (int i = 0; i < 4; i++) {
        base_addr_1[i] = buf[i] + buf[4];
    }
    
    // Avoid invalid addresses
    for (int i = 0; i < 4; i++) {
        if (base_addr_1[i] == 0x00 || base_addr_1[i] == 0x55 || base_addr_1[i] == 0xAA)
            base_addr_1[i] += 8;
    }
    
    // Use discovery addresses for base_addr_0 and prefix
    memcpy(base_addr_0, discovery_base_addr_0, 4);
    memcpy(addr_prefix, discovery_addr_prefix, 8);
}

/*============================================================================
 * Report FIFO Operations
 *============================================================================*/

static bool HOT fifo_push(const hid_report_t *report)
{
    u8 next = (fifo_write_idx + 1) % REPORT_FIFO_SIZE;
    if (next == fifo_read_idx) {
        dropped_packets++;
        return false;  // FIFO full
    }
    
    report_fifo[fifo_write_idx] = *report;
    fifo_write_idx = next;
    return true;
}

static bool HOT fifo_pop(hid_report_t *report)
{
    if (fifo_read_idx == fifo_write_idx) {
        return false;  // FIFO empty
    }
    
    *report = report_fifo[fifo_read_idx];
    fifo_read_idx = (fifo_read_idx + 1) % REPORT_FIFO_SIZE;
    return true;
}

static u8 fifo_available(void)
{
    return (fifo_write_idx - fifo_read_idx + REPORT_FIFO_SIZE) % REPORT_FIFO_SIZE;
}

/*============================================================================
 * Packet Processing
 *============================================================================*/

static void HOT process_data_packet(const u8 *data, u8 len, s8 rssi)
{
    // Validate length
    if (len != 16 && len != 20 && len != 21) {
        return;
    }
    
    u8 type = data[0];
    u8 id = data[1];
    
    // Check ID validity
    // 双重边界检查: 防止 stored_trackers 损坏导致越界
    if (id >= MAX_TRACKERS || id >= stored_trackers) {
        return;
    }
    
    // Reserved types
    if (type > 223) {
        return;
    }
    
    // CRC32 validation for 20+ byte packets
    if (len >= 20) {
        u32 crc_calc = crc32_k_update(CRC32_POLY, data, 16);
        u32 crc_recv = *(u32*)&data[16];
        if (crc_calc != crc_recv) {
            return;  // CRC mismatch
        }
    }
    
    // Sequence tracking for 21 byte packets
    if (len == 21) {
        u8 seq = data[20];
        u8 expected = sequences[id] + 1;
        
        if (seq != 0 && sequences[id] != 0 && expected != seq) {
            if (expected > seq && expected < seq + 128) {
                return;  // Old packet, discard
            }
            // Sequence gap - packet(s) lost
            trackers[id].lost_cnt++;
        }
        sequences[id] = seq;
    }
    
    // Detection filtering
    if (discovered_count[id] < DETECTION_THRESHOLD) {
        discovered_count[id]++;
        return;
    }
    
    // Create HID report
    hid_report_t report;
    memcpy(report.data, data, 16);
    
    // Add RSSI to packets that have room
    if (type != 1 && type != 4) {
        report.data[15] = (u8)rssi;
    }
    
    // Try to replace existing report for same tracker
    for (u8 i = fifo_read_idx; i != fifo_write_idx; i = (i + 1) % REPORT_FIFO_SIZE) {
        if (report_fifo[i].data[1] == id) {
            report_fifo[i] = report;
            return;
        }
    }
    
    // Push new report
    fifo_push(&report);
    total_packets++;
}

static void process_pairing_packet(const u8 *data, u8 len)
{
    if (len != 8) return;
    
    memcpy(pairing_buf, data, 8);
    
    switch (pairing_buf[1]) {
        case 0:  // First pairing request
            // Parse and validate
            {
                u64 found_addr = (*(u64*)pairing_buf >> 16) & 0xFFFFFFFFFFFFULL;
                u8 checksum = crc8_ccitt(0x07, &pairing_buf[2], 6);
                if (checksum == 0) checksum = 8;
                
                if (checksum == pairing_buf[0] && found_addr != 0) {
                    // Check if already stored
                    bool exists = false;
                    for (u8 i = 0; i < stored_trackers; i++) {
                        if (trackers[i].addr == found_addr) {
                            exists = true;
                            break;
                        }
                    }
                    
                    if (!exists && stored_trackers < MAX_TRACKERS) {
                        // Add new tracker
                        trackers[stored_trackers].addr = found_addr;
                        trackers[stored_trackers].sequence = 0;
                        trackers[stored_trackers].detect_cnt = 0;
                        trackers[stored_trackers].flags = TRK_DISCOVERED;
                        stored_trackers++;
                        
                        // Save to NVS
                        hal_storage_write(0, &stored_trackers, 1);
                        hal_storage_write(1 + (stored_trackers-1) * 8, &found_addr, 8);
                    }
                }
            }
            break;
            
        case 1:  // ACK sent
        case 2:  // ACK receiver
        default:
            break;
    }
}

/*============================================================================
 * RF Callbacks
 *============================================================================*/

static void on_rf_rx(const u8 *data, u8 len, u8 pipe, s8 rssi)
{
    if (pipe == 0) {
        // Pairing pipe
        process_pairing_packet(data, len);
    } else {
        // Data pipe
        process_data_packet(data, len, rssi);
    }
}

/*============================================================================
 * USB HID
 *============================================================================*/

static u8 sent_addr_idx = 0;
static u32 last_addr_send = 0;

static void make_addr_packet(hid_report_t *report, u8 id)
{
    report->data[0] = PKT_TYPE_RECEIVER;
    report->data[1] = id;
    memcpy(&report->data[2], &trackers[id].addr, 6);
    memset(&report->data[8], 0, 8);
}

static void usb_send_reports(void)
{
    hid_report_t reports[4];
    u8 count = 0;
    
    // Pop available reports
    while (count < 4 && fifo_pop(&reports[count])) {
        count++;
    }
    
    // Pad with address packets
    u32 now = hal_millis();
    if (now - last_addr_send > 100 && stored_trackers > 0) {
        last_addr_send = now;
        
        while (count < 4) {
            make_addr_packet(&reports[count], sent_addr_idx);
            sent_addr_idx = (sent_addr_idx + 1) % stored_trackers;
            count++;
        }
    }
    
    if (count > 0) {
        // Send via USB HID
        // usb_hid_write(reports, count * 16);
    }
}

/*============================================================================
 * Detection Filter Thread
 *============================================================================*/

static void detection_filter_update(void)
{
    // Reset detection count for trackers below threshold
    for (u8 i = 0; i < MAX_TRACKERS; i++) {
        if (discovered_count[i] < DETECTION_THRESHOLD) {
            discovered_count[i] = 0;
        }
    }
}

/*============================================================================
 * Main Receiver
 *============================================================================*/

static void receiver_init(void)
{
    // Load stored trackers
    hal_storage_read(0, &stored_trackers, 1);
    if (stored_trackers > MAX_TRACKERS) stored_trackers = 0;
    
    for (u8 i = 0; i < stored_trackers; i++) {
        hal_storage_read(1 + i * 8, &trackers[i].addr, 8);
    }
    
    // Generate addresses
    generate_paired_addresses();
    
    // Initialize RF
    // rf_init(base_addr_0, base_addr_1, addr_prefix);
    // rf_set_rx_callback(on_rf_rx);
    // rf_start_rx();
}

int main(void)
{
#ifdef CH59X
    SetSysClock(CLK_SOURCE_PLL_60MHz);
#endif
    
    hal_timer_init();
    hal_storage_init();
    
    receiver_init();
    
    u32 last_filter_time = 0;
    u32 last_report_time = 0;
    
    while (1) {
        u32 now = hal_millis();
        
        // Detection filter (1Hz)
        if (now - last_filter_time >= 1000) {
            last_filter_time = now;
            detection_filter_update();
        }
        
        // USB reports (1kHz)
        if (now - last_report_time >= REPORT_INTERVAL_MS) {
            last_report_time = now;
            usb_send_reports();
        }
        
        // RF processing
        // rf_process();
        
        hal_delay_us(100);
    }
    
    return 0;
}
