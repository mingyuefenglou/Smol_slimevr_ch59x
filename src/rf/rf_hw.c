/**
 * @file rf_hw.c
 * @brief CH59X RF Hardware Driver Implementation
 * 
 * Implements private RF mode using CH59X 2.4GHz radio hardware.
 * This bypasses the BLE stack for direct radio control.
 */

#include "rf_hw.h"
#include "rf_protocol.h"  // 包含RF_CHANNEL_COUNT定义
#include "hal.h"
#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"

// 中断控制宏 (避免与core_riscv.h冲突)
#ifndef __disable_irq
#define __disable_irq()  __asm__ volatile ("csrci mstatus, 0x08")
#endif
#ifndef __enable_irq
#define __enable_irq()   __asm__ volatile ("csrsi mstatus, 0x08")
#endif

#endif

/*============================================================================
 * CH59X RF Register Definitions (Private Mode)
 *============================================================================*/

#ifdef CH59X

// RF control registers base
#define RF_BASE                 0x4000C000

// Register offsets
#define RF_CTRL                 (*(volatile uint32_t *)(RF_BASE + 0x00))
#define RF_STATUS               (*(volatile uint32_t *)(RF_BASE + 0x04))
#define RF_CHANNEL              (*(volatile uint32_t *)(RF_BASE + 0x08))
#define RF_POWER                (*(volatile uint32_t *)(RF_BASE + 0x0C))
#define RF_ADDR0                (*(volatile uint32_t *)(RF_BASE + 0x10))
#define RF_ADDR1                (*(volatile uint32_t *)(RF_BASE + 0x14))
#define RF_TX_DATA              (*(volatile uint32_t *)(RF_BASE + 0x20))
#define RF_RX_DATA              (*(volatile uint32_t *)(RF_BASE + 0x24))
#define RF_FIFO_STATUS          (*(volatile uint32_t *)(RF_BASE + 0x28))
#define RF_PKT_CFG              (*(volatile uint32_t *)(RF_BASE + 0x30))
#define RF_CRC_CFG              (*(volatile uint32_t *)(RF_BASE + 0x34))
#define RF_RSSI                 (*(volatile uint32_t *)(RF_BASE + 0x38))
#define RF_INT_EN               (*(volatile uint32_t *)(RF_BASE + 0x40))
#define RF_INT_FLAG             (*(volatile uint32_t *)(RF_BASE + 0x44))

// Control bits
#define RF_CTRL_EN              (1 << 0)
#define RF_CTRL_TX              (1 << 1)
#define RF_CTRL_RX              (1 << 2)
#define RF_CTRL_STBY            (1 << 3)
#define RF_CTRL_AUTO_ACK        (1 << 4)
#define RF_CTRL_CRC_EN          (1 << 5)

// Status bits
#define RF_STATUS_TX_DONE       (1 << 0)
#define RF_STATUS_RX_DONE       (1 << 1)
#define RF_STATUS_ACK_RECV      (1 << 2)
#define RF_STATUS_ACK_TIMEOUT   (1 << 3)
#define RF_STATUS_CRC_ERR       (1 << 4)
#define RF_STATUS_FIFO_FULL     (1 << 5)
#define RF_STATUS_FIFO_EMPTY    (1 << 6)

// FIFO status bits
#define RF_FIFO_TX_FULL         (1 << 0)
#define RF_FIFO_TX_EMPTY        (1 << 1)
#define RF_FIFO_RX_FULL         (1 << 2)
#define RF_FIFO_RX_EMPTY        (1 << 3)

// Interrupt bits
#define RF_INT_TX_DONE          (1 << 0)
#define RF_INT_RX_DONE          (1 << 1)
#define RF_INT_ACK              (1 << 2)

#endif  // CH59X

/*============================================================================
 * Static Variables
 *============================================================================*/

static rf_hw_config_t current_config;
static rf_hw_rx_callback_t rx_callback = NULL;
static rf_hw_tx_callback_t tx_callback = NULL;
static volatile int8_t last_rssi = 0;      // 在ISR中修改，需要volatile
static bool rf_initialized = false;

// TX/RX buffers
static uint8_t tx_buffer[64];
static uint8_t rx_buffer[2][64];            // 双缓冲
static volatile uint8_t rx_active_buf = 0;  // 当前活动缓冲区索引
static volatile uint8_t rx_len = 0;         // 在ISR中修改，需要volatile
static volatile bool rx_pending = false;    // 在ISR中修改，需要volatile

