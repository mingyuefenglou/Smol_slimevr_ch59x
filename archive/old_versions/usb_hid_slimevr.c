/**
 * @file usb_hid_slimevr.c
 * @brief SlimeVR Compatible USB HID Implementation
 * 
 * 直接兼容 SlimeVR 官方软件的 USB HID 协议实现
 * Direct SlimeVR software compatible USB HID protocol
 * 
 * 特点 / Features:
 * - 使用 SlimeVR 官方识别的 VID/PID
 * - 双向 HID 通信 (IN + OUT)
 * - SlimeVR 协议数据包格式
 * - 支持多追踪器数据聚合
 */

#include "usb_hid_slime.h"
#include "hal.h"
#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"
#endif

/*============================================================================
 * SlimeVR USB 配置 / SlimeVR USB Configuration
 *============================================================================*/

// SlimeVR 官方 VID/PID 或兼容 VID/PID
// SlimeVR official or compatible VID/PID
#ifndef USB_VID
#define USB_VID             0x1209      // pid.codes (开源硬件)
#endif
#ifndef USB_PID
#define USB_PID             0x7690      // SlimeVR CH592 专用
#endif

// 端点配置 / Endpoint configuration
#define EP0_SIZE            64          // 控制端点
#define EP1_IN_SIZE         64          // 数据上报 (设备 -> 主机)
#define EP2_OUT_SIZE        64          // 命令接收 (主机 -> 设备)
#define POLL_INTERVAL       1           // 1ms 轮询

/*============================================================================
 * SlimeVR 协议定义 / SlimeVR Protocol Definitions
 *============================================================================*/

// 数据包类型 (设备 -> 主机) / Packet types (Device -> Host)
#define SLIME_PKT_HEARTBEAT      0x00   // 心跳包
#define SLIME_PKT_ROTATION       0x01   // 旋转数据
#define SLIME_PKT_ACCEL          0x02   // 加速度数据
#define SLIME_PKT_BATTERY        0x03   // 电池状态
#define SLIME_PKT_TRACKER_INFO   0x04   // 追踪器信息
#define SLIME_PKT_MAG_ACCURACY   0x05   // 磁力计精度
#define SLIME_PKT_SIGNAL         0x06   // 信号强度
#define SLIME_PKT_TAP            0x07   // 敲击检测
#define SLIME_PKT_ERROR          0x08   // 错误报告
#define SLIME_PKT_BUNDLE         0x10   // 批量数据包

// 命令类型 (主机 -> 设备) / Command types (Host -> Device)
#define SLIME_CMD_CALIBRATE      0x80   // 校准命令
#define SLIME_CMD_RESET          0x81   // 重置命令
#define SLIME_CMD_TARE           0x82   // 归零命令
#define SLIME_CMD_CONFIG         0x83   // 配置命令
#define SLIME_CMD_PING           0x84   // Ping 命令
#define SLIME_CMD_PAIR           0x85   // 配对命令

/*============================================================================
 * USB 描述符 / USB Descriptors
 *============================================================================*/

// 设备描述符 / Device descriptor
static const uint8_t PROGMEM device_desc[] = {
    18,                         // bLength
    0x01,                       // bDescriptorType (Device)
    0x00, 0x02,                 // bcdUSB 2.0
    0x00,                       // bDeviceClass (from interface)
    0x00,                       // bDeviceSubClass
    0x00,                       // bDeviceProtocol
    EP0_SIZE,                   // bMaxPacketSize0
    USB_VID & 0xFF, USB_VID >> 8,   // idVendor
    USB_PID & 0xFF, USB_PID >> 8,   // idProduct
    0x00, 0x01,                 // bcdDevice v1.0
    1,                          // iManufacturer
    2,                          // iProduct
    3,                          // iSerialNumber
    1,                          // bNumConfigurations
};

