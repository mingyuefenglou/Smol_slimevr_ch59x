/**
 * @file usb_protocol.c
 * @brief USB CDC Protocol Implementation
 * 
 * Implements USB CDC (Virtual COM Port) communication for the
 * SlimeVR receiver dongle. Aggregates tracker data and sends
 * to host PC at high speed.
 */

#include "usb_protocol.h"
#include "rf_protocol.h"
#include "hal.h"
#include <string.h>
#include <stdio.h>

#ifdef CH59X
#include "CH59x_common.h"
// Include CH59X USB CDC library headers
#endif

/*============================================================================
 * Configuration
 *============================================================================*/

#define USB_TX_BUFFER_SIZE      512
#define USB_RX_BUFFER_SIZE      128
#define USB_SEND_INTERVAL_MS    5       // Send every 5ms (200Hz)

/*============================================================================
 * Static Variables
 *============================================================================*/

static uint8_t tx_buffer[USB_TX_BUFFER_SIZE];
static uint8_t rx_buffer[USB_RX_BUFFER_SIZE];
static uint16_t tx_len = 0;
static uint16_t rx_len = 0;
static uint16_t rx_read_idx = 0;

static bool usb_connected = false;
static uint32_t last_send_time = 0;

/*============================================================================
 * USB Hardware Interface (CH59X Specific)
 *============================================================================*/

#ifdef CH59X

// CH59X USB CDC register definitions would go here
// For now, using placeholder implementations

static void usb_hw_init(void)
{
    // Initialize USB peripheral
    // Configure as CDC device
    // Set up endpoints
}

static void usb_hw_send(const uint8_t *data, uint16_t len)
{
    // Write to USB TX endpoint
    (void)data;
    (void)len;
}

static uint16_t usb_hw_receive(uint8_t *data, uint16_t max_len)
{
    // Read from USB RX endpoint
    (void)data;
    (void)max_len;
    return 0;
}

static bool usb_hw_tx_ready(void)
{
    // Check if TX endpoint is ready
    return true;
}

#else

// Stub implementations for non-CH59X builds
static void usb_hw_init(void) {}
static void usb_hw_send(const uint8_t *data, uint16_t len) { (void)data; (void)len; }
static uint16_t usb_hw_receive(uint8_t *data, uint16_t max_len) { (void)data; (void)max_len; return 0; }
static bool usb_hw_tx_ready(void) { return true; }

#endif

/*============================================================================
 * Checksum Calculation
 *============================================================================*/

static uint8_t calc_checksum(const uint8_t *data, uint16_t len)
{
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return ~sum;
}

/*============================================================================
 * Buffer Management
 *============================================================================*/

static void buffer_add_byte(uint8_t byte)
{
    if (tx_len < USB_TX_BUFFER_SIZE) {
        tx_buffer[tx_len++] = byte;
    }
}

static void buffer_add_word(uint16_t word)
{
    buffer_add_byte(word & 0xFF);
    buffer_add_byte((word >> 8) & 0xFF);
}

static void buffer_add_data(const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len && tx_len < USB_TX_BUFFER_SIZE; i++) {
        tx_buffer[tx_len++] = data[i];
    }
}

static void buffer_flush(void)
{
    if (tx_len > 0 && usb_hw_tx_ready()) {
        usb_hw_send(tx_buffer, tx_len);
        tx_len = 0;
    }
}

/*============================================================================
 * Public API Implementation
 *============================================================================*/

int usb_protocol_init(void)
{
    usb_hw_init();
    tx_len = 0;
    rx_len = 0;
    rx_read_idx = 0;
    usb_connected = false;
    return 0;
}

bool usb_is_connected(void)
{
    return usb_connected;
}

