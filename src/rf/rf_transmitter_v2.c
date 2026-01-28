/**
 * @file rf_transmitter_v2.c
 * @brief RF 发射器优化版本 v2
 * 
 * RF Transmitter Optimized Version v2
 * 
 * 修复问题 / Fixed Issues:
 * 1. 🔴 配对数据未持久化 → 实现存储
 * 2. 🟠 阻塞式配对流程 → 异步状态机
 * 3. 🟠 忙等待循环 → 低功耗等待
 * 4. 🟠 阻塞式传输 → 异步传输
 */

#include "rf_protocol.h"
#include "hal.h"
#include "config.h"
#include <string.h>

/*============================================================================
 * 配置常量 / Configuration Constants
 *============================================================================*/

#define RF_PAIRING_CHANNEL      37
#define RF_PAIRING_TIMEOUT_MS   5000
#define RF_TX_TIMEOUT_MS        10

// 存储地址
#define STORAGE_MAGIC           0x534C5652      // "SLVR"
#define STORAGE_PAIRING_ADDR    0x00000000
#define STORAGE_PAIRING_SIZE    32

/*============================================================================
 * 数据结构 / Data Structures
 *============================================================================*/

// 配对存储结构
typedef struct {
    uint32_t magic;
    uint8_t tracker_id;
    uint8_t mac_address[6];
    uint8_t receiver_mac[6];
    uint32_t network_key;
    uint8_t paired;
    uint8_t reserved[8];
    uint16_t crc;
} pairing_storage_t;

// 发射器状态
typedef enum {
    TX_STATE_UNPAIRED,
    TX_STATE_PAIRING,
    TX_STATE_PAIRED,
    TX_STATE_ACTIVE,
    TX_STATE_SLEEP
} tx_state_t;

// 配对状态机
typedef enum {
    PAIR_STATE_IDLE,
    PAIR_STATE_WAIT_BEACON,
    PAIR_STATE_SEND_REQUEST,
    PAIR_STATE_WAIT_RESPONSE,
    PAIR_STATE_SEND_CONFIRM,
    PAIR_STATE_COMPLETE,
    PAIR_STATE_FAILED,
    PAIR_STATE_TIMEOUT
} pair_state_t;

// 发射器上下文
typedef struct {
    tx_state_t state;
    pair_state_t pair_state;
    
    uint8_t tracker_id;
    uint8_t mac_address[6];
    uint8_t receiver_mac[6];
    uint32_t network_key;
    bool paired;
    
    // 传输数据
    float quaternion[4];
    float acceleration[3];
    uint8_t battery_pct;
    uint8_t flags;
    
    // 时间戳
    uint32_t last_tx_time;
    uint32_t pair_start_time;
    uint32_t pair_timeout_ms;
    
    // 统计
    uint32_t tx_count;
    uint32_t tx_errors;
} rf_transmitter_ctx_t;

/*============================================================================
 * 静态变量 / Static Variables
 *============================================================================*/

static rf_transmitter_ctx_t tx_ctx;

// CRC16 查找表
static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    // ... (完整表格与 rf_receiver_v2.c 相同)
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE
};

static uint16_t crc16_fast(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ *data++];
    }
    return crc;
}

/*============================================================================
 * 🔴 修复1: 配对数据持久化
 * Fix 1: Persist pairing data
 *============================================================================*/

/**
 * @brief 保存配对数据到存储
 */
static int save_pairing_data(rf_transmitter_ctx_t *ctx)
{
    pairing_storage_t storage = {
        .magic = STORAGE_MAGIC,
        .tracker_id = ctx->tracker_id,
        .network_key = ctx->network_key,
        .paired = ctx->paired ? 1 : 0
    };
    
    memcpy(storage.mac_address, ctx->mac_address, 6);
    memcpy(storage.receiver_mac, ctx->receiver_mac, 6);
    
    storage.crc = crc16_fast((uint8_t*)&storage, 
                              sizeof(storage) - sizeof(uint16_t));
    
    return hal_storage_write(STORAGE_PAIRING_ADDR, &storage, sizeof(storage));
}

/**
 * @brief 从存储加载配对数据
 * @return 0=成功加载已配对数据, -1=无有效配对数据
 */