// 配置描述符 (带 IN 和 OUT 端点) / Configuration descriptor with IN and OUT endpoints
static const uint8_t PROGMEM config_desc[] = {
    // Configuration Descriptor
    9,                          // bLength
    0x02,                       // bDescriptorType
    41, 0,                      // wTotalLength (9+9+9+7+7 = 41)
    1,                          // bNumInterfaces
    1,                          // bConfigurationValue
    0,                          // iConfiguration
    0x80,                       // bmAttributes (bus powered)
    100,                        // bMaxPower (200mA)
    
    // Interface Descriptor
    9,                          // bLength
    0x04,                       // bDescriptorType
    0,                          // bInterfaceNumber
    0,                          // bAlternateSetting
    2,                          // bNumEndpoints (IN + OUT)
    0x03,                       // bInterfaceClass (HID)
    0x00,                       // bInterfaceSubClass (None)
    0x00,                       // bInterfaceProtocol (None)
    4,                          // iInterface
    
    // HID Descriptor
    9,                          // bLength
    0x21,                       // bDescriptorType (HID)
    0x11, 0x01,                 // bcdHID 1.11
    0x00,                       // bCountryCode
    1,                          // bNumDescriptors
    0x22,                       // bDescriptorType (Report)
    47, 0,                      // wDescriptorLength
    
    // Endpoint 1 IN (设备 -> 主机)
    7,                          // bLength
    0x05,                       // bDescriptorType
    0x81,                       // bEndpointAddress (IN 1)
    0x03,                       // bmAttributes (Interrupt)
    EP1_IN_SIZE, 0,             // wMaxPacketSize
    POLL_INTERVAL,              // bInterval (1ms)
    
    // Endpoint 2 OUT (主机 -> 设备)
    7,                          // bLength
    0x05,                       // bDescriptorType
    0x02,                       // bEndpointAddress (OUT 2)
    0x03,                       // bmAttributes (Interrupt)
    EP2_OUT_SIZE, 0,            // wMaxPacketSize
    POLL_INTERVAL,              // bInterval (1ms)
};

// HID 报告描述符 (双向 64 字节) / HID Report descriptor (bidirectional 64 bytes)
static const uint8_t PROGMEM hid_report_desc[] = {
    0x06, 0x00, 0xFF,           // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,                 // Usage (Vendor Usage 1)
    0xA1, 0x01,                 // Collection (Application)
    
    // Input Report (Device -> Host)
    0x09, 0x02,                 //   Usage (Vendor Usage 2)
    0x15, 0x00,                 //   Logical Minimum (0)
    0x26, 0xFF, 0x00,           //   Logical Maximum (255)
    0x75, 0x08,                 //   Report Size (8 bits)
    0x95, 0x40,                 //   Report Count (64)
    0x81, 0x02,                 //   Input (Data, Var, Abs)
    
    // Output Report (Host -> Device)
    0x09, 0x03,                 //   Usage (Vendor Usage 3)
    0x15, 0x00,                 //   Logical Minimum (0)
    0x26, 0xFF, 0x00,           //   Logical Maximum (255)
    0x75, 0x08,                 //   Report Size (8 bits)
    0x95, 0x40,                 //   Report Count (64)
    0x91, 0x02,                 //   Output (Data, Var, Abs)
    
    0xC0,                       // End Collection
};

// 字符串描述符 / String descriptors
static const uint8_t PROGMEM string_lang[] = {4, 0x03, 0x09, 0x04};

static const uint8_t PROGMEM string_manufacturer[] = {
    16, 0x03, 'S', 0, 'l', 0, 'i', 0, 'm', 0, 'e', 0, 'V', 0, 'R', 0
};

static const uint8_t PROGMEM string_product[] = {
    36, 0x03,
    'S', 0, 'l', 0, 'i', 0, 'm', 0, 'e', 0, 'V', 0, 'R', 0, ' ', 0,
    'C', 0, 'H', 0, '5', 0, '9', 0, '2', 0, ' ', 0, 'R', 0, 'X', 0, ' ', 0
};

