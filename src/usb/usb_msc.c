/**
 * @file usb_msc.c
 * @brief USB Mass Storage Class 驱动 (用于 UF2 DFU)
 * 
 * 实现虚拟 FAT12 文件系统，支持拖放 UF2 固件升级
 * 
 * RAM 使用: ~512 bytes
 * Flash 使用: ~3KB
 */

#include "usb_bootloader.h"
#include "hal.h"
#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"
#include "ch59x_usb_regs.h"
#endif

/*============================================================================
 * USB MSC 常量
 *============================================================================*/

// MSC Class-Specific Requests
#define MSC_REQ_RESET           0xFF
#define MSC_REQ_GET_MAX_LUN     0xFE

// MSC Subclass/Protocol
#define MSC_SUBCLASS_SCSI       0x06
#define MSC_PROTOCOL_BOT        0x50    // Bulk-Only Transport

// SCSI Commands
#define SCSI_TEST_UNIT_READY    0x00
#define SCSI_REQUEST_SENSE      0x03
#define SCSI_INQUIRY            0x12
#define SCSI_MODE_SENSE_6       0x1A
#define SCSI_START_STOP_UNIT    0x1B
#define SCSI_PREVENT_ALLOW      0x1E
#define SCSI_READ_CAPACITY_10   0x25
#define SCSI_READ_10            0x28
#define SCSI_WRITE_10           0x2A
#define SCSI_MODE_SENSE_10      0x5A

// CBW (Command Block Wrapper)
#define CBW_SIGNATURE           0x43425355
#define CBW_SIZE                31

// CSW (Command Status Wrapper)
#define CSW_SIGNATURE           0x53425355
#define CSW_SIZE                13
#define CSW_STATUS_PASSED       0x00
#define CSW_STATUS_FAILED       0x01
#define CSW_STATUS_PHASE_ERROR  0x02

/*============================================================================
 * USB 描述符
 *============================================================================*/

// 设备描述符
static const uint8_t msc_device_desc[] = {
    18,                     // bLength
    0x01,                   // bDescriptorType (Device)
    0x00, 0x02,             // bcdUSB 2.0
    0x00,                   // bDeviceClass (defined in interface)
    0x00,                   // bDeviceSubClass
    0x00,                   // bDeviceProtocol
    64,                     // bMaxPacketSize0
    0x09, 0x12,             // idVendor (0x1209 - pid.codes)
    0x91, 0x76,             // idProduct (0x7691)
    0x01, 0x00,             // bcdDevice
    1,                      // iManufacturer
    2,                      // iProduct
    3,                      // iSerialNumber
    1,                      // bNumConfigurations
};

// 配置描述符
static const uint8_t msc_config_desc[] = {
    // Configuration Descriptor
    9,                      // bLength
    0x02,                   // bDescriptorType
    32, 0,                  // wTotalLength (9+9+7+7)
    1,                      // bNumInterfaces
    1,                      // bConfigurationValue
    0,                      // iConfiguration
    0x80,                   // bmAttributes (bus powered)
    250,                    // bMaxPower (500mA)
    
    // Interface Descriptor
    9,                      // bLength
    0x04,                   // bDescriptorType
    0,                      // bInterfaceNumber
    0,                      // bAlternateSetting
    2,                      // bNumEndpoints
    0x08,                   // bInterfaceClass (Mass Storage)
    MSC_SUBCLASS_SCSI,      // bInterfaceSubClass
    MSC_PROTOCOL_BOT,       // bInterfaceProtocol
    0,                      // iInterface
    
    // Endpoint OUT
    7,                      // bLength
    0x05,                   // bDescriptorType
    0x02,                   // bEndpointAddress (OUT 2)
    0x02,                   // bmAttributes (Bulk)
    64, 0,                  // wMaxPacketSize
    0,                      // bInterval
    
    // Endpoint IN
    7,                      // bLength
    0x05,                   // bDescriptorType
    0x82,                   // bEndpointAddress (IN 2)
    0x02,                   // bmAttributes (Bulk)
    64, 0,                  // wMaxPacketSize
    0,                      // bInterval
};