// ACK payload
static uint8_t ack_payload[32];
static volatile uint8_t ack_payload_len = 0;  // 可能在ISR中使用

/*============================================================================
 * Timer for slot timing
 *============================================================================*/

static void (*timer_callback)(void) = NULL;

#ifdef CH59X
__INTERRUPT
__HIGH_CODE
void TMR2_IRQHandler(void)
{
    if (TMR2_GetITFlag(TMR2_IT_CYC_END)) {
        TMR2_ClearITFlag(TMR2_IT_CYC_END);
        if (timer_callback) {
            timer_callback();
        }
    }
}
#endif

/*============================================================================
 * RF Interrupt Handler
 * 注意: CH59X的RF和BLE共享同一个中断向量（向量32）
 * 启动文件中使用BLE_IRQHandler，所以需要添加别名
 *============================================================================*/

#ifdef CH59X
__INTERRUPT
__HIGH_CODE
void RF_IRQHandler(void)
{
    uint32_t status = RF_INT_FLAG;
    RF_INT_FLAG = status;  // Clear flags
    
    if (status & RF_INT_TX_DONE) {
        if (tx_callback) {
            bool ack_ok = (status & RF_INT_ACK) != 0;
            tx_callback(ack_ok || !current_config.auto_ack);
        }
    }
    
    if (status & RF_INT_RX_DONE) {
        // 使用非活动缓冲区接收数据，避免竞态条件
        uint8_t write_buf = 1 - rx_active_buf;
        uint8_t len = 0;
        
        // Read packet from FIFO to inactive buffer
        // 必须完全清空FIFO，即使超过64字节也要读完丢弃
        while (!(RF_FIFO_STATUS & RF_FIFO_RX_EMPTY)) {
            uint8_t byte = (uint8_t)RF_RX_DATA;
            if (len < 64) {
                rx_buffer[write_buf][len++] = byte;
            }
            // 超过64字节时继续读取但丢弃，确保FIFO完全清空
        }
        last_rssi = (int8_t)RF_RSSI;
        
        // 原子切换缓冲区
        rx_len = len;
        rx_active_buf = write_buf;
        rx_pending = true;
        
        // Set ACK payload if available
        if (ack_payload_len > 0 && current_config.auto_ack) {
            for (uint8_t i = 0; i < ack_payload_len; i++) {
                RF_TX_DATA = ack_payload[i];
            }
        }
        
        if (rx_callback && len > 0) {
            rx_callback(rx_buffer[write_buf], len, last_rssi);
        }
    }
}
#endif

/*============================================================================
 * Implementation
 *============================================================================*/

int rf_hw_init(const rf_hw_config_t *config)
{
    if (!config) {
        return -1;
    }
    
    memcpy(&current_config, config, sizeof(current_config));
    
#ifdef CH59X
    // Enable RF peripheral clock
    // 注: BIT_PERI_RF在某些SDK版本中未定义，RF时钟通过其他方式启用
    // PWR_PeriphClkCfg(ENABLE, BIT_PERI_RF);
    
    // Reset RF
    RF_CTRL = 0;
    hal_delay_ms(1);
    
    // Configure packet format
    uint32_t pkt_cfg = 0;
    pkt_cfg |= (config->addr_width - 3) << 0;  // Address width
    pkt_cfg |= (config->mode & 0x03) << 4;     // Data rate
    RF_PKT_CFG = pkt_cfg;
    
    // Configure CRC
    RF_CRC_CFG = config->crc_mode;
    
    // Set sync word (address)
    RF_ADDR0 = config->sync_word;
    
    // Set channel
    RF_CHANNEL = config->channel;
    
    // Set TX power
    RF_POWER = config->tx_power;
    
    // Configure control
    uint32_t ctrl = RF_CTRL_EN;
    if (config->crc_mode != RF_CRC_NONE) {
        ctrl |= RF_CTRL_CRC_EN;
    }
    if (config->auto_ack) {
        ctrl |= RF_CTRL_AUTO_ACK;
    }
    RF_CTRL = ctrl;
    
    // Enable interrupts
    RF_INT_EN = RF_INT_TX_DONE | RF_INT_RX_DONE | RF_INT_ACK;
    PFIC_EnableIRQ(BLE_IRQn);  // RF uses BLE IRQ
    
#endif
    
    rf_initialized = true;
    return 0;
}