static const uint8_t PROGMEM string_serial[] = {
    26, 0x03,
    'C', 0, 'H', 0, '5', 0, '9', 0, '2', 0, '_', 0,
    '0', 0, '0', 0, '0', 0, '0', 0, '0', 0, '1', 0
};

static const uint8_t PROGMEM string_interface[] = {
    28, 0x03,
    'S', 0, 'l', 0, 'i', 0, 'm', 0, 'e', 0, 'V', 0, 'R', 0, ' ', 0,
    'H', 0, 'I', 0, 'D', 0, ' ', 0, ' ', 0
};

/*============================================================================
 * 状态和缓冲区 / State and Buffers
 *============================================================================*/

// USB 状态 / USB state
static volatile bool usb_configured = false;
static volatile bool ep1_busy = false;
static volatile uint8_t usb_address = 0;
static volatile uint8_t setup_request = 0;

// 缓冲区 / Buffers
static uint8_t __attribute__((aligned(4))) ep0_buffer[EP0_SIZE];
static uint8_t __attribute__((aligned(4))) ep1_in_buffer[EP1_IN_SIZE];
static uint8_t __attribute__((aligned(4))) ep2_out_buffer[EP2_OUT_SIZE];

// 追踪器数据缓存 / Tracker data cache
#define MAX_CACHED_TRACKERS 24
typedef struct {
    uint8_t  id;
    uint8_t  flags;
    int16_t  quat[4];       // Q15 格式
    int16_t  accel[3];      // mg
    uint8_t  battery;       // 0-100
    int8_t   rssi;          // dBm
    uint32_t last_update;   // ms
} tracker_cache_t;

static tracker_cache_t tracker_cache[MAX_CACHED_TRACKERS];
static uint8_t tracker_count = 0;

// 命令回调 / Command callback
static void (*cmd_callback)(uint8_t cmd, const uint8_t *data, uint8_t len) = NULL;

/*============================================================================
 * SlimeVR 协议实现 / SlimeVR Protocol Implementation
 *============================================================================*/

/**
 * @brief 构建旋转数据包 / Build rotation packet
 */
static int build_rotation_packet(uint8_t *buf, uint8_t tracker_id, 
                                  const int16_t quat[4], uint8_t accuracy)
{
    buf[0] = SLIME_PKT_ROTATION;
    buf[1] = tracker_id;
    buf[2] = accuracy;
    
    // 四元数 (w, x, y, z) 以 int16 Q15 格式
    memcpy(&buf[3], quat, 8);
    
    return 11;
}

/**
 * @brief 构建电池状态包 / Build battery packet
 */
static int build_battery_packet(uint8_t *buf, uint8_t tracker_id, 
                                 uint8_t battery_pct, uint16_t voltage_mv)
{
    buf[0] = SLIME_PKT_BATTERY;
    buf[1] = tracker_id;
    buf[2] = battery_pct;
    buf[3] = voltage_mv & 0xFF;
    buf[4] = voltage_mv >> 8;
    
    return 5;
}

/**
 * @brief 构建追踪器信息包 / Build tracker info packet
 */
static int build_tracker_info_packet(uint8_t *buf, uint8_t tracker_id,
                                      uint8_t imu_type, uint8_t fw_version[3])
{
    buf[0] = SLIME_PKT_TRACKER_INFO;
    buf[1] = tracker_id;
    buf[2] = imu_type;      // 0=Unknown, 1=MPU6050, 2=BMI160, 3=BMI270, 4=ICM42688, 5=ICM45686
    buf[3] = fw_version[0]; // Major
    buf[4] = fw_version[1]; // Minor
    buf[5] = fw_version[2]; // Patch
    buf[6] = 0;             // Board type (CH592 = custom)
    buf[7] = 0;             // MCU type
    
    return 8;
}

/**
 * @brief 构建批量数据包 (多追踪器) / Build bundle packet (multiple trackers)
 */