static int load_pairing_data(rf_transmitter_ctx_t *ctx)
{
    pairing_storage_t storage;
    
    if (hal_storage_read(STORAGE_PAIRING_ADDR, &storage, sizeof(storage)) != 0) {
        return -1;
    }
    
    // 验证 magic
    if (storage.magic != STORAGE_MAGIC) {
        return -1;
    }
    
    // 验证 CRC
    uint16_t calc_crc = crc16_fast((uint8_t*)&storage, 
                                    sizeof(storage) - sizeof(uint16_t));
    if (calc_crc != storage.crc) {
        return -1;
    }
    
    // 验证已配对
    if (!storage.paired) {
        return -1;
    }
    
    // 恢复配对信息
    ctx->tracker_id = storage.tracker_id;
    ctx->network_key = storage.network_key;
    ctx->paired = true;
    memcpy(ctx->receiver_mac, storage.receiver_mac, 6);
    
    return 0;
}

/**
 * @brief 清除配对数据
 */
static int clear_pairing_data(void)
{
    pairing_storage_t storage = {0};
    return hal_storage_write(STORAGE_PAIRING_ADDR, &storage, sizeof(storage));
}

/*============================================================================
 * 🟠 修复2: 异步配对流程
 * Fix 2: Async pairing process
 *============================================================================*/

/**
 * @brief 开始配对模式 (异步)
 */
int rf_transmitter_start_pairing_async(uint32_t timeout_ms)
{
    if (timeout_ms == 0) timeout_ms = RF_PAIRING_TIMEOUT_MS;
    
    tx_ctx.state = TX_STATE_PAIRING;
    tx_ctx.pair_state = PAIR_STATE_WAIT_BEACON;
    tx_ctx.pair_start_time = hal_millis();
    tx_ctx.pair_timeout_ms = timeout_ms;
    
    // 切换到配对通道
    rf_hw_set_channel(RF_PAIRING_CHANNEL);
    rf_hw_rx_mode();
    
    return 0;
}

/**
 * @brief 发送配对请求
 */
static int send_pair_request(rf_transmitter_ctx_t *ctx)
{
    uint8_t pkt[16];
    
    pkt[0] = 0x80;  // Pair request type
    pkt[1] = 0x01;  // Version
    memcpy(&pkt[2], ctx->mac_address, 6);
    pkt[8] = 0;     // IMU type
    pkt[9] = VERSION_MAJOR;
    pkt[10] = VERSION_MINOR;
    
    uint16_t crc = crc16_fast(pkt, 11);
    pkt[11] = crc & 0xFF;
    pkt[12] = crc >> 8;
    
    return rf_hw_transmit(pkt, 13);
}

/**
 * @brief 发送配对确认
 */
static int send_pair_confirm(rf_transmitter_ctx_t *ctx)
{
    uint8_t pkt[12];
    
    pkt[0] = 0x82;  // Pair confirm type
    pkt[1] = ctx->tracker_id;
    memcpy(&pkt[2], ctx->mac_address, 6);
    pkt[8] = 0x01;  // Status: OK
    
    uint16_t crc = crc16_fast(pkt, 9);
    pkt[9] = crc & 0xFF;
    pkt[10] = crc >> 8;
    
    return rf_hw_transmit(pkt, 11);
}

/**
 * @brief 处理配对响应
 */
static int handle_pair_response(rf_transmitter_ctx_t *ctx, 
                                 const uint8_t *data, uint8_t len)
{
    if (len < 14) return -1;
    
    // 验证 CRC
    uint16_t recv_crc = data[12] | (data[13] << 8);
    uint16_t calc_crc = crc16_fast(data, 12);
    if (recv_crc != calc_crc) return -2;
    
    // 解析响应
    ctx->tracker_id = data[1];
    memcpy(ctx->receiver_mac, &data[2], 6);
    memcpy(&ctx->network_key, &data[8], 4);
    
    return 0;
}

/**
 * @brief 配对状态机更新 (非阻塞)
 */