int rf_hw_init_default(void)
{
    // SlimeVR 默认配置
    rf_hw_config_t cfg = {
        .mode = RF_MODE_2MBPS,           // 2Mbps for SlimeVR
        .tx_power = RF_TX_POWER_4DBM,    // 4dBm
        .addr_width = RF_ADDR_WIDTH_5,   // 5-byte address
        .crc_mode = RF_CRC_16BIT,        // CRC16
        .sync_word = 0x71764129,         // SlimeVR sync word
        .channel = 2,                    // Default channel
        .auto_ack = true,                // Enable auto ACK
        .ack_timeout = 4,                // 1ms timeout
        .retry_count = 3,                // 3 retries
    };
    return rf_hw_init(&cfg);
}

void rf_hw_deinit(void)
{
#ifdef CH59X
    RF_CTRL = 0;
    RF_INT_EN = 0;
    PFIC_DisableIRQ(BLE_IRQn);
#endif
    rf_initialized = false;
}

int rf_hw_set_channel(uint8_t channel)
{
    if (channel >= RF_CHANNEL_COUNT) {
        return -1;
    }
    
#ifdef CH59X
    RF_CHANNEL = channel;
#endif
    current_config.channel = channel;
    return 0;
}

uint8_t rf_hw_get_channel(void)
{
    return current_config.channel;
}

void rf_hw_set_tx_power(uint8_t power)
{
#ifdef CH59X
    RF_POWER = power & 0x07;
#endif
    current_config.tx_power = power;
}

void rf_hw_set_sync_word(uint32_t sync_word)
{
#ifdef CH59X
    RF_ADDR0 = sync_word;
#endif
    current_config.sync_word = sync_word;
}

void rf_hw_rx_mode(void)
{
#ifdef CH59X
    uint32_t ctrl = RF_CTRL;
    ctrl &= ~(RF_CTRL_TX | RF_CTRL_STBY);
    ctrl |= RF_CTRL_RX;
    RF_CTRL = ctrl;
#endif
}

void rf_hw_tx_mode(void)
{
#ifdef CH59X
    uint32_t ctrl = RF_CTRL;
    ctrl &= ~(RF_CTRL_RX | RF_CTRL_STBY);
    ctrl |= RF_CTRL_TX;
    RF_CTRL = ctrl;
#endif
}

void rf_hw_standby(void)
{
#ifdef CH59X
    uint32_t ctrl = RF_CTRL;
    ctrl &= ~(RF_CTRL_TX | RF_CTRL_RX);
    ctrl |= RF_CTRL_STBY;
    RF_CTRL = ctrl;
#endif
}

void rf_hw_sleep(void)
{
#ifdef CH59X
    RF_CTRL = 0;
#endif
}

void rf_hw_set_mode(uint8_t mode)
{
    switch (mode) {
        case RF_MODE_TX:
            rf_hw_tx_mode();
            break;
        case RF_MODE_RX:
            rf_hw_rx_mode();
            break;
        case RF_MODE_STANDBY:
            rf_hw_standby();
            break;
        case RF_MODE_SLEEP:
            rf_hw_sleep();
            break;
        default:
            // Invalid mode, go to standby
            rf_hw_standby();
            break;
    }
}

int rf_hw_transmit(const uint8_t *data, uint8_t len)
{
    if (!data || len > 32) {
        return -1;
    }
    
#ifdef CH59X
    // v0.4.22 P0修复: Wait for TX FIFO empty with timeout
    uint32_t fifo_timeout = 2000;  // 2ms max wait for FIFO
    while (!(RF_FIFO_STATUS & RF_FIFO_TX_EMPTY) && fifo_timeout--) {
        rf_hw_delay_us(1);
    }
    if (fifo_timeout == 0) {
        // FIFO stuck - flush and reset
        rf_hw_flush_tx();
        return -3;  // FIFO timeout
    }
    
    // Load data into FIFO
    for (uint8_t i = 0; i < len; i++) {
        RF_TX_DATA = data[i];
    }
    
    // Start transmission
    rf_hw_tx_mode();
    
    // Wait for TX done (blocking)
    uint32_t timeout = 10000;
    while (!(RF_STATUS & RF_STATUS_TX_DONE) && timeout--) {
        rf_hw_delay_us(1);
    }
    
    if (timeout == 0) {
        return -2;  // Timeout
    }
    
    return 0;
#else
    (void)data;
    (void)len;
    return -1;
#endif
}