static int build_bundle_packet(uint8_t *buf)
{
    int offset = 0;
    buf[offset++] = SLIME_PKT_BUNDLE;
    buf[offset++] = 0;  // 追踪器计数 (稍后填充)
    
    uint8_t count = 0;
    uint32_t now = hal_millis();
    
    for (int i = 0; i < MAX_CACHED_TRACKERS && offset < 60; i++) {
        tracker_cache_t *t = &tracker_cache[i];
        
        // 只发送最近 50ms 内更新的追踪器
        if (t->flags && (now - t->last_update) < 50) {
            buf[offset++] = t->id;
            memcpy(&buf[offset], t->quat, 8);
            offset += 8;
            buf[offset++] = t->battery;
            count++;
        }
    }
    
    buf[1] = count;
    return offset;
}

/*============================================================================
 * USB 事件处理 / USB Event Handling
 *============================================================================*/

#ifdef CH59X

// 处理 SETUP 请求 / Handle SETUP request
static void handle_setup_packet(void)
{
    uint8_t *setup = ep0_buffer;
    uint8_t bmRequestType = setup[0];
    uint8_t bRequest = setup[1];
    uint16_t wValue = setup[2] | (setup[3] << 8);
    uint16_t wIndex = setup[4] | (setup[5] << 8);
    uint16_t wLength = setup[6] | (setup[7] << 8);
    
    (void)wIndex;
    
    const uint8_t *desc = NULL;
    uint16_t len = 0;
    
    if ((bmRequestType & 0x60) == 0x00) {
        // Standard request
        switch (bRequest) {
            case 0x06: // GET_DESCRIPTOR
                switch (wValue >> 8) {
                    case 0x01: // Device
                        desc = device_desc;
                        len = sizeof(device_desc);
                        break;
                    case 0x02: // Configuration
                        desc = config_desc;
                        len = sizeof(config_desc);
                        break;
                    case 0x03: // String
                        switch (wValue & 0xFF) {
                            case 0: desc = string_lang; len = sizeof(string_lang); break;
                            case 1: desc = string_manufacturer; len = sizeof(string_manufacturer); break;
                            case 2: desc = string_product; len = sizeof(string_product); break;
                            case 3: desc = string_serial; len = sizeof(string_serial); break;
                            case 4: desc = string_interface; len = sizeof(string_interface); break;
                        }
                        break;
                    case 0x22: // HID Report
                        desc = hid_report_desc;
                        len = sizeof(hid_report_desc);
                        break;
                }
                break;
            
            case 0x05: // SET_ADDRESS
                usb_address = wValue & 0x7F;
                // 地址在状态阶段后设置
                break;
            
            case 0x09: // SET_CONFIGURATION
                usb_configured = (wValue != 0);
                break;
        }
    }
    else if ((bmRequestType & 0x60) == 0x20) {
        // Class request (HID)
        switch (bRequest) {
            case 0x01: // GET_REPORT
                // 返回当前追踪器数据
                len = build_bundle_packet(ep0_buffer);
                desc = ep0_buffer;
                break;
            
            case 0x09: // SET_REPORT
                // 接收命令 (在 OUT 阶段处理)
                break;
        }
    }
    
    if (desc && len > 0) {
        if (len > wLength) len = wLength;
        if (len > EP0_SIZE) len = EP0_SIZE;
        memcpy(ep0_buffer, desc, len);
        R8_UEP0_T_LEN = len;
        R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
    } else {
        R8_UEP0_T_LEN = 0;
        R8_UEP0_CTRL = RB_UEP_R_TOG | RB_UEP_T_TOG | UEP_R_RES_ACK | UEP_T_RES_ACK;
    }
}

