/**
 * @file usb_debug.c
 * @brief USB 调试接口
 * 
 * 功能:
 * 1. 实时数据流 (四元数、传感器)
 * 2. 状态查询
 * 3. 配置修改
 * 4. 诊断命令
 * 
 * 同时支持 Tracker 和 Receiver
 * Receiver 可同时接收数据和调试
 */

#include "hal.h"
#include "usb_hid_slime.h"
#include "version.h"      // FIRMWARE_VERSION_xxx
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#ifdef CH59X
#include "CH59x_common.h"  // SYS_ResetExecute
#endif

/*============================================================================
 * 调试命令定义
 *============================================================================*/

typedef enum {
    DBG_CMD_PING            = 0x01,
    DBG_CMD_GET_VERSION     = 0x02,
    DBG_CMD_GET_STATUS      = 0x03,
    DBG_CMD_GET_CONFIG      = 0x04,
    DBG_CMD_SET_CONFIG      = 0x05,
    
    DBG_CMD_GET_SENSORS     = 0x10,
    DBG_CMD_GET_QUAT        = 0x11,
    DBG_CMD_GET_RF_STATUS   = 0x12,
    DBG_CMD_GET_BATTERY     = 0x13,
    DBG_CMD_GET_TEMP        = 0x14,
    DBG_CMD_GET_STATS       = 0x15,
    
    DBG_CMD_CALIBRATE       = 0x20,
    DBG_CMD_RESET           = 0x21,
    DBG_CMD_ENTER_BOOT      = 0x22,
    DBG_CMD_SAVE_CONFIG     = 0x23,
    
    DBG_CMD_STREAM_START    = 0x30,
    DBG_CMD_STREAM_STOP     = 0x31,
    
    DBG_CMD_MAG_ENABLE      = 0x40,
    DBG_CMD_MAG_DISABLE     = 0x41,
    DBG_CMD_MAG_CALIBRATE   = 0x42,
    
    // Receiver 专用
    DBG_CMD_GET_TRACKERS    = 0x50,
    DBG_CMD_PAIR_START      = 0x51,
    DBG_CMD_PAIR_STOP       = 0x52,
    
    DBG_CMD_LOG             = 0xF0,
} debug_cmd_t;

/*============================================================================
 * 状态
 *============================================================================*/

typedef struct {
    bool enabled;
    bool streaming;
    uint8_t stream_mask;    // bit0=quat, bit1=gyro, bit2=accel, bit3=temp
    uint32_t stream_interval_ms;
    uint32_t last_stream_ms;
    
    // 统计
    uint32_t rx_count;
    uint32_t tx_count;
} debug_state_t;

static debug_state_t dbg = {0};
static uint8_t tx_buf[64];

/*============================================================================
 * 外部数据引用
 *============================================================================*/

// Tracker
extern float quaternion[4];
extern float gyro[3], accel[3];
extern uint8_t battery_percent;
extern bool is_charging;
extern uint8_t tracker_id;

// 温度
extern float temp_comp_get_temp(void);

// 校准
extern void auto_calib_force(void);
extern bool auto_calib_is_valid(void);

// 磁力计
extern int mag_enable(void);
extern int mag_disable(void);
extern int mag_calibrate_start(void);

// Bootloader
extern int bootloader_enter_update_mode(void);

/*============================================================================
 * 命令处理
 *============================================================================*/

