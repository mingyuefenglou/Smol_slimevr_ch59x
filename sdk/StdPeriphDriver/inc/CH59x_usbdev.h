/**
 * @file CH59x_usbdev.h
 * @brief CH592 USB Device Driver (Reconstructed from WCH SDK)
 * 
 * CH592 USB Features:
 * - USB 2.0 Full-Speed (12Mbps)
 * - Host and Device modes
 * - 8 endpoints (EP0-EP7)
 * - Built-in transceiver
 * 
 * Based on official WCH CH592 EVT SDK structure
 */

#ifndef __CH59X_USBDEV_H__
#define __CH59X_USBDEV_H__

#include "CH59x_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * USB Registers (from CH592 datasheet)
 *============================================================================*/

// USB Base Address
#define USB_BASE_ADDR       0x40008000

// Control registers
#define R8_USB_CTRL         (*((volatile uint8_t*)(USB_BASE_ADDR + 0x00)))
#define R8_UDEV_CTRL        (*((volatile uint8_t*)(USB_BASE_ADDR + 0x01)))
#define R8_USB_INT_EN       (*((volatile uint8_t*)(USB_BASE_ADDR + 0x02)))
#define R8_USB_DEV_AD       (*((volatile uint8_t*)(USB_BASE_ADDR + 0x03)))
#define R8_USB_MIS_ST       (*((volatile uint8_t*)(USB_BASE_ADDR + 0x05)))
#define R8_USB_INT_FG       (*((volatile uint8_t*)(USB_BASE_ADDR + 0x06)))
#define R8_USB_INT_ST       (*((volatile uint8_t*)(USB_BASE_ADDR + 0x07)))
#define R16_USB_RX_LEN      (*((volatile uint16_t*)(USB_BASE_ADDR + 0x08)))

// Endpoint 0
#define R8_UEP0_CTRL        (*((volatile uint8_t*)(USB_BASE_ADDR + 0x22)))
#define R8_UEP0_T_LEN       (*((volatile uint8_t*)(USB_BASE_ADDR + 0x23)))

// Endpoint 1-7 (simplified)
#define R8_UEP1_CTRL        (*((volatile uint8_t*)(USB_BASE_ADDR + 0x24)))
#define R8_UEP1_T_LEN       (*((volatile uint8_t*)(USB_BASE_ADDR + 0x25)))
#define R8_UEP2_CTRL        (*((volatile uint8_t*)(USB_BASE_ADDR + 0x28)))
#define R8_UEP2_T_LEN       (*((volatile uint8_t*)(USB_BASE_ADDR + 0x29)))
#define R8_UEP3_CTRL        (*((volatile uint8_t*)(USB_BASE_ADDR + 0x2C)))
#define R8_UEP3_T_LEN       (*((volatile uint8_t*)(USB_BASE_ADDR + 0x2D)))
#define R8_UEP4_CTRL        (*((volatile uint8_t*)(USB_BASE_ADDR + 0x30)))
#define R8_UEP4_T_LEN       (*((volatile uint8_t*)(USB_BASE_ADDR + 0x31)))

// DMA buffers
#define R32_UEP0_DMA        (*((volatile uint32_t*)(USB_BASE_ADDR + 0x0C)))
#define R32_UEP1_DMA        (*((volatile uint32_t*)(USB_BASE_ADDR + 0x10)))
#define R32_UEP2_DMA        (*((volatile uint32_t*)(USB_BASE_ADDR + 0x14)))
#define R32_UEP3_DMA        (*((volatile uint32_t*)(USB_BASE_ADDR + 0x18)))

/*============================================================================
 * USB Control Register Bits
 *============================================================================*/

// R8_USB_CTRL
#define RB_UC_HOST_MODE     0x80    // Host mode
#define RB_UC_LOW_SPEED     0x40    // Low speed mode
#define RB_UC_DEV_PU_EN     0x20    // Device pull-up enable
#define RB_UC_SYS_CTRL1     0x10    // System control 1
#define RB_UC_SYS_CTRL0     0x08    // System control 0
#define RB_UC_INT_BUSY      0x04    // Interrupt busy
#define RB_UC_RESET_SIE     0x02    // Reset USB SIE
#define RB_UC_CLR_ALL       0x01    // Clear all

// R8_UDEV_CTRL
#define RB_UD_PD_DIS        0x80    // Pull-down disable
#define RB_UD_DP_PIN        0x20    // D+ pin status
#define RB_UD_DM_PIN        0x10    // D- pin status
#define RB_UD_LOW_SPEED     0x04    // Low speed mode
#define RB_UD_GP_BIT        0x02    // General purpose bit
#define RB_UD_PORT_EN       0x01    // Port enable

// R8_USB_INT_EN
#define RB_UIE_DEV_SOF      0x80    // SOF interrupt enable
#define RB_UIE_DEV_NAK      0x40    // NAK interrupt enable
#define RB_UIE_FIFO_OV      0x10    // FIFO overflow interrupt
#define RB_UIE_SUSPEND      0x04    // Suspend interrupt
#define RB_UIE_TRANSFER     0x02    // Transfer interrupt
#define RB_UIE_BUS_RST      0x01    // Bus reset interrupt

// R8_USB_INT_FG
#define RB_UIF_SOF_ACT      0x80    // SOF active
#define RB_UIF_FIFO_OV      0x10    // FIFO overflow
#define RB_UIF_HST_SOF      0x08    // Host SOF
#define RB_UIF_SUSPEND      0x04    // Suspend
#define RB_UIF_TRANSFER     0x02    // Transfer complete
#define RB_UIF_BUS_RST      0x01    // Bus reset

