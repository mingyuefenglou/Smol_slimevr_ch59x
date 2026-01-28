/**
 * @file rf_hw.h
 * @brief CH59X RF Hardware Driver for Private Mode
 * 
 * This driver provides low-level access to the CH59X 2.4GHz radio
 * in proprietary (non-BLE) mode for custom protocol implementation.
 */

#ifndef __RF_HW_H__
#define __RF_HW_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Configuration
 *============================================================================*/

// RF data rate mode
#define RF_MODE_1MBPS           0
#define RF_MODE_2MBPS           1
#define RF_MODE_250KBPS         2

// RF operating mode (for rf_hw_set_mode)
#define RF_MODE_TX              0x10    // Transmit mode
#define RF_MODE_RX              0x11    // Receive mode  
#define RF_MODE_STANDBY         0x12    // Standby mode
#define RF_MODE_SLEEP           0x13    // Sleep mode (low power)

// TX power levels (dBm)
#define RF_TX_POWER_N20DBM      0   // -20 dBm
#define RF_TX_POWER_N10DBM      1   // -10 dBm
#define RF_TX_POWER_N5DBM       2   // -5 dBm
#define RF_TX_POWER_0DBM        3   // 0 dBm
#define RF_TX_POWER_1DBM        4   // 1 dBm
#define RF_TX_POWER_2DBM        5   // 2 dBm
#define RF_TX_POWER_3DBM        6   // 3 dBm
#define RF_TX_POWER_4DBM        7   // 4 dBm (max)

// Packet configuration
#define RF_ADDR_WIDTH_3         3
#define RF_ADDR_WIDTH_4         4
#define RF_ADDR_WIDTH_5         5

// CRC configuration
#define RF_CRC_NONE             0
#define RF_CRC_8BIT             1
#define RF_CRC_16BIT            2

/*============================================================================
 * RF Configuration Structure
 *============================================================================*/

typedef struct {
    uint8_t mode;               // RF_MODE_xxx
    uint8_t tx_power;           // RF_TX_POWER_xxx
    uint8_t addr_width;         // Address width (3-5 bytes)
    uint8_t crc_mode;           // RF_CRC_xxx
    uint32_t sync_word;         // Sync word (4 bytes)
    uint8_t channel;            // RF channel (0-39)
    bool auto_ack;              // Enable auto ACK
    uint8_t ack_timeout;        // ACK timeout in units of 250us
    uint8_t retry_count;        // Auto retry count
} rf_hw_config_t;

/*============================================================================
 * Callback Types
 *============================================================================*/

typedef void (*rf_hw_rx_callback_t)(const uint8_t *data, uint8_t len, int8_t rssi);
typedef void (*rf_hw_tx_callback_t)(bool success);

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Initialize RF hardware
 * @param config Pointer to configuration structure
 * @return 0 on success, negative on error
 */
int rf_hw_init(const rf_hw_config_t *config);

/**
 * @brief Initialize RF hardware with default SlimeVR config
 * @return 0 on success, negative on error
 */
int rf_hw_init_default(void);

/**
 * @brief Deinitialize RF hardware
 */
void rf_hw_deinit(void);

/**
 * @brief Set RF channel
 * @param channel Channel number (0-39)
 * @return 0 on success
 */
int rf_hw_set_channel(uint8_t channel);

/**
 * @brief Get current RF channel
 * @return Current channel number
 */
uint8_t rf_hw_get_channel(void);

/**
 * @brief Set TX power
 * @param power Power level (RF_TX_POWER_xxx)
 */
void rf_hw_set_tx_power(uint8_t power);

/**
 * @brief Set sync word (address)
 * @param sync_word 4-byte sync word
 */
void rf_hw_set_sync_word(uint32_t sync_word);

/**
 * @brief Enter RX mode
 */
void rf_hw_rx_mode(void);

/**
 * @brief Enter TX mode
 */
void rf_hw_tx_mode(void);

/**
 * @brief Enter standby mode (low power)
 */
