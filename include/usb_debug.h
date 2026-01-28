/**
 * @file usb_debug.h
 * @brief USB 调试接口
 * 
 * 功能:
 * 1. 实时数据流 (四元数、传感器)
 * 2. 状态查询
 * 3. 配置修改
 * 4. 诊断命令
 * 
 * 同时支持 Tracker 和 Receiver
 */

#ifndef __USB_DEBUG_H__
#define __USB_DEBUG_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
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
} usb_debug_cmd_t;

/*============================================================================
 * API 函数
 *============================================================================*/

/**
 * @brief 初始化USB调试模块
 */
void usb_debug_init(void);

/**
 * @brief 处理USB调试命令 (在主循环中调用)
 */
void usb_debug_process(void);

/**
 * @brief 启用/禁用调试
 * @param enable true 启用
 */
void usb_debug_enable(bool enable);

/**
 * @brief 检查是否正在流式传输数据
 * @return true 如果正在流式传输
 */
bool usb_debug_is_streaming(void);

/**
 * @brief 发送调试日志
 * @param fmt 格式字符串
 */
void usb_debug_printf(const char *fmt, ...);

/**
 * @brief 发送传感器数据
 * @param gyro 陀螺仪数据
 * @param accel 加速度计数据
 * @param quat 四元数
 */
void usb_debug_send_sensors(const float gyro[3], const float accel[3], const float quat[4]);

#ifdef __cplusplus
}
#endif

#endif /* __USB_DEBUG_H__ */