static void handle_command(const uint8_t *data, uint8_t len)
{
    if (len < 1) return;
    
    uint8_t cmd = data[0];
    memset(tx_buf, 0, sizeof(tx_buf));
    tx_buf[0] = cmd | 0x80;  // 响应标志
    
    switch (cmd) {
        case DBG_CMD_PING:
            tx_buf[1] = 'O';
            tx_buf[2] = 'K';
            usb_hid_write(tx_buf, 3);
            break;
            
        case DBG_CMD_GET_VERSION:
            tx_buf[1] = FIRMWARE_VERSION_MAJOR;
            tx_buf[2] = FIRMWARE_VERSION_MINOR;
            tx_buf[3] = FIRMWARE_VERSION_PATCH;
            usb_hid_write(tx_buf, 4);
            break;
            
        case DBG_CMD_GET_STATUS:
            tx_buf[1] = tracker_id;
            tx_buf[2] = (uint8_t)battery_percent;
            tx_buf[3] = is_charging ? 1 : 0;
            tx_buf[4] = auto_calib_is_valid() ? 1 : 0;
            tx_buf[5] = dbg.streaming ? 1 : 0;
            usb_hid_write(tx_buf, 6);
            break;
            
        case DBG_CMD_GET_SENSORS:
            {
                int16_t gx = (int16_t)(gyro[0] * 1000);
                int16_t gy = (int16_t)(gyro[1] * 1000);
                int16_t gz = (int16_t)(gyro[2] * 1000);
                int16_t ax = (int16_t)(accel[0] * 100);
                int16_t ay = (int16_t)(accel[1] * 100);
                int16_t az = (int16_t)(accel[2] * 100);
                
                tx_buf[1] = gx & 0xFF; tx_buf[2] = gx >> 8;
                tx_buf[3] = gy & 0xFF; tx_buf[4] = gy >> 8;
                tx_buf[5] = gz & 0xFF; tx_buf[6] = gz >> 8;
                tx_buf[7] = ax & 0xFF; tx_buf[8] = ax >> 8;
                tx_buf[9] = ay & 0xFF; tx_buf[10] = ay >> 8;
                tx_buf[11] = az & 0xFF; tx_buf[12] = az >> 8;
                usb_hid_write(tx_buf, 13);
            }
            break;
            
        case DBG_CMD_GET_QUAT:
            {
                int16_t qw = (int16_t)(quaternion[0] * 32767);
                int16_t qx = (int16_t)(quaternion[1] * 32767);
                int16_t qy = (int16_t)(quaternion[2] * 32767);
                int16_t qz = (int16_t)(quaternion[3] * 32767);
                
                tx_buf[1] = qw & 0xFF; tx_buf[2] = qw >> 8;
                tx_buf[3] = qx & 0xFF; tx_buf[4] = qx >> 8;
                tx_buf[5] = qy & 0xFF; tx_buf[6] = qy >> 8;
                tx_buf[7] = qz & 0xFF; tx_buf[8] = qz >> 8;
                usb_hid_write(tx_buf, 9);
            }
            break;
            
        case DBG_CMD_GET_TEMP:
            {
                float temp = temp_comp_get_temp();
                int16_t t = (int16_t)(temp * 100);
                tx_buf[1] = t & 0xFF;
                tx_buf[2] = t >> 8;
                usb_hid_write(tx_buf, 3);
            }
            break;
            
        case DBG_CMD_CALIBRATE:
            auto_calib_force();
            tx_buf[1] = 1;
            usb_hid_write(tx_buf, 2);
            break;
            
        case DBG_CMD_RESET:
            hal_delay_ms(100);
            #ifdef CH59X
            SYS_ResetExecute();
            #else
            // 非CH59X平台的复位实现
            while(1);
            #endif
            break;
            
        case DBG_CMD_ENTER_BOOT:
            bootloader_enter_update_mode();
            tx_buf[1] = 1;
            usb_hid_write(tx_buf, 2);
            break;
            
        case DBG_CMD_STREAM_START:
            dbg.streaming = true;
            dbg.stream_mask = (len > 1) ? data[1] : 0x0F;
            dbg.stream_interval_ms = (len > 2) ? (data[2] * 10) : 50;
            tx_buf[1] = 1;
            usb_hid_write(tx_buf, 2);
            break;
            
        case DBG_CMD_STREAM_STOP:
            dbg.streaming = false;
            tx_buf[1] = 1;
            usb_hid_write(tx_buf, 2);
            break;
            
        case DBG_CMD_MAG_ENABLE:
            tx_buf[1] = (mag_enable() == 0) ? 1 : 0;
            usb_hid_write(tx_buf, 2);
            break;
            
        case DBG_CMD_MAG_DISABLE:
            tx_buf[1] = (mag_disable() == 0) ? 1 : 0;
            usb_hid_write(tx_buf, 2);
            break;
            
        default:
            tx_buf[1] = 0xFF;  // 未知命令
            usb_hid_write(tx_buf, 2);
            break;
    }
    
    dbg.rx_count++;
}

