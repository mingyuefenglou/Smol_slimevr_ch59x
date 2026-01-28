/**
 * @file usb_msc.h
 * @brief USB Mass Storage Class 驱动
 * 
 * 实现虚拟 FAT12 文件系统，支持拖放 UF2 固件升级
 * 
 * 功能:
 * 1. USB大容量存储设备
 * 2. 虚拟FAT12文件系统
 * 3. UF2固件拖放升级
 * 4. 配置文件访问
 */

#ifndef __USB_MSC_H__
#define __USB_MSC_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * API 函数
 *============================================================================*/

/**
 * @brief 初始化USB MSC模块
 * @return 0 成功
 */
int usb_msc_init(void);

/**
 * @brief USB MSC主任务 (在主循环中调用)
 */
void usb_msc_task(void);

/**
 * @brief 检查是否已连接
 * @return true 如果已连接
 */
bool usb_msc_connected(void);

/**
 * @brief 处理EP2数据 (USB中断回调)
 * @param token 令牌类型
 */
void usb_msc_handle_ep2(uint8_t token);

/**
 * @brief 设置配置文件数据 (用于虚拟文件系统)
 * @param data 配置数据
 * @param len 数据长度
 */
void usb_msc_set_config_data(const uint8_t *data, uint16_t len);

/**
 * @brief 检查是否有新固件写入
 * @return true 如果有新固件
 */
bool usb_msc_firmware_pending(void);

/**
 * @brief 获取固件数据
 * @param data 输出缓冲区
 * @param max_len 最大长度
 * @return 实际长度
 */
uint32_t usb_msc_get_firmware(uint8_t *data, uint32_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* __USB_MSC_H__ */
