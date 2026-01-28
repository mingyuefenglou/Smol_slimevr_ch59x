/**
 * @file usb_hid_slime.c
 * @brief USB HID Implementation for SlimeVR Receiver
 * 
 * CH592 USB Full-Speed Device with HID class
 * 完整实现 USB Setup 处理和 HID 协议
 */

#include "usb_hid_slime.h"
#include "hal.h"
#include "version.h"
#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"
#include "ch59x_usb_regs.h"  // 补充 USB 寄存器定义
#endif

/*============================================================================
 * USB 描述符定义 / USB Descriptors
 *============================================================================*/

// HID Report 描述符 (64字节 Vendor 定义报告)
static const uint8_t usb_hid_report_desc[] = {
    0x06, 0x00, 0xFF,       // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,             // Usage (Vendor Usage 1)
    0xA1, 0x01,             // Collection (Application)
    
    // Input Report (64 bytes)
    0x09, 0x01,             //   Usage (Vendor Usage 1)
    0x15, 0x00,             //   Logical Minimum (0)
    0x26, 0xFF, 0x00,       //   Logical Maximum (255)
    0x75, 0x08,             //   Report Size (8 bits)
    0x95, 0x40,             //   Report Count (64)
    0x81, 0x02,             //   Input (Data, Var, Abs)
    
    // Output Report (64 bytes)
    0x09, 0x02,             //   Usage (Vendor Usage 2)
    0x15, 0x00,             //   Logical Minimum (0)
    0x26, 0xFF, 0x00,       //   Logical Maximum (255)
    0x75, 0x08,             //   Report Size (8 bits)
    0x95, 0x40,             //   Report Count (64)
    0x91, 0x02,             //   Output (Data, Var, Abs)
    
    0xC0,                   // End Collection
};

// 设备描述符
static const uint8_t usb_device_desc[] = {
    18,                     // bLength
    0x01,                   // bDescriptorType (Device)
    0x00, 0x02,             // bcdUSB 2.0
    0x00,                   // bDeviceClass (from interface)
    0x00,                   // bDeviceSubClass
    0x00,                   // bDeviceProtocol
    64,                     // bMaxPacketSize0
    (USB_VID & 0xFF), (USB_VID >> 8),   // idVendor (0x1209)
    (USB_PID & 0xFF), (USB_PID >> 8),   // idProduct (0x7690)
    0x01, 0x00,             // bcdDevice 1.0
    1,                      // iManufacturer
    2,                      // iProduct
    3,                      // iSerialNumber
    1,                      // bNumConfigurations
};

// 配置描述符 (包含接口、HID、端点)
static const uint8_t usb_config_desc[] = {
    // Configuration Descriptor
    9,                      // bLength
    0x02,                   // bDescriptorType (Configuration)
    41, 0,                  // wTotalLength (9+9+9+7+7=41)
    1,                      // bNumInterfaces
    1,                      // bConfigurationValue
    0,                      // iConfiguration
    0x80,                   // bmAttributes (bus powered)
    50,                     // bMaxPower (100mA)
    
    // Interface Descriptor
    9,                      // bLength
    0x04,                   // bDescriptorType (Interface)
    0,                      // bInterfaceNumber
    0,                      // bAlternateSetting
    2,                      // bNumEndpoints (IN + OUT)
    0x03,                   // bInterfaceClass (HID)
    0x00,                   // bInterfaceSubClass (None)
    0x00,                   // bInterfaceProtocol (None)
    0,                      // iInterface
    
    // HID Descriptor
    9,                      // bLength
    0x21,                   // bDescriptorType (HID)
    0x11, 0x01,             // bcdHID 1.11
    0x00,                   // bCountryCode
    1,                      // bNumDescriptors
    0x22,                   // bDescriptorType (Report)
    sizeof(usb_hid_report_desc), 0,  // wDescriptorLength
    
    // Endpoint IN Descriptor
    7,                      // bLength
    0x05,                   // bDescriptorType (Endpoint)
    0x81,                   // bEndpointAddress (IN 1)
    0x03,                   // bmAttributes (Interrupt)
    USB_HID_EP_SIZE, 0,     // wMaxPacketSize (64)
    USB_HID_INTERVAL,       // bInterval (5ms)
    
    // Endpoint OUT Descriptor
    7,                      // bLength
    0x05,                   // bDescriptorType (Endpoint)
    0x01,                   // bEndpointAddress (OUT 1)
    0x03,                   // bmAttributes (Interrupt)
    USB_HID_EP_SIZE, 0,     // wMaxPacketSize (64)
    USB_HID_INTERVAL,       // bInterval (5ms)
};