// 字符串描述符
static const uint8_t msc_string_lang[] = {4, 0x03, 0x09, 0x04};
static const uint8_t msc_string_mfg[] = {16, 0x03, 'S',0,'l',0,'i',0,'m',0,'e',0,'V',0,'R',0};
static const uint8_t msc_string_prod[] = {20, 0x03, 'C',0,'H',0,'5',0,'9',0,'X',0,' ',0,'D',0,'F',0,'U',0};
static const uint8_t msc_string_serial[] = {18, 0x03, '0',0,'0',0,'0',0,'0',0,'0',0,'0',0,'0',0,'1',0};

/*============================================================================
 * SCSI Inquiry 响应
 *============================================================================*/

static const uint8_t scsi_inquiry_data[] = {
    0x00,                   // Peripheral Device Type (SBC)
    0x80,                   // RMB (Removable)
    0x02,                   // Version (SPC-2)
    0x02,                   // Response Data Format
    0x1F,                   // Additional Length
    0x00, 0x00, 0x00,       // Reserved
    'S', 'l', 'i', 'm', 'e', 'V', 'R', ' ',  // Vendor (8)
    'C', 'H', '5', '9', 'X', ' ', 'D', 'F',  // Product (16)
    'U', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
    '1', '.', '0', '0',     // Revision (4)
};

/*============================================================================
 * 状态
 *============================================================================*/

static struct {
    bool configured;
    bool unit_ready;
    
    // CBW/CSW
    uint8_t cbw[CBW_SIZE];
    uint8_t csw[CSW_SIZE];
    uint32_t tag;
    uint32_t data_residue;
    
    // 传输状态
    uint32_t transfer_lba;
    uint32_t transfer_blocks;
    bool transfer_read;
    
    // 缓冲区
    uint8_t buffer[512];
} msc_ctx;

/*============================================================================
 * SCSI 命令处理
 *============================================================================*/

static int process_scsi_cmd(const uint8_t *cbw)
{
    uint8_t cmd = cbw[15];  // SCSI command
    uint32_t lba, blocks;
    
    switch (cmd) {
        case SCSI_TEST_UNIT_READY:
            // 检查设备就绪
            return msc_is_ready() ? 0 : -1;
            
        case SCSI_REQUEST_SENSE:
            // 返回固定的 sense 数据
            memset(msc_ctx.buffer, 0, 18);
            msc_ctx.buffer[0] = 0x70;       // Response code
            msc_ctx.buffer[2] = 0x00;       // Sense key: No Sense
            msc_ctx.buffer[7] = 10;         // Additional sense length
            msc_ctx.buffer[12] = 0x00;      // ASC
            msc_ctx.buffer[13] = 0x00;      // ASCQ
            return 18;
            
        case SCSI_INQUIRY:
            memcpy(msc_ctx.buffer, scsi_inquiry_data, sizeof(scsi_inquiry_data));
            return sizeof(scsi_inquiry_data);
            
        case SCSI_MODE_SENSE_6:
            memset(msc_ctx.buffer, 0, 4);
            msc_ctx.buffer[0] = 3;          // Mode data length
            msc_ctx.buffer[2] = msc_is_writable() ? 0x00 : 0x80;  // Write protect
            return 4;
            
        case SCSI_MODE_SENSE_10:
            memset(msc_ctx.buffer, 0, 8);
            msc_ctx.buffer[1] = 6;          // Mode data length
            msc_ctx.buffer[3] = msc_is_writable() ? 0x00 : 0x80;
            return 8;
            
        case SCSI_START_STOP_UNIT:
        case SCSI_PREVENT_ALLOW:
            return 0;
            
        case SCSI_READ_CAPACITY_10:
            // 返回容量信息
            {
                uint32_t block_count, block_size;
                msc_get_capacity(&block_count, &block_size);
                
                uint32_t last_lba = block_count - 1;
                msc_ctx.buffer[0] = (last_lba >> 24) & 0xFF;
                msc_ctx.buffer[1] = (last_lba >> 16) & 0xFF;
                msc_ctx.buffer[2] = (last_lba >> 8) & 0xFF;
                msc_ctx.buffer[3] = last_lba & 0xFF;
                msc_ctx.buffer[4] = (block_size >> 24) & 0xFF;
                msc_ctx.buffer[5] = (block_size >> 16) & 0xFF;
                msc_ctx.buffer[6] = (block_size >> 8) & 0xFF;
                msc_ctx.buffer[7] = block_size & 0xFF;
            }
            return 8;
            
        case SCSI_READ_10:
            // 读取扇区
            lba = (cbw[17] << 24) | (cbw[18] << 16) | (cbw[19] << 8) | cbw[20];
            blocks = (cbw[22] << 8) | cbw[23];
            
            msc_ctx.transfer_lba = lba;
            msc_ctx.transfer_blocks = blocks;
            msc_ctx.transfer_read = true;
            return blocks * 512;
            
        case SCSI_WRITE_10:
            // 写入扇区
            lba = (cbw[17] << 24) | (cbw[18] << 16) | (cbw[19] << 8) | cbw[20];
            blocks = (cbw[22] << 8) | cbw[23];
            
            msc_ctx.transfer_lba = lba;
            msc_ctx.transfer_blocks = blocks;
            msc_ctx.transfer_read = false;
            return 0;
            
        default:
            return -1;
    }
}