void usb_send_tracker_data(uint16_t frame_number,
                            const tracker_info_t *trackers,
                            uint8_t count)
{
    if (!usb_connected || count == 0) return;
    
    // Rate limiting
    uint32_t now = hal_millis();
    if (now - last_send_time < USB_SEND_INTERVAL_MS) return;
    last_send_time = now;
    
    // Build packet
    tx_len = 0;
    
    // Header
    buffer_add_word(USB_FRAME_HEADER);
    buffer_add_byte(USB_PKT_TRACKER_DATA);
    
    // Calculate payload length: 3 + count * sizeof(usb_tracker_data_t) + 1 (checksum)
    uint8_t payload_len = 3 + count * sizeof(usb_tracker_data_t) + 1;
    buffer_add_byte(payload_len);
    
    // Frame number and count
    buffer_add_word(frame_number);
    buffer_add_byte(count);
    
    // Tracker data
    for (uint8_t i = 0; i < RF_MAX_TRACKERS && count > 0; i++) {
        if (trackers[i].active && trackers[i].connected) {
            usb_tracker_data_t data;
            data.tracker_id = i;
            data.quat_w = (int16_t)(trackers[i].quaternion[0] * 32767.0f);
            data.quat_x = (int16_t)(trackers[i].quaternion[1] * 32767.0f);
            data.quat_y = (int16_t)(trackers[i].quaternion[2] * 32767.0f);
            data.quat_z = (int16_t)(trackers[i].quaternion[3] * 32767.0f);
            data.accel_x = (int16_t)(trackers[i].acceleration[0] * 1000.0f);
            data.accel_y = (int16_t)(trackers[i].acceleration[1] * 1000.0f);
            data.accel_z = (int16_t)(trackers[i].acceleration[2] * 1000.0f);
            data.battery = trackers[i].battery;
            data.flags = trackers[i].flags;
            
            buffer_add_data((uint8_t *)&data, sizeof(data));
            count--;
        }
    }
    
    // Checksum (over everything after header word)
    uint8_t checksum = calc_checksum(tx_buffer + 2, tx_len - 2);
    buffer_add_byte(checksum);
    
    buffer_flush();
}

void usb_send_tracker_status(uint8_t tracker_id, const tracker_info_t *info)
{
    if (!usb_connected || !info) return;
    
    usb_tracker_status_t pkt;
    pkt.frame.header = USB_FRAME_HEADER;
    pkt.frame.type = USB_PKT_TRACKER_STATUS;
    pkt.frame.length = sizeof(usb_tracker_status_t) - sizeof(usb_frame_header_t);
    pkt.tracker_id = tracker_id;
    pkt.connected = info->connected ? 1 : 0;
    pkt.rssi = (int8_t)(info->rssi - 128);
    pkt.packet_loss = info->packet_loss;
    pkt.battery = info->battery;
    pkt.flags = info->flags;
    memcpy(pkt.mac_address, info->mac_address, 6);
    pkt.checksum = calc_checksum((uint8_t *)&pkt + 2, sizeof(pkt) - 3);
    
    tx_len = 0;
    buffer_add_data((uint8_t *)&pkt, sizeof(pkt));
    buffer_flush();
}

void usb_send_system_status(const rf_receiver_ctx_t *ctx)
{
    if (!usb_connected || !ctx) return;
    
    // Count connected trackers
    uint8_t connected_count = 0;
    for (int i = 0; i < RF_MAX_TRACKERS; i++) {
        if (ctx->trackers[i].connected) {
            connected_count++;
        }
    }
    
    usb_system_status_t pkt;
    pkt.frame.header = USB_FRAME_HEADER;
    pkt.frame.type = USB_PKT_SYSTEM_STATUS;
    pkt.frame.length = sizeof(usb_system_status_t) - sizeof(usb_frame_header_t);
    pkt.state = ctx->state;
    pkt.paired_count = ctx->paired_count;
    pkt.connected_count = connected_count;
    pkt.frame_count = ctx->frame_number;
    pkt.packets_received = ctx->total_packets;
    pkt.packets_lost = ctx->lost_packets;
    pkt.current_channel = ctx->current_channel;
    pkt.firmware_version[0] = 1;
    pkt.firmware_version[1] = 0;
    pkt.checksum = calc_checksum((uint8_t *)&pkt + 2, sizeof(pkt) - 3);
    
    tx_len = 0;
    buffer_add_data((uint8_t *)&pkt, sizeof(pkt));
    buffer_flush();
}