// 字符串描述符
static const uint8_t usb_string_lang[] = {4, 0x03, 0x09, 0x04};  // English US

static const uint8_t usb_string_mfg[] = {
    16, 0x03,
    'S', 0, 'l', 0, 'i', 0, 'm', 0, 'e', 0, 'V', 0, 'R', 0,
};

static const uint8_t usb_string_prod[] = {
    36, 0x03,
    'C', 0, 'H', 0, '5', 0, '9', 0, 'X', 0, ' ', 0,
    'R', 0, 'e', 0, 'c', 0, 'e', 0, 'i', 0, 'v', 0, 'e', 0, 'r', 0,
    ' ', 0, ' ', 0, ' ', 0,
};

static const uint8_t usb_string_serial[] = {
    26, 0x03,
    '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '0', 0,
    '0', 0, '0', 0, '0', 0, '1', 0,
};

/*============================================================================
 * USB Setup 请求定义
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

#define HID_REQ_GET_REPORT          0x01
#define HID_REQ_GET_IDLE            0x02
#define HID_REQ_GET_PROTOCOL        0x03
#define HID_REQ_SET_REPORT          0x09
#define HID_REQ_SET_IDLE            0x0A
#define HID_REQ_SET_PROTOCOL        0x0B

#define USB_DESC_TYPE_DEVICE        0x01
#define USB_DESC_TYPE_CONFIG        0x02
#define USB_DESC_TYPE_STRING        0x03
#define USB_DESC_TYPE_HID           0x21
#define USB_DESC_TYPE_HID_REPORT    0x22

#define USB_REQ_TYPE_STANDARD       0x00
#define USB_REQ_TYPE_CLASS          0x20
#define USB_REQ_TYPE_VENDOR         0x40
#define USB_REQ_TYPE_MASK           0x60

#define USB_REQ_RECIP_DEVICE        0x00
#define USB_REQ_RECIP_INTERFACE     0x01
#define USB_REQ_RECIP_ENDPOINT      0x02
#define USB_REQ_RECIP_MASK          0x1F

/*============================================================================
 * 状态变量
 *============================================================================*/

static volatile uint8_t usb_dev_addr = 0;
static volatile bool usb_configured = false;
static volatile bool usb_suspended = false;
static volatile uint8_t usb_config_value = 0;
static volatile bool ep1_in_busy = false;

static uint8_t hid_idle_rate = 0;
static uint8_t hid_protocol = 1;

static uint8_t __attribute__((aligned(4))) ep0_buffer[64];
static uint8_t __attribute__((aligned(4))) ep1_in_buffer[USB_HID_EP_SIZE];
static uint8_t __attribute__((aligned(4))) ep1_out_buffer[USB_HID_EP_SIZE];

static struct {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} setup_packet;

static const uint8_t *ep0_tx_ptr = NULL;
static uint16_t ep0_tx_len = 0;

static usb_hid_rx_callback_t rx_callback = NULL;

/*============================================================================
 * 内部函数
 *============================================================================*/

#ifdef CH59X