// R8_USB_INT_ST
#define RB_UIS_IS_NAK       0x80    // NAK status
#define RB_UIS_TOG_OK       0x40    // Toggle OK
#define RB_UIS_TOKEN1       0x20    // Token 1
#define RB_UIS_TOKEN0       0x10    // Token 0
#define MASK_UIS_TOKEN      0x30    // Token mask
#define MASK_UIS_ENDP       0x0F    // Endpoint mask

// Token types
#define UIS_TOKEN_OUT       0x00
#define UIS_TOKEN_SOF       0x10
#define UIS_TOKEN_IN        0x20
#define UIS_TOKEN_SETUP     0x30

// Endpoint control
#define RB_UEP_R_TOG        0x80    // RX toggle
#define RB_UEP_T_TOG        0x40    // TX toggle
#define RB_UEP_AUTO_TOG     0x10    // Auto toggle
#define RB_UEP_R_RES1       0x08    // RX response bit 1
#define RB_UEP_R_RES0       0x04    // RX response bit 0
#define RB_UEP_T_RES1       0x02    // TX response bit 1
#define RB_UEP_T_RES0       0x01    // TX response bit 0

// Response types
#define UEP_R_RES_ACK       0x00
#define UEP_R_RES_TOUT      0x04
#define UEP_R_RES_NAK       0x08
#define UEP_R_RES_STALL     0x0C

#define UEP_T_RES_ACK       0x00
#define UEP_T_RES_TOUT      0x01
#define UEP_T_RES_NAK       0x02
#define UEP_T_RES_STALL     0x03

/*============================================================================
 * USB Standard Requests
 *============================================================================*/

#define USB_REQ_GET_STATUS          0x00
#define USB_REQ_CLEAR_FEATURE       0x01
#define USB_REQ_SET_FEATURE         0x03
#define USB_REQ_SET_ADDRESS         0x05
#define USB_REQ_GET_DESCRIPTOR      0x06
#define USB_REQ_SET_DESCRIPTOR      0x07
#define USB_REQ_GET_CONFIGURATION   0x08
#define USB_REQ_SET_CONFIGURATION   0x09
#define USB_REQ_GET_INTERFACE       0x0A
#define USB_REQ_SET_INTERFACE       0x0B
#define USB_REQ_SYNCH_FRAME         0x0C

// Descriptor types
#define USB_DESC_TYPE_DEVICE        0x01
#define USB_DESC_TYPE_CONFIG        0x02
#define USB_DESC_TYPE_STRING        0x03
#define USB_DESC_TYPE_INTERFACE     0x04
#define USB_DESC_TYPE_ENDPOINT      0x05
#define USB_DESC_TYPE_HID           0x21
#define USB_DESC_TYPE_HID_REPORT    0x22

// HID class requests
#define HID_REQ_GET_REPORT          0x01
#define HID_REQ_GET_IDLE            0x02
#define HID_REQ_GET_PROTOCOL        0x03
#define HID_REQ_SET_REPORT          0x09
#define HID_REQ_SET_IDLE            0x0A
#define HID_REQ_SET_PROTOCOL        0x0B

/*============================================================================
 * USB Setup Packet
 *============================================================================*/

typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} USB_SETUP_REQ;

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Initialize USB device
 */
void USB_DeviceInit(void);

/**
 * @brief Enable USB device
 */
void USB_DeviceEnable(void);

/**
 * @brief Disable USB device
 */
void USB_DeviceDisable(void);

/**
 * @brief Set device address
 * @param addr Device address (0-127)
 */
void USB_SetAddress(uint8_t addr);

/**
 * @brief Configure endpoint
 * @param ep Endpoint number (0-7)
 * @param type Endpoint type
 * @param size Max packet size
 */
void USB_EndpointConfig(uint8_t ep, uint8_t type, uint16_t size);

/**
 * @brief Write data to endpoint TX buffer
 * @param ep Endpoint number
 * @param data Data buffer
 * @param len Data length
 * @return Bytes written
 */
uint8_t USB_EndpointWrite(uint8_t ep, const uint8_t *data, uint8_t len);

/**
 * @brief Read data from endpoint RX buffer
 * @param ep Endpoint number
 * @param data Data buffer
 * @param max_len Maximum length to read
 * @return Bytes read
 */
uint8_t USB_EndpointRead(uint8_t ep, uint8_t *data, uint8_t max_len);

/**
 * @brief Stall endpoint
 * @param ep Endpoint number
 */
void USB_EndpointStall(uint8_t ep);

/**
 * @brief Clear endpoint stall
 * @param ep Endpoint number
 */
void USB_EndpointClearStall(uint8_t ep);

/**
 * @brief USB interrupt handler (call from ISR)
 */
void USB_DevTransProcess(void);

/*============================================================================
 * Callback Functions (implement in application)
 *============================================================================*/

/**
 * @brief Called when USB is configured
 */
extern void USB_OnConfigured(void);

/**
 * @brief Called when USB is suspended
 */
extern void USB_OnSuspend(void);

/**
 * @brief Called for class-specific requests
 * @param setup Setup packet
 * @return 0 on success, -1 to stall
 */
extern int USB_OnClassRequest(USB_SETUP_REQ *setup);

/**
 * @brief Called when data received on OUT endpoint
 * @param ep Endpoint number
 * @param data Data buffer
 * @param len Data length
 */
extern void USB_OnDataOut(uint8_t ep, const uint8_t *data, uint8_t len);

/**
 * @brief Called when data sent on IN endpoint
 * @param ep Endpoint number
 */
extern void USB_OnDataIn(uint8_t ep);

#ifdef __cplusplus
}
#endif

#endif /* __CH59X_USBDEV_H__ */