/*============================================================================
 * 数据流输出
 *============================================================================*/

static void stream_output(void)
{
    if (!dbg.streaming) return;
    
    uint32_t now = hal_get_tick_ms();
    if (now - dbg.last_stream_ms < dbg.stream_interval_ms) return;
    dbg.last_stream_ms = now;
    
    memset(tx_buf, 0, sizeof(tx_buf));
    tx_buf[0] = 0xFE;  // 流数据标识
    tx_buf[1] = dbg.stream_mask;
    
    uint8_t idx = 2;
    
    // 四元数
    if (dbg.stream_mask & 0x01) {
        int16_t q[4];
        q[0] = (int16_t)(quaternion[0] * 32767);
        q[1] = (int16_t)(quaternion[1] * 32767);
        q[2] = (int16_t)(quaternion[2] * 32767);
        q[3] = (int16_t)(quaternion[3] * 32767);
        for (int i = 0; i < 4; i++) {
            tx_buf[idx++] = q[i] & 0xFF;
            tx_buf[idx++] = q[i] >> 8;
        }
    }
    
    // 陀螺仪
    if (dbg.stream_mask & 0x02) {
        int16_t g[3];
        g[0] = (int16_t)(gyro[0] * 1000);
        g[1] = (int16_t)(gyro[1] * 1000);
        g[2] = (int16_t)(gyro[2] * 1000);
        for (int i = 0; i < 3; i++) {
            tx_buf[idx++] = g[i] & 0xFF;
            tx_buf[idx++] = g[i] >> 8;
        }
    }
    
    // 加速度
    if (dbg.stream_mask & 0x04) {
        int16_t a[3];
        a[0] = (int16_t)(accel[0] * 100);
        a[1] = (int16_t)(accel[1] * 100);
        a[2] = (int16_t)(accel[2] * 100);
        for (int i = 0; i < 3; i++) {
            tx_buf[idx++] = a[i] & 0xFF;
            tx_buf[idx++] = a[i] >> 8;
        }
    }
    
    // 温度
    if (dbg.stream_mask & 0x08) {
        int16_t t = (int16_t)(temp_comp_get_temp() * 100);
        tx_buf[idx++] = t & 0xFF;
        tx_buf[idx++] = t >> 8;
    }
    
    usb_hid_write(tx_buf, idx);
    dbg.tx_count++;
}

/*============================================================================
 * 日志输出
 *============================================================================*/

void debug_log(const char *fmt, ...)
{
    if (!dbg.enabled) return;
    
    char buf[56];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    if (len > 0 && len < 56) {
        tx_buf[0] = DBG_CMD_LOG | 0x80;
        tx_buf[1] = len;
        memcpy(&tx_buf[2], buf, len);
        usb_hid_write(tx_buf, len + 2);
    }
}

/*============================================================================
 * USB 回调
 *============================================================================*/

static void rx_callback(const uint8_t *data, uint8_t len)
{
    // 调试命令范围: 0x01-0x7F
    if (len > 0 && data[0] >= 0x01 && data[0] < 0x80) {
        handle_command(data, len);
    }
}

/*============================================================================
 * 主处理
 *============================================================================*/

void usb_debug_process(void)
{
    stream_output();
}

void usb_debug_init(void)
{
    memset(&dbg, 0, sizeof(dbg));
    dbg.enabled = true;
    dbg.stream_interval_ms = 50;
    
    // 注册接收回调
    usb_hid_set_rx_callback(rx_callback);
}

void usb_debug_enable(bool enable) { dbg.enabled = enable; }
bool usb_debug_is_streaming(void) { return dbg.streaming; }