/*============================================================================
 * USB 端点处理
 *============================================================================*/

#ifdef CH59X

static uint8_t __attribute__((aligned(4))) ep0_buf[64];
static uint8_t __attribute__((aligned(4))) ep2_out_buf[64];
static uint8_t __attribute__((aligned(4))) ep2_in_buf[64];

static volatile uint8_t setup_req_type;
static volatile uint8_t setup_req;
static volatile uint16_t setup_value;
static volatile uint16_t setup_index;
static volatile uint16_t setup_len;
static volatile uint8_t usb_addr = 0;

static const uint8_t *ep0_tx_ptr = NULL;
static uint16_t ep0_tx_len = 0;

static void ep0_send(const uint8_t *data, uint16_t len)
{
    if (len > 64) len = 64;
    memcpy(ep0_buf, data, len);
    R8_UEP0_T_LEN = len;
    R8_UEP0_CTRL = (R8_UEP0_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
}

static void ep0_zlp(void)
{
    R8_UEP0_T_LEN = 0;
    R8_UEP0_CTRL = (R8_UEP0_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
}

static void ep0_stall(void)
{
    R8_UEP0_CTRL = UEP_T_RES_STALL | UEP_R_RES_STALL;
}

static void handle_setup(void)
{
    uint8_t *p = ep0_buf;
    setup_req_type = p[0];
    setup_req = p[1];
    setup_value = p[2] | (p[3] << 8);
    setup_index = p[4] | (p[5] << 8);
    setup_len = p[6] | (p[7] << 8);
    
    const uint8_t *desc = NULL;
    uint16_t len = 0;
    
    // 标准设备请求
    if ((setup_req_type & 0x60) == 0x00) {
        switch (setup_req) {
            case 0x00: // GET_STATUS
                ep0_buf[0] = 0;
                ep0_buf[1] = 0;
                ep0_send(ep0_buf, 2);
                break;
                
            case 0x05: // SET_ADDRESS
                usb_addr = setup_value & 0x7F;
                ep0_zlp();
                break;
                
            case 0x06: // GET_DESCRIPTOR
                switch (setup_value >> 8) {
                    case 0x01: desc = msc_device_desc; len = sizeof(msc_device_desc); break;
                    case 0x02: desc = msc_config_desc; len = sizeof(msc_config_desc); break;
                    case 0x03:
                        switch (setup_value & 0xFF) {
                            case 0: desc = msc_string_lang; len = sizeof(msc_string_lang); break;
                            case 1: desc = msc_string_mfg; len = sizeof(msc_string_mfg); break;
                            case 2: desc = msc_string_prod; len = sizeof(msc_string_prod); break;
                            case 3: desc = msc_string_serial; len = sizeof(msc_string_serial); break;
                        }
                        break;
                }
                if (desc) {
                    if (len > setup_len) len = setup_len;
                    ep0_tx_ptr = desc;
                    ep0_tx_len = len;
                    uint16_t chunk = (len > 64) ? 64 : len;
                    ep0_send(desc, chunk);
                    ep0_tx_ptr += chunk;
                    ep0_tx_len -= chunk;
                } else {
                    ep0_stall();
                }
                break;
                
            case 0x08: // GET_CONFIGURATION
                ep0_buf[0] = msc_ctx.configured ? 1 : 0;
                ep0_send(ep0_buf, 1);
                break;
                
            case 0x09: // SET_CONFIGURATION
                msc_ctx.configured = (setup_value != 0);
                msc_ctx.unit_ready = true;
                ep0_zlp();
                break;
                
            default:
                ep0_stall();
                break;
        }
    }
    // MSC 类请求
    else if ((setup_req_type & 0x60) == 0x20) {
        switch (setup_req) {
            case MSC_REQ_RESET:
                ep0_zlp();
                break;
            case MSC_REQ_GET_MAX_LUN:
                ep0_buf[0] = 0;
                ep0_send(ep0_buf, 1);
                break;
            default:
                ep0_stall();
                break;
        }
    }
    else {
        ep0_stall();
    }
}

static void handle_bulk_out(void)
{
    uint8_t len = R8_USB_RX_LEN;
    
    // CBW?
    if (len == CBW_SIZE && 
        *(uint32_t*)ep2_out_buf == CBW_SIGNATURE) {
        
        memcpy(msc_ctx.cbw, ep2_out_buf, CBW_SIZE);
        msc_ctx.tag = *(uint32_t*)(msc_ctx.cbw + 4);
        msc_ctx.data_residue = *(uint32_t*)(msc_ctx.cbw + 8);
        
        int result = process_scsi_cmd(msc_ctx.cbw);
        
        if (result >= 0) {
            // 发送数据或 CSW
            if (msc_ctx.transfer_read && msc_ctx.transfer_blocks > 0) {
                // 读取扇区数据
                msc_read(msc_ctx.transfer_lba, 0, ep2_in_buf, 64);
                R8_UEP2_T_LEN = 64;
                R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
            } else if (result > 0) {
                // 发送响应数据
                memcpy(ep2_in_buf, msc_ctx.buffer, (result > 64) ? 64 : result);
                R8_UEP2_T_LEN = (result > 64) ? 64 : result;
                R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
            } else {
                // 发送 CSW
                *(uint32_t*)(msc_ctx.csw) = CSW_SIGNATURE;
                *(uint32_t*)(msc_ctx.csw + 4) = msc_ctx.tag;
                *(uint32_t*)(msc_ctx.csw + 8) = 0;
                msc_ctx.csw[12] = CSW_STATUS_PASSED;
                
                memcpy(ep2_in_buf, msc_ctx.csw, CSW_SIZE);
                R8_UEP2_T_LEN = CSW_SIZE;
                R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
            }
        } else {
            // 命令失败
            *(uint32_t*)(msc_ctx.csw) = CSW_SIGNATURE;
            *(uint32_t*)(msc_ctx.csw + 4) = msc_ctx.tag;
            *(uint32_t*)(msc_ctx.csw + 8) = msc_ctx.data_residue;
            msc_ctx.csw[12] = CSW_STATUS_FAILED;
            
            memcpy(ep2_in_buf, msc_ctx.csw, CSW_SIZE);
            R8_UEP2_T_LEN = CSW_SIZE;
            R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
        }
    }
    // 写入数据
    else if (!msc_ctx.transfer_read && msc_ctx.transfer_blocks > 0) {
        msc_write(msc_ctx.transfer_lba, 0, ep2_out_buf, len);
        msc_ctx.transfer_lba++;
        msc_ctx.transfer_blocks--;
        
        if (msc_ctx.transfer_blocks == 0) {
            // 写入完成，发送 CSW
            *(uint32_t*)(msc_ctx.csw) = CSW_SIGNATURE;
            *(uint32_t*)(msc_ctx.csw + 4) = msc_ctx.tag;
            *(uint32_t*)(msc_ctx.csw + 8) = 0;
            msc_ctx.csw[12] = CSW_STATUS_PASSED;
            
            memcpy(ep2_in_buf, msc_ctx.csw, CSW_SIZE);
            R8_UEP2_T_LEN = CSW_SIZE;
            R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
        }
    }
    
    R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_R_RES) | UEP_R_RES_ACK;
}

/*============================================================================
 * USB_IRQHandler - 条件编译避免与usb_hid_slime.c冲突
 * Receiver目标: USB_IRQHandler在usb_hid_slime.c中定义
 * Tracker目标: USB_IRQHandler在这里定义
 *============================================================================*/

#ifndef BUILD_RECEIVER
// Tracker目标：只使用MSC，在这里定义USB中断处理
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
                if (token == UIS_TOKEN_SETUP) {
                    memcpy(ep0_buf, (void*)R16_UEP0_DMA, 8);
                    R8_UEP0_CTRL = UEP_T_RES_NAK | UEP_R_RES_NAK;
                    handle_setup();
                } else if (token == UIS_TOKEN_IN) {
                    if (usb_addr != 0 && R8_USB_DEV_AD == 0) {
                        R8_USB_DEV_AD = usb_addr;
                    }
                    if (ep0_tx_len > 0) {
                        uint16_t chunk = (ep0_tx_len > 64) ? 64 : ep0_tx_len;
                        ep0_send(ep0_tx_ptr, chunk);
                        ep0_tx_ptr += chunk;
                        ep0_tx_len -= chunk;
                    } else {
                        R8_UEP0_CTRL = UEP_T_RES_NAK | UEP_R_RES_ACK;
                    }
                } else if (token == UIS_TOKEN_OUT) {
                    R8_UEP0_CTRL = UEP_T_RES_NAK | UEP_R_RES_NAK;
                }
                break;
                
            case 2:
                if (token == UIS_TOKEN_OUT) {
                    handle_bulk_out();
                } else if (token == UIS_TOKEN_IN) {
                    // 继续读取传输
                    if (msc_ctx.transfer_read && msc_ctx.transfer_blocks > 0) {
                        msc_ctx.transfer_lba++;
                        msc_ctx.transfer_blocks--;
                        
                        if (msc_ctx.transfer_blocks > 0) {
                            msc_read(msc_ctx.transfer_lba, 0, ep2_in_buf, 64);
                            R8_UEP2_T_LEN = 64;
                            R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
                        } else {
                            // 读取完成，发送 CSW
                            *(uint32_t*)(msc_ctx.csw) = CSW_SIGNATURE;
                            *(uint32_t*)(msc_ctx.csw + 4) = msc_ctx.tag;
                            *(uint32_t*)(msc_ctx.csw + 8) = 0;
                            msc_ctx.csw[12] = CSW_STATUS_PASSED;
                            
                            memcpy(ep2_in_buf, msc_ctx.csw, CSW_SIZE);
                            R8_UEP2_T_LEN = CSW_SIZE;
                            R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
                        }
                    } else {
                        R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
                    }
                }
                break;
        }
        
        R8_USB_INT_FG = RB_UIF_TRANSFER;
    }
    
    if (int_flag & RB_UIF_BUS_RST) {
        R8_USB_DEV_AD = 0;
        usb_addr = 0;
        msc_ctx.configured = false;
        R8_UEP0_CTRL = UEP_T_RES_NAK | UEP_R_RES_ACK;
        R8_UEP2_CTRL = UEP_T_RES_NAK | UEP_R_RES_ACK;
        R8_USB_INT_FG = RB_UIF_BUS_RST;
    }
    
    if (int_flag & RB_UIF_SUSPEND) {
        R8_USB_INT_FG = RB_UIF_SUSPEND;
    }
}
#endif // !BUILD_RECEIVER