void rf_hw_standby(void);

/**
 * @brief Enter sleep mode (lowest power)
 */
void rf_hw_sleep(void);

/**
 * @brief Transmit packet
 * @param data Packet data
 * @param len Data length (max 32 bytes)
 * @return 0 on success, negative on error
 */
int rf_hw_transmit(const uint8_t *data, uint8_t len);

/**
 * @brief 非阻塞发送数据包 (适用于中断/回调上下文)
 * @param data 数据缓冲区
 * @param len 数据长度 (最大32字节)
 * @return 0=发送已启动, -1=参数错误, -3=FIFO忙
 * @note 发送完成通过TX_DONE中断通知
 */
int rf_hw_transmit_async(const uint8_t *data, uint8_t len);

/**
 * @brief Transmit packet with ACK request
 * @param data Packet data
 * @param len Data length
 * @param ack_buf Buffer for ACK payload (can be NULL)
 * @param ack_len Pointer to ACK length (output)
 * @return 0 on success with ACK, 1 on success no ACK, negative on error
 */
int rf_hw_transmit_ack(const uint8_t *data, uint8_t len, 
                        uint8_t *ack_buf, uint8_t *ack_len);

/**
 * @brief Check if packet received
 * @return true if packet available
 */
bool rf_hw_rx_available(void);

/**
 * @brief Check if RX data is ready (alias)
 * @return true if data available
 */
#define rf_hw_data_ready() rf_hw_rx_available()

/**
 * @brief Set RF mode using mode constant
 * @param mode RF_MODE_xxx constant
 */
void rf_hw_set_mode(uint8_t mode);

/**
 * @brief Read received packet
 * @param data Buffer for packet data
 * @param max_len Maximum buffer size
 * @param rssi Pointer to store RSSI (can be NULL)
 * @return Number of bytes received, negative on error
 */
int rf_hw_receive(uint8_t *data, uint8_t max_len, int8_t *rssi);

/**
 * @brief Set ACK payload for next received packet
 * @param data ACK payload data
 * @param len Payload length (max 32 bytes)
 */
void rf_hw_set_ack_payload(const uint8_t *data, uint8_t len);

/**
 * @brief Flush TX FIFO
 */
void rf_hw_flush_tx(void);

/**
 * @brief Flush RX FIFO
 */
void rf_hw_flush_rx(void);

/**
 * @brief Get RSSI of last received packet
 * @return RSSI in dBm
 */
int8_t rf_hw_get_rssi(void);

/**
 * @brief Check if carrier detected (channel busy)
 * @return true if carrier detected
 */
bool rf_hw_carrier_detect(void);

/**
 * @brief Set RX callback (called from IRQ)
 */
void rf_hw_set_rx_callback(rf_hw_rx_callback_t callback);

/**
 * @brief Set TX complete callback (called from IRQ)
 */
void rf_hw_set_tx_callback(rf_hw_tx_callback_t callback);

/**
 * @brief Get unique device MAC address
 * @param mac Buffer for 6-byte MAC address
 */
void rf_hw_get_mac_address(uint8_t *mac);

/**
 * @brief Enable/disable RF interrupt
 */
void rf_hw_enable_irq(bool enable);

/**
 * @brief Process RF interrupt (call from ISR)
 */
void rf_hw_irq_handler(void);

/*============================================================================
 * Timing Functions
 *============================================================================*/

/**
 * @brief Get high-resolution timer value (microseconds)
 */
uint32_t rf_hw_get_time_us(void);

/**
 * @brief Delay for specified microseconds
 */
void rf_hw_delay_us(uint32_t us);

/**
 * @brief Start timer for slot timing
 * @param period_us Timer period in microseconds
 * @param callback Function to call on timer expiry
 */
void rf_hw_start_timer(uint32_t period_us, void (*callback)(void));

/**
 * @brief Stop slot timer
 */
void rf_hw_stop_timer(void);

#ifdef __cplusplus
}
#endif

#endif /* __RF_HW_H__ */
