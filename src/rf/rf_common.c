/**
 * @file rf_common.c
 * @brief RF通用函数 - Tracker和Receiver共享
 * 
 * 包含CRC计算和信道跳频等公共函数
 */

#include "rf_protocol.h"
#include <stdint.h>

/*============================================================================
 * CRC-16 计算 (ModBus)
 *============================================================================*/

uint16_t rf_calc_crc16(const void *data, uint16_t len)
{
    const uint8_t *ptr = (const uint8_t *)data;
    uint16_t crc = 0xFFFF;
    
    for (uint16_t i = 0; i < len; i++) {
        crc ^= ptr[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

/*============================================================================
 * 跳频信道计算
 *============================================================================*/

uint8_t rf_get_hop_channel(uint16_t frame_number, uint32_t network_key)
{
    // 使用简单哈希算法基于帧号和网络密钥计算信道
    uint32_t hash = frame_number;
    hash ^= network_key;
    hash ^= (hash >> 16);
    hash *= 0x85EBCA6B;
    hash ^= (hash >> 13);
    hash *= 0xC2B2AE35;
    hash ^= (hash >> 16);
    
    // 映射到可用信道范围 (避开WiFi信道)
    // 使用2400-2480MHz中的低干扰信道
    static const uint8_t hop_channels[] = {
        5, 15, 25, 35, 45, 55, 65, 75,  // 主跳频序列
        10, 20, 30, 40, 50, 60, 70, 80  // 备用序列
    };
    
    return hop_channels[hash % (sizeof(hop_channels) / sizeof(hop_channels[0]))];
}