/*============================================================================
 * 公共函数: 供Receiver目标中的usb_hid_slime.c调用
 *============================================================================*/

#ifdef BUILD_RECEIVER
void usb_msc_handle_ep2(uint8_t token)
{
    if (token == UIS_TOKEN_OUT) {
        handle_bulk_out();
    } else if (token == UIS_TOKEN_IN) {
        // 继续读取传输
        if (msc_ctx.transfer_read && msc_ctx.transfer_blocks > 0) {
            msc_ctx.transfer_lba++;
            msc_ctx.transfer_blocks--;
            
            if (msc_ctx.transfer_blocks > 0) {
                msc_read(msc_ctx.transfer_lba, 0, ep2_in_buf, 64);
                R8_UEP2_T_LEN = 64;
                R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
            } else {
                // 读取完成，发送 CSW
                *(uint32_t*)(msc_ctx.csw) = CSW_SIGNATURE;
                *(uint32_t*)(msc_ctx.csw + 4) = msc_ctx.tag;
                *(uint32_t*)(msc_ctx.csw + 8) = 0;
                msc_ctx.csw[12] = CSW_STATUS_PASSED;
                
                memcpy(ep2_in_buf, msc_ctx.csw, CSW_SIZE);
                R8_UEP2_T_LEN = CSW_SIZE;
                R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_ACK;
            }
        } else {
            R8_UEP2_CTRL = (R8_UEP2_CTRL & ~MASK_UEP_T_RES) | UEP_T_RES_NAK;
        }
    }
}
#endif // BUILD_RECEIVER