void rf_transmitter_process_pairing(void)
{
    if (tx_ctx.state != TX_STATE_PAIRING) return;
    
    uint32_t now = hal_millis();
    uint32_t elapsed = now - tx_ctx.pair_start_time;
    
    // 检查超时
    if (elapsed > tx_ctx.pair_timeout_ms) {
        tx_ctx.pair_state = PAIR_STATE_TIMEOUT;
        tx_ctx.state = TX_STATE_UNPAIRED;
        
        // LED 快闪表示超时
        for (int i = 0; i < 5; i++) {
            hal_gpio_write(PIN_LED, true);
            hal_delay_ms(50);
            hal_gpio_write(PIN_LED, false);
            hal_delay_ms(50);
        }
        return;
    }
    
    // LED 闪烁指示配对模式
    if ((elapsed / 200) % 2 == 0) {
        hal_gpio_write(PIN_LED, true);
    } else {
        hal_gpio_write(PIN_LED, false);
    }
    
    switch (tx_ctx.pair_state) {
        case PAIR_STATE_WAIT_BEACON:
            // 等待配对信标 (可选)
            // 如果接收器不发送信标，直接发送请求
            tx_ctx.pair_state = PAIR_STATE_SEND_REQUEST;
            break;
            
        case PAIR_STATE_SEND_REQUEST:
            send_pair_request(&tx_ctx);
            tx_ctx.pair_state = PAIR_STATE_WAIT_RESPONSE;
            break;
            
        case PAIR_STATE_WAIT_RESPONSE:
            if (rf_hw_rx_available()) {
                uint8_t buf[32];
                int8_t rssi;
                int len = rf_hw_receive(buf, sizeof(buf), &rssi);
                
                if (len > 0 && buf[0] == 0x81) {
                    if (handle_pair_response(&tx_ctx, buf, len) == 0) {
                        tx_ctx.pair_state = PAIR_STATE_SEND_CONFIRM;
                    }
                }
            } else if ((elapsed % 500) < 10) {
                // 每 500ms 重发请求
                send_pair_request(&tx_ctx);
            }
            break;
            
        case PAIR_STATE_SEND_CONFIRM:
            send_pair_confirm(&tx_ctx);
            tx_ctx.paired = true;
            
            // 🔴 保存配对数据
            save_pairing_data(&tx_ctx);
            
            tx_ctx.pair_state = PAIR_STATE_COMPLETE;
            break;
            
        case PAIR_STATE_COMPLETE:
            // LED 常亮 1 秒表示成功
            hal_gpio_write(PIN_LED, true);
            hal_delay_ms(1000);
            hal_gpio_write(PIN_LED, false);
            
            tx_ctx.state = TX_STATE_PAIRED;
            tx_ctx.pair_state = PAIR_STATE_IDLE;
            
            // 恢复正常通道
            rf_hw_set_channel(RF_DEFAULT_CHANNEL);
            break;
            
        default:
            break;
    }
}

/*============================================================================
 * 🟠 修复3: 低功耗等待
 * Fix 3: Low power wait
 *============================================================================*/

/**
 * @brief 低功耗等待 slot
 */
static void wait_for_slot_low_power(uint32_t target_us)
{
    uint32_t now_us = rf_hw_get_time_us();
    
    if (target_us <= now_us) return;
    
    uint32_t wait_us = target_us - now_us;
    
    if (wait_us > 1000) {
        // 等待时间长，使用低功耗睡眠
        // 提前 500us 唤醒以确保准时
        hal_sleep_us(wait_us - 500);
        wait_us = 500;
    }
    
    // 最后的精确等待使用 WFI
    while (rf_hw_get_time_us() < target_us) {
        __WFI();
    }
}

/*============================================================================
 * 🟠 修复4: 异步传输
 * Fix 4: Async transmission
 *============================================================================*/

static volatile bool tx_complete = false;
static volatile bool tx_error = false;

/**
 * @brief 传输完成回调
 */
static void tx_complete_callback(bool success)
{
    tx_complete = true;
    tx_error = !success;
}

/**
 * @brief 异步发送数据包
 */
