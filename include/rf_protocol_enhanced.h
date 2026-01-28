/**
 * @file rf_protocol_enhanced.h
 * @brief 增强的 RF 协议头文件
 * 
 * 功能:
 * - 自动重传 (ARQ)
 * - RSSI 测量
 * - 信道质量评估
 * - 自适应跳频
 * - 时间同步
 */

#ifndef __RF_PROTOCOL_ENHANCED_H__
#define __RF_PROTOCOL_ENHANCED_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 配置常量
 *============================================================================*/

#define RF_MAX_RETRIES          3       // 最大重传次数
#define RF_ACK_TIMEOUT_US       500     // ACK 超时 (us)
#define RF_CHANNEL_COUNT        40      // 信道数量
#define RF_HOP_SEQUENCE_LEN     8       // 跳频序列长度

// RSSI 阈值
#define RF_RSSI_GOOD            -60     // 良好
#define RF_RSSI_FAIR            -75     // 一般
#define RF_RSSI_POOR            -85     // 较差

/*============================================================================
 * 信道质量接口
 *============================================================================*/

/**
 * @brief 初始化信道质量跟踪
 */
void rf_channel_init(void);

/**
 * @brief 更新信道质量
 * @param channel 信道号
 * @param success 本次传输是否成功
 * @param rssi 接收信号强度
 */
void rf_channel_update(uint8_t channel, bool success, int8_t rssi);

/**
 * @brief 获取信道质量评分
 * @param channel 信道号
 * @return 质量评分 (0-100)
 */
uint8_t rf_channel_get_quality(uint8_t channel);

/**
 * @brief 检查信道是否良好
 * @param channel 信道号
 * @return true = 良好
 */
bool rf_channel_is_good(uint8_t channel);

/**
 * @brief 生成跳频序列
 * @param seed 随机种子 (通常是 network_key)
 */
void rf_channel_generate_hop_sequence(uint32_t seed);

/**
 * @brief 获取下一个跳频信道
 * @return 信道号
 */
uint8_t rf_channel_get_next(void);

/*============================================================================
 * RSSI 接口
 *============================================================================*/

/**
 * @brief 更新 RSSI 记录
 * @param rssi 新的 RSSI 值
 */
void rf_rssi_update(int8_t rssi);

/**
 * @brief 获取当前 RSSI
 * @return RSSI (dBm)
 */
int8_t rf_rssi_get_current(void);

/**
 * @brief 获取平均 RSSI
 * @return 平均 RSSI (dBm)
 */
int8_t rf_rssi_get_average(void);

/**
 * @brief 获取链路质量
 * @return 质量评分 (0-100)
 */
uint8_t rf_get_link_quality(void);

/*============================================================================
 * 带重传的发送接口
 *============================================================================*/

/**
 * @brief 发送数据并等待 ACK
 * @param data 发送数据
 * @param len 数据长度
 * @param ack_buf ACK 接收缓冲区
 * @param ack_len ACK 缓冲区大小
 * @param max_retries 最大重试次数
 * @return ACK 长度 (>0), 或 -1 = 失败
 */
int rf_transmit_with_ack(const uint8_t *data, uint8_t len,
                         uint8_t *ack_buf, uint8_t ack_len,
                         uint8_t max_retries);

/*============================================================================
 * 统计接口
 *============================================================================*/

/**
 * @brief 获取传输统计
 */
void rf_get_stats(uint32_t *tx, uint32_t *success, uint32_t *retry, uint32_t *timeout);

/**
 * @brief 重置统计
 */
void rf_reset_stats(void);

/**
 * @brief 获取丢包率
 * @return 丢包率 (%)
 */
uint8_t rf_get_packet_loss(void);

/*============================================================================
 * 时间同步接口
 *============================================================================*/

/**
 * @brief 发送同步信标
 * @param network_key 网络密钥
 * @param tracker_mask 已连接追踪器位图
 */
void rf_sync_send_beacon(const uint8_t *network_key, uint8_t tracker_mask);

/**
 * @brief 接收同步信标
 * @param beacon_out 输出信标数据
 * @param rssi 输出 RSSI
 * @param timeout_ms 超时时间
 * @return true = 成功接收
 */
bool rf_sync_receive_beacon(void *beacon_out, int8_t *rssi, uint32_t timeout_ms);

/**
 * @brief 获取 slot 开始时间
 * @param slot_id Slot ID
 * @return 开始时间 (us)
 */
uint32_t rf_sync_get_slot_time(uint8_t slot_id);

/**
 * @brief 等待指定 slot
 * @param slot_id Slot ID
 */
void rf_sync_wait_for_slot(uint8_t slot_id);

/*============================================================================
 * ACK 包接口
 *============================================================================*/

/**
 * @brief 构建 ACK 包
 * @param packet 输出缓冲区
 * @param tracker_id 追踪器 ID
 * @param status 状态 (0=OK, 1=Resend, 2=Error)
 */
void rf_build_ack_packet(uint8_t *packet, uint8_t tracker_id, uint8_t status);

/**
 * @brief 解析 ACK 包
 * @param packet 输入数据
 * @param len 数据长度
 * @param expected_tracker_id 期望的追踪器 ID
 * @param status 输出状态
 * @return true = 有效 ACK
 */
bool rf_parse_ack_packet(const uint8_t *packet, uint8_t len,
                         uint8_t expected_tracker_id, uint8_t *status);

#ifdef __cplusplus
}
#endif

#endif /* __RF_PROTOCOL_ENHANCED_H__ */
