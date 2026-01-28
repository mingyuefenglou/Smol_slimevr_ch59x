/**
 * @file usb_hid_slime.h
 * @brief USB HID for SlimeVR Receiver on CH592
 * 
 * SlimeVR 官方软件兼容的 USB HID 实现
 * 完整 USB 枚举和 HID 协议支持
 */

#ifndef __USB_HID_SLIME_H__
#define __USB_HID_SLIME_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Configuration
 *============================================================================*/

// SlimeVR 兼容 VID/PID
#ifndef USB_VID
#define USB_VID             0x1209      // pid.codes (开源硬件)
#endif
#ifndef USB_PID
#define USB_PID             0x7690      // SlimeVR CH592
#endif

#define USB_HID_EP_SIZE     64          // Endpoint size
#define USB_HID_INTERVAL    1           // 1ms polling interval

/*============================================================================
 * Callback Types
 *============================================================================*/

// 接收数据回调
typedef void (*usb_hid_rx_callback_t)(const uint8_t *data, uint8_t len);

/*============================================================================
 * USB HID API
 *============================================================================*/

/**
 * @brief 初始化 USB HID
 * @return 0: 成功
 */
int usb_hid_init(void);

/**
 * @brief 反初始化 USB HID
 */
void usb_hid_deinit(void);

/**
 * @brief 检查 USB 是否就绪 (已配置且未挂起)
 * @return true: 就绪
 */
bool usb_hid_ready(void);

/**
 * @brief 检查 USB 是否已连接 (已配置)
 * @return true: 已连接
 */
bool usb_hid_connected(void);

/**
 * @brief 检查 USB 是否挂起
 * @return true: 已挂起
 */
bool usb_hid_suspended(void);

/**
 * @brief 检查发送是否忙
 * @return true: 忙
 */
bool usb_hid_busy(void);

/**
 * @brief 发送 HID 报告
 * @param data 数据
 * @param len 长度 (max 64)
 * @return 发送的字节数, <0: 错误
 */
int usb_hid_write(const uint8_t *data, uint8_t len);

/**
 * @brief 阻塞发送 HID 报告
 * @param data 数据
 * @param len 长度
 * @param timeout_ms 超时 (ms)
 * @return 发送的字节数, <0: 错误
 */
int usb_hid_write_blocking(const uint8_t *data, uint8_t len, uint32_t timeout_ms);

/**
 * @brief 设置接收回调
 * @param callback 回调函数
 */
void usb_hid_set_rx_callback(usb_hid_rx_callback_t callback);

/**
 * @brief USB 任务 (主循环调用)
 */
void usb_hid_task(void);

/*============================================================================
 * Tracker Data API (SlimeVR 兼容)
 *============================================================================*/

/**
 * @brief 更新追踪器数据 (缓存)
 */
void usb_hid_update_tracker(uint8_t id, const int16_t quat[4], 
                            const int16_t accel[3], uint8_t battery, int8_t rssi);

/**
 * @brief 发送单个追踪器数据
 */
int usb_hid_send_tracker_data(uint8_t id, const int16_t quat[4], 
                               uint8_t battery, uint8_t accuracy);

/**
 * @brief 发送所有追踪器数据包
 */
int usb_hid_send_bundle(void);

#ifdef __cplusplus
}
#endif

#endif /* __USB_HID_SLIME_H__ */
