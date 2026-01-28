/**
 * @file usb_protocol.h
 * @brief USB CDC Protocol for SlimeVR Receiver
 * 
 * Defines the USB communication protocol between the RF receiver
 * dongle and the host PC running SlimeVR Server.
 */

#ifndef __USB_PROTOCOL_H__
#define __USB_PROTOCOL_H__

#include <stdint.h>
#include <stdbool.h>
#include "rf_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Protocol Constants
 *============================================================================*/

#define USB_FRAME_HEADER        0xAA55
#define USB_MAX_PAYLOAD_SIZE    256
#define USB_BAUD_RATE           921600  // High speed for 10 trackers @ 200Hz

/*============================================================================
 * Packet Types (Host ← Dongle)
 *============================================================================*/

typedef enum {
    USB_PKT_TRACKER_DATA    = 0x01,     // Aggregated tracker data
    USB_PKT_TRACKER_STATUS  = 0x02,     // Individual tracker status
    USB_PKT_SYSTEM_STATUS   = 0x03,     // Dongle system status
    USB_PKT_PAIR_EVENT      = 0x04,     // Pairing event notification
    USB_PKT_DEBUG           = 0x0F,     // Debug message
} usb_packet_type_t;

/*============================================================================
 * Command Types (Host → Dongle)
 *============================================================================*/

typedef enum {
    USB_CMD_GET_STATUS      = 0x10,     // Request system status
    USB_CMD_START_PAIRING   = 0x20,     // Enter pairing mode
    USB_CMD_STOP_PAIRING    = 0x21,     // Exit pairing mode
    USB_CMD_UNPAIR          = 0x22,     // Unpair specific tracker
    USB_CMD_UNPAIR_ALL      = 0x23,     // Unpair all trackers
    USB_CMD_TRACKER_CMD     = 0x30,     // Send command to tracker
    USB_CMD_SET_CONFIG      = 0x40,     // Set configuration
    USB_CMD_SAVE_CONFIG     = 0x41,     // Save config to flash
    USB_CMD_RESET           = 0xFF,     // Reset dongle
} usb_command_type_t;

/*============================================================================
 * Packet Structures
 *============================================================================*/

// Frame header (all packets start with this)
typedef struct __attribute__((packed)) {
    uint16_t header;        // 0xAA55
    uint8_t type;           // Packet type
    uint8_t length;         // Payload length
} usb_frame_header_t;

// Single tracker data in aggregated packet
typedef struct __attribute__((packed)) {
    uint8_t tracker_id;
    int16_t quat_w;
    int16_t quat_x;
    int16_t quat_y;
    int16_t quat_z;
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    uint8_t battery;
    uint8_t flags;
} usb_tracker_data_t;

// Aggregated tracker data packet
// Sent every 5ms with all active tracker data
typedef struct __attribute__((packed)) {
    usb_frame_header_t frame;
    uint16_t frame_number;          // RF frame number
    uint8_t tracker_count;          // Number of trackers in packet
    usb_tracker_data_t trackers[];  // Variable length array
    // uint8_t checksum;            // At end of packet
} usb_tracker_packet_t;

// Individual tracker status
typedef struct __attribute__((packed)) {
    usb_frame_header_t frame;
    uint8_t tracker_id;
    uint8_t connected;
    int8_t rssi;
    uint8_t packet_loss;
    uint8_t battery;
    uint8_t flags;
    uint8_t mac_address[6];
    uint8_t checksum;
} usb_tracker_status_t;

// System status
typedef struct __attribute__((packed)) {
    usb_frame_header_t frame;
    uint8_t state;                  // Dongle state
    uint8_t paired_count;           // Number of paired trackers
    uint8_t connected_count;        // Number of connected trackers
    uint16_t frame_count;           // Frames since start
    uint32_t packets_received;      // Total packets received
    uint32_t packets_lost;          // Total packets lost
    uint8_t current_channel;        // Current RF channel
    uint8_t firmware_version[2];    // Major.Minor
    uint8_t checksum;
} usb_system_status_t;

// Pairing event
typedef struct __attribute__((packed)) {
    usb_frame_header_t frame;
    uint8_t event;                  // 0=started, 1=tracker_added, 2=stopped, 3=timeout
    uint8_t tracker_id;             // For tracker_added event
    uint8_t mac_address[6];         // Tracker MAC
    uint8_t checksum;
} usb_pair_event_t;

// Command to tracker
typedef struct __attribute__((packed)) {
    usb_frame_header_t frame;
    uint8_t tracker_id;
    uint8_t command;                // rf_command_t
    uint8_t param;
    uint8_t checksum;
} usb_tracker_cmd_t;

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Initialize USB CDC
 * @return 0 on success
 */
int usb_protocol_init(void);

/**
 * @brief Check if USB is connected
 * @return true if connected
 */
bool usb_is_connected(void);

/**
 * @brief Send aggregated tracker data
 * @param frame_number Current RF frame number
 * @param trackers Array of tracker info
 * @param count Number of active trackers
 */
void usb_send_tracker_data(uint16_t frame_number, 
                            const tracker_info_t *trackers,
                            uint8_t count);

/**
 * @brief Send tracker status
 * @param tracker_id Tracker ID
 * @param info Tracker information
 */
void usb_send_tracker_status(uint8_t tracker_id, const tracker_info_t *info);

/**
 * @brief Send system status
 * @param ctx Receiver context
 */
void usb_send_system_status(const rf_receiver_ctx_t *ctx);

/**
 * @brief Send pairing event
 * @param event Event type
 * @param tracker_id Tracker ID (if applicable)
 * @param mac MAC address (if applicable)
 */
void usb_send_pair_event(uint8_t event, uint8_t tracker_id, const uint8_t *mac);

/**
 * @brief Send debug message
 * @param msg Message string
 */
void usb_send_debug(const char *msg);

/**
 * @brief Process received USB data (call from main loop)
 * @param rx_ctx Receiver context for command handling
 */
void usb_process_rx(rf_receiver_ctx_t *rx_ctx);

/**
 * @brief Flush USB TX buffer
 */
void usb_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* __USB_PROTOCOL_H__ */