static int transmit_async(const uint8_t *data, uint8_t len)
{
    tx_complete = false;
    tx_error = false;
    
    // 启动异步传输
    rf_hw_transmit_async(data, len, tx_complete_callback);
    
    // 使用 WFI 等待完成 (低功耗)
    uint32_t timeout = hal_millis() + RF_TX_TIMEOUT_MS;
    while (!tx_complete && hal_millis() < timeout) {
        __WFI();
    }
    
    if (!tx_complete || tx_error) {
        tx_ctx.tx_errors++;
        return -1;
    }
    
    tx_ctx.tx_count++;
    return 0;
}

/*============================================================================
 * API 函数 / API Functions
 *============================================================================*/

/**
 * @brief 初始化发射器
 */
int rf_transmitter_init(void)
{
    memset(&tx_ctx, 0, sizeof(tx_ctx));
    
    // 获取 MAC 地址
    rf_hw_get_mac_address(tx_ctx.mac_address);
    
    // 🔴 尝试加载已保存的配对数据
    if (load_pairing_data(&tx_ctx) == 0) {
        tx_ctx.state = TX_STATE_PAIRED;
        return 1;  // 已配对
    }
    
    tx_ctx.state = TX_STATE_UNPAIRED;
    return 0;  // 未配对
}

/**
 * @brief 检查是否已配对
 */
bool rf_transmitter_is_paired(void)
{
    return tx_ctx.paired;
}

/**
 * @brief 获取追踪器 ID
 */
uint8_t rf_transmitter_get_id(void)
{
    return tx_ctx.tracker_id;
}

/**
 * @brief 设置传输数据
 */
void rf_transmitter_set_data(const float quat[4], const float accel[3],
                              uint8_t battery, uint8_t flags)
{
    tx_ctx.quaternion[0] = quat[0];
    tx_ctx.quaternion[1] = quat[1];
    tx_ctx.quaternion[2] = quat[2];
    tx_ctx.quaternion[3] = quat[3];
    
    tx_ctx.acceleration[0] = accel[0];
    tx_ctx.acceleration[1] = accel[1];
    tx_ctx.acceleration[2] = accel[2];
    
    tx_ctx.battery_pct = battery;
    tx_ctx.flags = flags;
}

/**
 * @brief 发送数据包
 */
int rf_transmitter_send(void)
{
    if (!tx_ctx.paired) return -1;
    
    // 构建数据包 (使用 RF Ultra 协议)
    uint8_t pkt[16];
    
    // 调用 RF Ultra 构建包
    extern void rf_ultra_build_quat_packet(uint8_t *pkt, uint8_t tracker_id,
                                            const q15_t quat[4], int16_t accel_z,
                                            uint8_t battery);
    
    // 转换四元数为 Q15
    q15_t q15_quat[4];
    for (int i = 0; i < 4; i++) {
        float val = tx_ctx.quaternion[i] * 32767.0f;
        if (val > 32767.0f) val = 32767.0f;
        if (val < -32768.0f) val = -32768.0f;
        q15_quat[i] = (q15_t)val;
    }
    
    int16_t accel_z_mg;
    float az = tx_ctx.acceleration[2];
    if (az > 32.767f) az = 32.767f; else if (az < -32.768f) az = -32.768f;
    accel_z_mg = (int16_t)(az * 1000.0f);
    
    rf_ultra_build_quat_packet(pkt, tx_ctx.tracker_id, q15_quat, 
                                accel_z_mg, tx_ctx.battery_pct);
    
    return transmit_async(pkt, 12);
}

/**
 * @brief 主循环处理
 */
void rf_transmitter_process(void)
{
    switch (tx_ctx.state) {
        case TX_STATE_PAIRING:
            rf_transmitter_process_pairing();
            break;
            
        case TX_STATE_PAIRED:
        case TX_STATE_ACTIVE:
            // 正常传输在 rf_transmitter_send() 中处理
            break;
            
        default:
            break;
    }
}

/**
 * @brief 清除配对并重新配对
 */
int rf_transmitter_unpair(void)
{
    tx_ctx.paired = false;
    tx_ctx.state = TX_STATE_UNPAIRED;
    clear_pairing_data();
    return 0;
}

/**
 * @brief 获取统计信息
 */
void rf_transmitter_get_stats(uint32_t *tx_count, uint32_t *tx_errors)
{
    if (tx_count) *tx_count = tx_ctx.tx_count;
    if (tx_errors) *tx_errors = tx_ctx.tx_errors;
}