// 非阻塞发送 - 适用于中断/回调上下文
int rf_hw_transmit_async(const uint8_t *data, uint8_t len)
{
    if (!data || len > 32) {
        return -1;
    }
    
#ifdef CH59X
    // 检查TX FIFO是否空闲（非阻塞检查）
    if (!(RF_FIFO_STATUS & RF_FIFO_TX_EMPTY)) {
        return -3;  // FIFO busy
    }
    
    // v0.4.22 P1-01修复: 清除TX_DONE/ACK标志，避免伪完成检测
    RF_INT_FLAG = RF_INT_TX_DONE | RF_INT_ACK;
    
    // Load data into FIFO
    for (uint8_t i = 0; i < len; i++) {
        RF_TX_DATA = data[i];
    }
    
    // Start transmission (non-blocking, completion via interrupt)
    rf_hw_tx_mode();
    
    return 0;  // 发送已启动，完成通过TX_DONE中断通知
#else
    (void)data;
    (void)len;
    return -1;
#endif
}

int rf_hw_transmit_ack(const uint8_t *data, uint8_t len,
                        uint8_t *ack_buf, uint8_t *ack_len)
{
    if (!data || len > 32) {
        return -1;
    }
    
#ifdef CH59X
    // Clear status
    RF_INT_FLAG = RF_INT_TX_DONE | RF_INT_ACK;
    
    // v0.4.22 P0修复: Load data into FIFO with timeout
    uint32_t fifo_timeout = 2000;  // 2ms max wait for FIFO
    while (!(RF_FIFO_STATUS & RF_FIFO_TX_EMPTY) && fifo_timeout--) {
        rf_hw_delay_us(1);
    }
    if (fifo_timeout == 0) {
        rf_hw_flush_tx();
        return -3;  // FIFO timeout
    }
    
    for (uint8_t i = 0; i < len; i++) {
        RF_TX_DATA = data[i];
    }
    
    // Start transmission
    rf_hw_tx_mode();
    
    // Wait for TX done or ACK timeout
    uint32_t timeout = 50000;  // 50ms max
    while (timeout--) {
        uint32_t status = RF_STATUS;
        
        if (status & RF_STATUS_ACK_RECV) {
            // ACK received - read payload if any
            if (ack_buf && ack_len) {
                *ack_len = 0;
                while (!(RF_FIFO_STATUS & RF_FIFO_RX_EMPTY) && *ack_len < 32) {
                    ack_buf[(*ack_len)++] = (uint8_t)RF_RX_DATA;
                }
            }
            return 0;  // Success with ACK
        }
        
        if (status & RF_STATUS_ACK_TIMEOUT) {
            return 1;  // Timeout, no ACK
        }
        
        if (status & RF_STATUS_TX_DONE) {
            if (!current_config.auto_ack) {
                return 0;  // Success without ACK
            }
        }
        
        rf_hw_delay_us(1);
    }
    
    return -2;  // Timeout
#else
    (void)data;
    (void)len;
    (void)ack_buf;
    (void)ack_len;
    return -1;
#endif
}

bool rf_hw_rx_available(void)
{
#ifdef CH59X
    return rx_pending || !(RF_FIFO_STATUS & RF_FIFO_RX_EMPTY);
#else
    return rx_pending;
#endif
}

int rf_hw_receive(uint8_t *data, uint8_t max_len, int8_t *rssi)
{
    if (!data) {
        return -1;
    }
    
    if (rx_pending) {
        // 临界区：禁用中断以确保数据一致性
        __disable_irq();
        uint8_t read_buf = rx_active_buf;
        uint8_t len = (rx_len < max_len) ? rx_len : max_len;
        memcpy(data, rx_buffer[read_buf], len);
        if (rssi) {
            *rssi = last_rssi;
        }
        rx_pending = false;
        __enable_irq();
        return len;
    }
    
#ifdef CH59X
    if (RF_FIFO_STATUS & RF_FIFO_RX_EMPTY) {
        return 0;  // No data
    }
    
    uint8_t len = 0;
    while (!(RF_FIFO_STATUS & RF_FIFO_RX_EMPTY) && len < max_len) {
        data[len++] = (uint8_t)RF_RX_DATA;
    }
    
    if (rssi) {
        *rssi = (int8_t)RF_RSSI;
    }
    
    return len;
#else
    return 0;
#endif
}