void usb_send_pair_event(uint8_t event, uint8_t tracker_id, const uint8_t *mac)
{
    if (!usb_connected) return;
    
    usb_pair_event_t pkt;
    pkt.frame.header = USB_FRAME_HEADER;
    pkt.frame.type = USB_PKT_PAIR_EVENT;
    pkt.frame.length = sizeof(usb_pair_event_t) - sizeof(usb_frame_header_t);
    pkt.event = event;
    pkt.tracker_id = tracker_id;
    if (mac) {
        memcpy(pkt.mac_address, mac, 6);
    } else {
        memset(pkt.mac_address, 0, 6);
    }
    pkt.checksum = calc_checksum((uint8_t *)&pkt + 2, sizeof(pkt) - 3);
    
    tx_len = 0;
    buffer_add_data((uint8_t *)&pkt, sizeof(pkt));
    buffer_flush();
}

void usb_send_debug(const char *msg)
{
    if (!usb_connected || !msg) return;
    
    uint8_t len = strlen(msg);
    if (len > 200) len = 200;
    
    tx_len = 0;
    buffer_add_word(USB_FRAME_HEADER);
    buffer_add_byte(USB_PKT_DEBUG);
    buffer_add_byte(len + 1);  // +1 for checksum
    buffer_add_data((const uint8_t *)msg, len);
    
    uint8_t checksum = calc_checksum(tx_buffer + 2, tx_len - 2);
    buffer_add_byte(checksum);
    
    buffer_flush();
}

void usb_process_rx(rf_receiver_ctx_t *rx_ctx)
{
    // Check for USB connection state
    // usb_connected = usb_hw_is_connected();
    usb_connected = true;  // Assume connected for now
    
    // Read incoming data
    uint16_t received = usb_hw_receive(rx_buffer + rx_len, 
                                        USB_RX_BUFFER_SIZE - rx_len);
    rx_len += received;
    
    // Process complete packets
    while (rx_len >= 4) {
        // Look for header
        if (rx_buffer[rx_read_idx] != 0x55 || rx_buffer[rx_read_idx + 1] != 0xAA) {
            // Sync error, skip byte
            rx_read_idx++;
            rx_len--;
            continue;
        }
        
        uint8_t type = rx_buffer[rx_read_idx + 2];
        uint8_t length = rx_buffer[rx_read_idx + 3];
        
        // Check if complete packet available
        if (rx_len < 4 + length) {
            break;  // Wait for more data
        }
        
        // Verify checksum
        uint8_t calc_chk = calc_checksum(rx_buffer + rx_read_idx + 2, length - 1);
        uint8_t pkt_chk = rx_buffer[rx_read_idx + 3 + length];
        
        if (calc_chk != pkt_chk) {
            // Checksum error, skip
            rx_read_idx++;
            rx_len--;
            continue;
        }
        
        // Process command
        switch (type) {
            case USB_CMD_GET_STATUS:
                usb_send_system_status(rx_ctx);
                break;
                
            case USB_CMD_START_PAIRING:
                rf_receiver_start_pairing(rx_ctx);
                usb_send_pair_event(0, 0, NULL);
                break;
                
            case USB_CMD_STOP_PAIRING:
                rf_receiver_stop_pairing(rx_ctx);
                usb_send_pair_event(2, 0, NULL);
                break;
                
            case USB_CMD_UNPAIR: {
                uint8_t tracker_id = rx_buffer[rx_read_idx + 4];
                rf_receiver_unpair(rx_ctx, tracker_id);
                break;
            }
                
            case USB_CMD_UNPAIR_ALL:
                rf_receiver_unpair_all(rx_ctx);
                break;
                
            case USB_CMD_TRACKER_CMD: {
                usb_tracker_cmd_t *cmd = (usb_tracker_cmd_t *)(rx_buffer + rx_read_idx);
                rf_receiver_send_command(rx_ctx, cmd->tracker_id, 
                                          cmd->command, cmd->param);
                break;
            }
                
            case USB_CMD_RESET:
#ifdef CH59X
                SYS_ResetExecute();
#endif
                break;
                
            default:
                break;
        }
        
        // Consume packet
        rx_read_idx += 4 + length;
        rx_len -= 4 + length;
    }
    
    // Compact buffer if needed
    if (rx_read_idx > 0 && rx_len > 0) {
        memmove(rx_buffer, rx_buffer + rx_read_idx, rx_len);
        rx_read_idx = 0;
    } else if (rx_len == 0) {
        rx_read_idx = 0;
    }
}

void usb_flush(void)
{
    buffer_flush();
}