// 处理接收到的命令 / Handle received command
static void handle_out_data(const uint8_t *data, uint8_t len)
{
    if (len < 1) return;
    
    uint8_t cmd = data[0];
    
    // 内部命令处理 / Internal command handling
    switch (cmd) {
        case SLIME_CMD_PING:
            // 响应 Ping
            {
                uint8_t pong[2] = {SLIME_PKT_HEARTBEAT, 0x00};
                usb_hid_write(pong, 2);
            }
            break;
        
        case SLIME_CMD_RESET:
            // 重置所有追踪器
            memset(tracker_cache, 0, sizeof(tracker_cache));
            tracker_count = 0;
            break;
        
        default:
            // 调用用户回调
            if (cmd_callback) {
                cmd_callback(cmd, data + 1, len - 1);
            }
            break;
    }
}

// USB 中断处理 / USB interrupt handler
__attribute__((interrupt("WCH-Interrupt-fast")))
void USB_IRQHandler(void)
{
    uint8_t int_flag = R8_USB_INT_FG;
    
    if (int_flag & RB_UIF_TRANSFER) {
        uint8_t int_st = R8_USB_INT_ST;
        uint8_t token = int_st & MASK_UIS_TOKEN;
        uint8_t endp = int_st & MASK_UIS_ENDP;
        
        switch (endp) {
            case 0: // EP0
                if (token == UIS_TOKEN_SETUP) {
                    handle_setup_packet();
                }
                else if (token == UIS_TOKEN_IN) {
                    if (usb_address) {
                        R8_USB_DEV_AD = usb_address;
                        usb_address = 0;
                    }
                    R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                }
                else if (token == UIS_TOKEN_OUT) {
                    R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
                }
                break;
            
            case 1: // EP1 IN
                if (token == UIS_TOKEN_IN) {
                    ep1_busy = false;
                    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    R8_UEP1_CTRL ^= RB_UEP_T_TOG;
                }
                break;
            
            case 2: // EP2 OUT
                if (token == UIS_TOKEN_OUT) {
                    uint8_t len = R8_USB_RX_LEN;
                    handle_out_data(ep2_out_buffer, len);
                    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_ACK;
                    R8_UEP2_CTRL ^= RB_UEP_R_TOG;
                }
                break;
        }
        
        R8_USB_INT_FG = RB_UIF_TRANSFER;
    }
    
    if (int_flag & RB_UIF_BUS_RST) {
        usb_configured = false;
        usb_address = 0;
        R8_USB_DEV_AD = 0;
        
        // 重置端点
        R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
        R8_UEP1_CTRL = UEP_T_RES_NAK;
        R8_UEP2_CTRL = UEP_R_RES_ACK;
        
        R8_USB_INT_FG = RB_UIF_BUS_RST;
    }
    
    if (int_flag & RB_UIF_SUSPEND) {
        R8_USB_INT_FG = RB_UIF_SUSPEND;
    }
}

#endif // CH59X

/*============================================================================
 * API 实现 / API Implementation
 *============================================================================*/

int usb_hid_init(void)
{
#ifdef CH59X
    // 启用 USB 时钟 / Enable USB clock
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    R8_SLP_CLK_OFF0 &= ~RB_SLP_CLK_USB;
    R8_SAFE_ACCESS_SIG = 0;
    
    // 配置 USB 设备模式 / Configure USB device mode
    R8_USB_CTRL = 0;
    R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN;
    R8_USB_DEV_AD = 0;
    
    // 配置端点 DMA / Configure endpoint DMA
    R16_UEP0_DMA = (uint16_t)(uint32_t)ep0_buffer;
    R16_UEP1_DMA = (uint16_t)(uint32_t)ep1_in_buffer;
    R16_UEP2_DMA = (uint16_t)(uint32_t)ep2_out_buffer;
    
    // 配置端点模式 / Configure endpoint modes
    R8_UEP4_1_MOD = RB_UEP1_TX_EN;               // EP1 TX only
    R8_UEP2_3_MOD = RB_UEP2_RX_EN;               // EP2 RX only
    
    // 初始化端点控制 / Initialize endpoint control
    R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
    R8_UEP1_CTRL = UEP_T_RES_NAK;
    R8_UEP2_CTRL = UEP_R_RES_ACK;
    
    // 启用中断 / Enable interrupts
    R8_USB_INT_EN = RB_UIE_TRANSFER | RB_UIE_BUS_RST | RB_UIE_SUSPEND;
    PFIC_EnableIRQ(USB_IRQn);
    
    // 启用上拉 / Enable pull-up
    R8_USB_CTRL |= RB_UC_DEV_PU_EN;
#endif
    
    // 初始化追踪器缓存 / Initialize tracker cache
    memset(tracker_cache, 0, sizeof(tracker_cache));
    tracker_count = 0;
    
    return 0;
}