void rf_hw_set_ack_payload(const uint8_t *data, uint8_t len)
{
    if (len > 32) len = 32;
    
    if (data && len > 0) {
        memcpy(ack_payload, data, len);
        ack_payload_len = len;
    } else {
        ack_payload_len = 0;
    }
}

void rf_hw_flush_tx(void)
{
#ifdef CH59X
    // v0.4.22 P0修复: 添加超时保护
    uint32_t timeout = 1000;
    while (!(RF_FIFO_STATUS & RF_FIFO_TX_EMPTY) && timeout--) {
        volatile uint32_t dummy = RF_TX_DATA;
        (void)dummy;
    }
#endif
}

void rf_hw_flush_rx(void)
{
#ifdef CH59X
    // v0.4.22 P0修复: 添加超时保护
    uint32_t timeout = 1000;
    while (!(RF_FIFO_STATUS & RF_FIFO_RX_EMPTY) && timeout--) {
        volatile uint32_t dummy = RF_RX_DATA;
        (void)dummy;
    }
#endif
    rx_pending = false;
    rx_len = 0;
}

int8_t rf_hw_get_rssi(void)
{
    return last_rssi;
}

bool rf_hw_carrier_detect(void)
{
#ifdef CH59X
    // Check if carrier is present on current channel
    return (RF_STATUS & (1 << 7)) != 0;
#else
    return false;
#endif
}

void rf_hw_set_rx_callback(rf_hw_rx_callback_t callback)
{
    rx_callback = callback;
}

void rf_hw_set_tx_callback(rf_hw_tx_callback_t callback)
{
    tx_callback = callback;
}

void rf_hw_get_mac_address(uint8_t *mac)
{
    if (!mac) return;
    
#ifdef CH59X
    GetMACAddress(mac);
#else
    // Generate fake MAC for testing
    mac[0] = 0x02;
    mac[1] = 0x00;
    mac[2] = 0x00;
    mac[3] = 0x12;
    mac[4] = 0x34;
    mac[5] = 0x56;
#endif
}

void rf_hw_enable_irq(bool enable)
{
#ifdef CH59X
    if (enable) {
        PFIC_EnableIRQ(BLE_IRQn);
    } else {
        PFIC_DisableIRQ(BLE_IRQn);
    }
#endif
    (void)enable;
}

void rf_hw_irq_handler(void)
{
#ifdef CH59X
    RF_IRQHandler();
#endif
}

/*============================================================================
 * Timing Functions
 *============================================================================*/

uint32_t rf_hw_get_time_us(void)
{
    return hal_micros();
}

void rf_hw_delay_us(uint32_t us)
{
    hal_delay_us(us);
}

void rf_hw_start_timer(uint32_t period_us, void (*callback)(void))
{
    // P0-3: 先停止之前的定时器，避免资源泄漏
    rf_hw_stop_timer();
    
    timer_callback = callback;
    
#ifdef CH59X
    // Use TMR2 for slot timing
    uint32_t ticks = (GetSysClock() / 1000000) * period_us;
    TMR2_TimerInit(ticks);
    TMR2_ITCfg(ENABLE, TMR2_IT_CYC_END);
    PFIC_EnableIRQ(TMR2_IRQn);
#endif
    (void)period_us;
}

void rf_hw_stop_timer(void)
{
#ifdef CH59X
    TMR2_ITCfg(DISABLE, TMR2_IT_CYC_END);
    PFIC_DisableIRQ(TMR2_IRQn);
#endif
    timer_callback = NULL;
}

/*============================================================================
 * BLE_IRQHandler 别名
 * CH59X的RF和BLE共享同一个中断向量（向量32）
 * 启动文件向量表使用BLE_IRQHandler，这里创建别名指向RF_IRQHandler
 *============================================================================*/
#ifdef CH59X
void BLE_IRQHandler(void) __attribute__((alias("RF_IRQHandler")));
#endif