static void ep0_send_data(const uint8_t *data, uint16_t len)
{
    if (len > 64) len = 64;
    memcpy(ep0_buffer, data, len);
    R8_UEP0_T_LEN = len;
    R8_UEP0_CTRL = (R8_UEP0_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
}

static void ep0_send_zlp(void)
{
    R8_UEP0_T_LEN = 0;
    R8_UEP0_CTRL = (R8_UEP0_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
}

static void ep0_stall(void)
{
    R8_UEP0_CTRL = UEP_T_RES_STALL | UEP_R_RES_STALL;
}

static void handle_get_descriptor(void)
{
    uint8_t desc_type = setup_packet.wValue >> 8;
    uint8_t desc_index = setup_packet.wValue & 0xFF;
    const uint8_t *desc = NULL;
    uint16_t len = 0;
    
    switch (desc_type) {
        case USB_DESC_TYPE_DEVICE:
            desc = usb_device_desc;
            len = sizeof(usb_device_desc);
            break;
            
        case USB_DESC_TYPE_CONFIG:
            desc = usb_config_desc;
            len = sizeof(usb_config_desc);
            break;
            
        case USB_DESC_TYPE_STRING:
            switch (desc_index) {
                case 0: desc = usb_string_lang; len = sizeof(usb_string_lang); break;
                case 1: desc = usb_string_mfg; len = sizeof(usb_string_mfg); break;
                case 2: desc = usb_string_prod; len = sizeof(usb_string_prod); break;
                case 3: desc = usb_string_serial; len = sizeof(usb_string_serial); break;
                default: ep0_stall(); return;
            }
            break;
            
        case USB_DESC_TYPE_HID_REPORT:
            desc = usb_hid_report_desc;
            len = sizeof(usb_hid_report_desc);
            break;
            
        case USB_DESC_TYPE_HID:
            desc = usb_config_desc + 18;
            len = 9;
            break;
            
        default:
            ep0_stall();
            return;
    }
    
    if (len > setup_packet.wLength) {
        len = setup_packet.wLength;
    }
    
    ep0_tx_ptr = desc;
    ep0_tx_len = len;
    
    uint16_t chunk = (len > 64) ? 64 : len;
    ep0_send_data(desc, chunk);
    ep0_tx_ptr += chunk;
    ep0_tx_len -= chunk;
}

static void handle_standard_device_request(void)
{
    switch (setup_packet.bRequest) {
        case USB_REQ_GET_STATUS:
            ep0_buffer[0] = 0x00;
            ep0_buffer[1] = 0x00;
            ep0_send_data(ep0_buffer, 2);
            break;
            
        case USB_REQ_CLEAR_FEATURE:
            // 设备级 CLEAR_FEATURE: 仅支持 DEVICE_REMOTE_WAKEUP (feature=1)
            if (setup_packet.wValue == 1) {
                // Remote wakeup disabled (no action needed for this device)
            }
            ep0_send_zlp();
            break;
            
        case USB_REQ_SET_FEATURE:
            // 设备级 SET_FEATURE: 仅支持 DEVICE_REMOTE_WAKEUP (feature=1)
            if (setup_packet.wValue == 1) {
                // Remote wakeup enabled (no action needed for this device)
            } else if (setup_packet.wValue == 2) {
                // TEST_MODE - not supported, stall
                ep0_stall();
                return;
            }
            ep0_send_zlp();
            break;
            
        case USB_REQ_SET_ADDRESS:
            usb_dev_addr = setup_packet.wValue & 0x7F;
            ep0_send_zlp();
            break;
            
        case USB_REQ_GET_DESCRIPTOR:
            handle_get_descriptor();
            break;
            
        case USB_REQ_GET_CONFIGURATION:
            ep0_buffer[0] = usb_config_value;
            ep0_send_data(ep0_buffer, 1);
            break;
            
        case USB_REQ_SET_CONFIGURATION:
            usb_config_value = setup_packet.wValue & 0xFF;
            usb_configured = (usb_config_value != 0);
            if (usb_configured) {
                R8_UEP1_CTRL = UEP_T_RES_NAK | UEP_R_RES_ACK;
                ep1_in_busy = false;
            }
            ep0_send_zlp();
            break;
            
        default:
            ep0_stall();
            break;
    }
}

static void handle_standard_interface_request(void)
{
    switch (setup_packet.bRequest) {
        case USB_REQ_GET_STATUS:
            ep0_buffer[0] = 0x00;
            ep0_buffer[1] = 0x00;
            ep0_send_data(ep0_buffer, 2);
            break;
            
        case USB_REQ_GET_INTERFACE:
            ep0_buffer[0] = 0x00;
            ep0_send_data(ep0_buffer, 1);
            break;
            
        case USB_REQ_SET_INTERFACE:
            ep0_send_zlp();
            break;
            
        case USB_REQ_GET_DESCRIPTOR:
            handle_get_descriptor();
            break;
            
        default:
            ep0_stall();
            break;
    }
}

static void handle_hid_class_request(void)
{
    switch (setup_packet.bRequest) {
        case HID_REQ_GET_REPORT:
            memset(ep0_buffer, 0, USB_HID_EP_SIZE);
            ep0_send_data(ep0_buffer, (setup_packet.wLength > 64) ? 64 : setup_packet.wLength);
            break;
            
        case HID_REQ_SET_REPORT:
            R8_UEP0_CTRL = (R8_UEP0_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_ACK;
            break;
            
        case HID_REQ_GET_IDLE:
            ep0_buffer[0] = hid_idle_rate;
            ep0_send_data(ep0_buffer, 1);
            break;
            
        case HID_REQ_SET_IDLE:
            hid_idle_rate = (setup_packet.wValue >> 8) & 0xFF;
            ep0_send_zlp();
            break;
            
        case HID_REQ_GET_PROTOCOL:
            ep0_buffer[0] = hid_protocol;
            ep0_send_data(ep0_buffer, 1);
            break;
            
        case HID_REQ_SET_PROTOCOL:
            hid_protocol = setup_packet.wValue & 0xFF;
            ep0_send_zlp();
            break;
            
        default:
            ep0_stall();
            break;
    }
}

static void handle_standard_endpoint_request(void)
{
    uint8_t ep_addr = setup_packet.wIndex & 0x0F;
    
    switch (setup_packet.bRequest) {
        case USB_REQ_GET_STATUS:
            // 返回端点 HALT 状态
            ep0_buffer[0] = 0x00;  // 假设未 halt
            ep0_buffer[1] = 0x00;
            if (ep_addr == 1) {
                // 检查 EP1 是否 stalled
                if ((R8_UEP1_CTRL & MASK_UEP_T_RES) == UEP_T_RES_STALL) {
                    ep0_buffer[0] = 0x01;
                }
            }
            ep0_send_data(ep0_buffer, 2);
            break;
            
        case USB_REQ_CLEAR_FEATURE:
            // ENDPOINT_HALT (feature=0)
            if (setup_packet.wValue == 0) {
                if (ep_addr == 1) {
                    // Clear EP1 halt, reset data toggle
                    R8_UEP1_CTRL = UEP_T_RES_NAK | UEP_R_RES_ACK;
                }
            }
            ep0_send_zlp();
            break;
            
        case USB_REQ_SET_FEATURE:
            // ENDPOINT_HALT (feature=0)
            if (setup_packet.wValue == 0) {
                if (ep_addr == 1) {
                    // Set EP1 halt
                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_STALL;
                }
            }
            ep0_send_zlp();
            break;
            
        default:
            ep0_stall();
            break;
    }
}

static void handle_setup_packet(void)
{
    uint8_t *p = (uint8_t *)ep0_buffer;
    setup_packet.bmRequestType = p[0];
    setup_packet.bRequest = p[1];
    setup_packet.wValue = p[2] | (p[3] << 8);
    setup_packet.wIndex = p[4] | (p[5] << 8);
    setup_packet.wLength = p[6] | (p[7] << 8);
    
    uint8_t req_type = setup_packet.bmRequestType & USB_REQ_TYPE_MASK;
    uint8_t recipient = setup_packet.bmRequestType & USB_REQ_RECIP_MASK;
    
    if (req_type == USB_REQ_TYPE_STANDARD) {
        if (recipient == USB_REQ_RECIP_DEVICE) {
            handle_standard_device_request();
        } else if (recipient == USB_REQ_RECIP_INTERFACE) {
            handle_standard_interface_request();
        } else if (recipient == USB_REQ_RECIP_ENDPOINT) {
            handle_standard_endpoint_request();
        } else {
            ep0_stall();
        }
    } else if (req_type == USB_REQ_TYPE_CLASS) {
        if (recipient == USB_REQ_RECIP_INTERFACE) {
            handle_hid_class_request();
        } else {
            ep0_stall();
        }
    } else {
        ep0_stall();
    }
}

// USB_IRQHandler 在 usb_msc.c 中定义（Tracker目标）
// 这里只定义Receiver版本的USB中断处理
#if defined(BUILD_RECEIVER)
__attribute__((interrupt("WCH-Interrupt-fast")))
void USB_IRQHandler(void)
{
    uint8_t int_flag = R8_USB_INT_FG;
    uint8_t int_st = R8_USB_INT_ST;
    
    if (int_flag & RB_UIF_TRANSFER) {
        uint8_t token = (int_st & MASK_UIS_TOKEN);
        uint8_t endp = (int_st & MASK_UIS_ENDP);
        
        switch (endp) {
            case 0:
                switch (token) {
                    case UIS_TOKEN_SETUP:
                        memcpy(ep0_buffer, (void *)R16_UEP0_DMA, 8);
                        R8_UEP0_CTRL = UEP_T_RES_NAK | UEP_R_RES_NAK;
                        handle_setup_packet();
                        break;
                        
                    case UIS_TOKEN_IN:
                        if (usb_dev_addr != 0 && R8_USB_DEV_AD == 0) {
                            R8_USB_DEV_AD = usb_dev_addr;
                        }
                        if (ep0_tx_len > 0) {
                            uint16_t chunk = (ep0_tx_len > 64) ? 64 : ep0_tx_len;
                            ep0_send_data(ep0_tx_ptr, chunk);
                            ep0_tx_ptr += chunk;
                            ep0_tx_len -= chunk;
                        } else {
                            R8_UEP0_CTRL = (R8_UEP0_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_ACK;
                        }
                        break;
                        
                    case UIS_TOKEN_OUT:
                        R8_UEP0_CTRL = UEP_T_RES_NAK | UEP_R_RES_NAK;
                        break;
                }
                break;
                
            case 1:
                switch (token) {
                    case UIS_TOKEN_IN:
                        ep1_in_busy = false;
                        R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                        break;
                        
                    case UIS_TOKEN_OUT:
                        if (rx_callback) {
                            uint8_t len = R8_USB_RX_LEN;
                            // 防止缓冲区溢出：限制长度不超过端点大小
                            if (len > USB_HID_EP_SIZE) {
                                len = USB_HID_EP_SIZE;
                            }
                            rx_callback(ep1_out_buffer, len);
                        }
                        R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_ACK;
                        break;
                }
                break;
                
#ifdef BUILD_RECEIVER
            case 2:
                // Receiver目标：同时支持MSC（端点2）用于bootloader模式
                {
                    extern void usb_msc_handle_ep2(uint8_t token);
                    usb_msc_handle_ep2(token);
                }
                break;
#endif
        }
        
        R8_USB_INT_FG = RB_UIF_TRANSFER;
    }
    
    if (int_flag & RB_UIF_BUS_RST) {
        R8_USB_DEV_AD = 0;
        usb_dev_addr = 0;
        usb_configured = false;
        usb_config_value = 0;
        ep1_in_busy = false;
        R8_UEP0_CTRL = UEP_T_RES_NAK | UEP_R_RES_ACK;
        R8_UEP1_CTRL = UEP_T_RES_NAK | UEP_R_RES_ACK;
        R8_USB_INT_FG = RB_UIF_BUS_RST;
    }
    
    if (int_flag & RB_UIF_SUSPEND) {
        usb_suspended = (R8_USB_MIS_ST & RB_UMS_SUSPEND) ? true : false;
        R8_USB_INT_FG = RB_UIF_SUSPEND;
    }
}
#endif  // BUILD_RECEIVER

#endif // CH59X

/*============================================================================
 * 公共 API
 *============================================================================*/

int usb_hid_init(void)
{
#ifdef CH59X
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    R8_SLP_CLK_OFF0 &= ~RB_SLP_CLK_USB;
    R8_SAFE_ACCESS_SIG = 0;
    
    R16_UEP0_DMA = (uint16_t)(uint32_t)ep0_buffer;
    R16_UEP1_DMA = (uint16_t)(uint32_t)ep1_in_buffer;
    R16_UEP2_DMA = (uint16_t)(uint32_t)ep1_out_buffer;
    
    R8_UEP4_1_MOD = RB_UEP1_TX_EN | RB_UEP1_RX_EN;
    R8_UEP0_CTRL = UEP_T_RES_NAK | UEP_R_RES_ACK;
    R8_UEP1_CTRL = UEP_T_RES_NAK | UEP_R_RES_ACK;
    
    R8_USB_CTRL = 0;
    R8_USB_DEV_AD = 0;
    R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN;
    
    R8_USB_INT_EN = RB_UIE_TRANSFER | RB_UIE_BUS_RST | RB_UIE_SUSPEND;
    PFIC_EnableIRQ(USB_IRQn);
    
    R8_USB_CTRL |= RB_UC_DEV_PU_EN;
#endif
    
    return 0;
}

void usb_hid_deinit(void)
{
#ifdef CH59X
    PFIC_DisableIRQ(USB_IRQn);
    R8_USB_CTRL = 0;
    usb_configured = false;
#endif
}

bool usb_hid_ready(void)
{
    return usb_configured && !usb_suspended;
}

bool usb_hid_connected(void)
{
    return usb_configured;
}

bool usb_hid_suspended(void)
{
    return usb_suspended;
}

bool usb_hid_busy(void)
{
    return ep1_in_busy;
}

int usb_hid_write(const uint8_t *data, uint8_t len)
{
    if (!data) return -4;
    if (len == 0) return -3;
    if (!usb_configured) return -1;
    if (ep1_in_busy) return -2;
    if (len > USB_HID_EP_SIZE) len = USB_HID_EP_SIZE;
    
    memcpy(ep1_in_buffer, data, len);
    ep1_in_busy = true;
    
#ifdef CH59X
    R8_UEP1_T_LEN = len;
    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
#endif
    
    return len;
}

int usb_hid_write_blocking(const uint8_t *data, uint8_t len, uint32_t timeout_ms)
{
    uint32_t start = hal_get_tick_ms();
    
    while (ep1_in_busy) {
        if (hal_get_tick_ms() - start > timeout_ms) {
            return -3;
        }
    }
    
    return usb_hid_write(data, len);
}

void usb_hid_set_rx_callback(usb_hid_rx_callback_t callback)
{
    rx_callback = callback;
}

void usb_hid_task(void)
{
    // 周期性处理
}