#endif // CH59X

/*============================================================================
 * 公共 API
 *============================================================================*/

int usb_msc_init(void)
{
    memset(&msc_ctx, 0, sizeof(msc_ctx));
    
#ifdef CH59X
    // 开启 USB 时钟
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG1;
    R8_SAFE_ACCESS_SIG = SAFE_ACCESS_SIG2;
    R8_SLP_CLK_OFF0 &= ~RB_SLP_CLK_USB;
    R8_SAFE_ACCESS_SIG = 0;
    
    // 配置 DMA
    R16_UEP0_DMA = (uint16_t)(uint32_t)ep0_buf;
    R16_UEP2_DMA = (uint16_t)(uint32_t)ep2_out_buf;
    R16_UEP3_DMA = (uint16_t)(uint32_t)ep2_in_buf;  // EP2 IN 使用 EP3 DMA
    
    // 配置端点
    R8_UEP4_1_MOD = 0;
    R8_UEP2_3_MOD = RB_UEP2_RX_EN | RB_UEP2_TX_EN;
    
    R8_UEP0_CTRL = UEP_T_RES_NAK | UEP_R_RES_ACK;
    R8_UEP2_CTRL = UEP_T_RES_NAK | UEP_R_RES_ACK;
    
    // 启用 USB
    R8_USB_CTRL = 0;
    R8_USB_DEV_AD = 0;
    R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_DMA_EN;
    
    // 启用中断
    R8_USB_INT_EN = RB_UIE_TRANSFER | RB_UIE_BUS_RST | RB_UIE_SUSPEND;
    PFIC_EnableIRQ(USB_IRQn);
    
    R8_USB_CTRL |= RB_UC_DEV_PU_EN;
#endif
    
    return 0;
}

bool usb_msc_connected(void)
{
    return msc_ctx.configured;
}

void usb_msc_task(void)
{
    // 由中断处理
}