bool usb_hid_ready(void)
{
    return usb_configured;
}

bool usb_hid_busy(void)
{
    return ep1_busy;
}

int usb_hid_write(const uint8_t *data, uint8_t len)
{
    if (!usb_configured) return -1;
    if (ep1_busy) return -2;
    if (len > EP1_IN_SIZE) len = EP1_IN_SIZE;
    
    memcpy(ep1_in_buffer, data, len);
    ep1_busy = true;
    
#ifdef CH59X
    R8_UEP1_T_LEN = len;
    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
#endif
    
    return len;
}

void usb_hid_set_callback(void (*cb)(uint8_t, const uint8_t*, uint8_t))
{
    cmd_callback = cb;
}

/*============================================================================
 * 追踪器数据 API / Tracker Data API
 *============================================================================*/

/**
 * @brief 更新追踪器数据 / Update tracker data
 */
void usb_hid_update_tracker(uint8_t id, const int16_t quat[4], 
                            const int16_t accel[3], uint8_t battery, int8_t rssi)
{
    // 查找或分配缓存槽 / Find or allocate cache slot
    tracker_cache_t *t = NULL;
    
    for (int i = 0; i < MAX_CACHED_TRACKERS; i++) {
        if (tracker_cache[i].id == id && tracker_cache[i].flags) {
            t = &tracker_cache[i];
            break;
        }
    }
    
    if (!t) {
        // 分配新槽 / Allocate new slot
        for (int i = 0; i < MAX_CACHED_TRACKERS; i++) {
            if (!tracker_cache[i].flags) {
                t = &tracker_cache[i];
                t->id = id;
                tracker_count++;
                break;
            }
        }
    }
    
    if (t) {
        t->flags = 1;
        memcpy(t->quat, quat, 8);
        if (accel) memcpy(t->accel, accel, 6);
        t->battery = battery;
        t->rssi = rssi;
        t->last_update = hal_millis();
    }
}

/**
 * @brief 发送追踪器数据到主机 / Send tracker data to host
 */
int usb_hid_send_tracker_data(uint8_t id, const int16_t quat[4], 
                               uint8_t battery, uint8_t accuracy)
{
    if (!usb_configured || ep1_busy) return -1;
    
    int len = build_rotation_packet(ep1_in_buffer, id, quat, accuracy);
    
    // 附加电池信息 / Append battery info
    ep1_in_buffer[len++] = battery;
    
    ep1_busy = true;
    
#ifdef CH59X
    R8_UEP1_T_LEN = len;
    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
#endif
    
    return len;
}

/**
 * @brief 发送批量追踪器数据 / Send bundle tracker data
 */
int usb_hid_send_bundle(void)
{
    if (!usb_configured || ep1_busy) return -1;
    
    int len = build_bundle_packet(ep1_in_buffer);
    if (len <= 2) return 0;  // 无数据 / No data
    
    ep1_busy = true;
    
#ifdef CH59X
    R8_UEP1_T_LEN = len;
    R8_UEP1_CTRL = (R8_UEP1_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
#endif
    
    return len;
}

void usb_hid_task(void)
{
    // 周期性发送追踪器数据 / Periodic tracker data transmission
    static uint32_t last_send = 0;
    uint32_t now = hal_millis();
    
    if (usb_configured && !ep1_busy && (now - last_send) >= 5) {
        usb_hid_send_bundle();
        last_send = now;
    }
}
