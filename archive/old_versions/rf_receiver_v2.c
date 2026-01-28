/**
 * @file rf_receiver_v2.c
 * @brief RF 接收器优化版本 v2
 * 
 * RF Receiver Optimized Version v2
 * 
 * 修复问题 / Fixed Issues:
 * 1. 🔴 network_key 硬编码 → 随机生成并持久化
 * 2. 🔴 配对数据未保存 → 实现持久化存储
 * 3. 🟠 阻塞式配对流程 → 异步状态机
 * 4. 🟠 配对冲突处理 → 改进 slot 分配
 * 5. 🟢 CRC 计算优化 → 查找表
 */

#include "rf_protocol.h"
#include "hal.h"
#include "config.h"
#include <string.h>

/*============================================================================
 * 配置常量 / Configuration Constants
 *============================================================================*/

#include "config.h"  // 使用统一的 MAX_TRACKERS

#define RF_MAX_TRACKERS         MAX_TRACKERS  // 使用 config.h 中的定义
#define RF_PAIRING_CHANNEL      37
#define RF_PAIRING_TIMEOUT_MS   30000
#define RF_SYNC_INTERVAL_MS     5

// 存储地址 / Storage addresses
#define STORAGE_MAGIC           0x534C5652      // "SLVR"
#define STORAGE_NETWORK_KEY     0x00000000
#define STORAGE_TRACKER_BASE    0x00000100
#define STORAGE_TRACKER_SIZE    16

/*============================================================================
 * CRC16 查找表 (优化性能) / CRC16 Lookup Table (Optimized)
 *============================================================================*/

static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
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
 * 数据结构 / Data Structures
 *============================================================================*/

// 追踪器信息
typedef struct {
    uint8_t mac_address[6];
    uint8_t active;
    uint8_t reserved;
    uint32_t last_seen_ms;
    int8_t rssi;
    uint8_t packet_loss;
} tracker_info_t;

// 接收器状态
typedef enum {
    RX_STATE_IDLE,
    RX_STATE_NORMAL,
    RX_STATE_PAIRING,
    RX_STATE_ERROR
} rx_state_t;

// 接收器上下文
typedef struct {
    rx_state_t state;
    uint32_t network_key;
    uint8_t mac_address[6];
    tracker_info_t trackers[RF_MAX_TRACKERS];
    uint8_t active_count;
    uint32_t last_sync_ms;
    uint32_t pairing_timeout_ms;
    
    // 统计
    uint32_t rx_count;
    uint32_t crc_errors;
} rf_receiver_ctx_t;

// 存储数据结构
typedef struct {
    uint32_t magic;
    uint32_t network_key;
    uint16_t crc;
} network_key_storage_t;

typedef struct {
    uint8_t mac_address[6];
    uint8_t active;
    uint8_t tracker_id;
    uint32_t paired_time;
    uint16_t crc;
} tracker_storage_t;

/*============================================================================
 * 静态变量 / Static Variables
 *============================================================================*/

static rf_receiver_ctx_t rx_ctx;

/*============================================================================
 * 🔴 修复1: network_key 随机生成并持久化
 * Fix 1: Generate random network_key and persist
 *============================================================================*/

/**
 * @brief 生成随机 network_key
 * 
 * 使用硬件随机数生成器，如果不可用则使用 MAC 地址作为种子
 */
static uint32_t generate_network_key(void)
{
    uint32_t key = 0;
    
#ifdef CH59X
    // 尝试使用硬件 RNG
    // CH592 有内置 TRNG
    extern uint32_t TRNG_GetRand(void);
    key = TRNG_GetRand();
#endif
    
    // 如果硬件 RNG 失败或不可用，使用 MAC 地址 + 时间
    if (key == 0 || key == 0xFFFFFFFF) {
        uint8_t mac[6];
        hal_get_mac_address(mac);
        
        key = mac[0] | (mac[1] << 8) | (mac[2] << 16) | (mac[3] << 24);
        key ^= (mac[4] | (mac[5] << 8)) << 16;
        key ^= hal_millis();
        
        // 简单的 LFSR 混淆
        for (int i = 0; i < 16; i++) {
            key ^= key << 13;
            key ^= key >> 17;
            key ^= key << 5;
        }
    }
    
    // 确保不为 0 或全 1
    if (key == 0) key = 0xDEADBEEF;
    if (key == 0xFFFFFFFF) key = 0xCAFEBABE;
    
    return key;
}

/**
 * @brief 加载或生成 network_key
 */
static int load_or_generate_network_key(rf_receiver_ctx_t *ctx)
{
    network_key_storage_t storage;
    
    // 尝试从存储加载
    if (hal_storage_read(STORAGE_NETWORK_KEY, &storage, sizeof(storage)) == 0) {
        // 验证 magic 和 CRC
        if (storage.magic == STORAGE_MAGIC) {
            uint16_t calc_crc = crc16_fast((uint8_t*)&storage, 
                                           sizeof(storage) - sizeof(uint16_t));
            if (calc_crc == storage.crc) {
                ctx->network_key = storage.network_key;
                return 0;  // 加载成功
            }
        }
    }
    
    // 生成新的 network_key
    ctx->network_key = generate_network_key();
    
    // 保存到存储
    storage.magic = STORAGE_MAGIC;
    storage.network_key = ctx->network_key;
    storage.crc = crc16_fast((uint8_t*)&storage, sizeof(storage) - sizeof(uint16_t));
    
    hal_storage_write(STORAGE_NETWORK_KEY, &storage, sizeof(storage));
    
    return 1;  // 生成了新的 key
}

/*============================================================================
 * 🔴 修复2: 配对数据持久化
 * Fix 2: Persist pairing data
 *============================================================================*/

/**
 * @brief 保存追踪器配对信息
 */
static int save_tracker_pairing(uint8_t tracker_id)
{
    if (tracker_id >= RF_MAX_TRACKERS) return -1;
    
    tracker_info_t *tracker = &rx_ctx.trackers[tracker_id];
    tracker_storage_t storage;
    
    memcpy(storage.mac_address, tracker->mac_address, 6);
    storage.active = tracker->active;
    storage.tracker_id = tracker_id;
    storage.paired_time = hal_millis();
    storage.crc = crc16_fast((uint8_t*)&storage, sizeof(storage) - sizeof(uint16_t));
    
    uint32_t addr = STORAGE_TRACKER_BASE + tracker_id * STORAGE_TRACKER_SIZE;
    return hal_storage_write(addr, &storage, sizeof(storage));
}

/**
 * @brief 加载所有已配对的追踪器
 */
static int load_paired_trackers(rf_receiver_ctx_t *ctx)
{
    int loaded = 0;
    tracker_storage_t storage;
    
    for (int i = 0; i < RF_MAX_TRACKERS; i++) {
        uint32_t addr = STORAGE_TRACKER_BASE + i * STORAGE_TRACKER_SIZE;
        
        if (hal_storage_read(addr, &storage, sizeof(storage)) == 0) {
            // 验证 CRC
            uint16_t calc_crc = crc16_fast((uint8_t*)&storage, 
                                           sizeof(storage) - sizeof(uint16_t));
            
            if (calc_crc == storage.crc && storage.active) {
                memcpy(ctx->trackers[i].mac_address, storage.mac_address, 6);
                ctx->trackers[i].active = 1;
                loaded++;
            }
        }
    }
    
    ctx->active_count = loaded;
    return loaded;
}

/**
 * @brief 清除追踪器配对信息
 */
static int clear_tracker_pairing(uint8_t tracker_id)
{
    if (tracker_id >= RF_MAX_TRACKERS) return -1;
    
    tracker_storage_t storage = {0};
    uint32_t addr = STORAGE_TRACKER_BASE + tracker_id * STORAGE_TRACKER_SIZE;
    
    rx_ctx.trackers[tracker_id].active = 0;
    memset(rx_ctx.trackers[tracker_id].mac_address, 0, 6);
    
    return hal_storage_write(addr, &storage, sizeof(storage));
}

/*============================================================================
 * 🟠 修复3: 异步配对流程
 * Fix 3: Async pairing process
 *============================================================================*/

// 配对状态机
typedef enum {
    PAIR_STATE_IDLE,
    PAIR_STATE_LISTENING,
    PAIR_STATE_RESPONDING,
    PAIR_STATE_CONFIRMING,
    PAIR_STATE_COMPLETE,
    PAIR_STATE_FAILED,
    PAIR_STATE_TIMEOUT
} pair_state_t;

static struct {
    pair_state_t state;
    uint32_t start_time;
    uint32_t timeout_ms;
    uint8_t pending_mac[6];
    uint8_t pending_id;
} pair_ctx;

/**
 * @brief 开始配对模式 (异步)
 */
int rf_receiver_start_pairing_async(uint32_t timeout_ms)
{
    if (timeout_ms == 0) timeout_ms = RF_PAIRING_TIMEOUT_MS;
    
    // 切换到配对通道
    rf_hw_set_channel(RF_PAIRING_CHANNEL);
    rf_hw_rx_mode();
    
    rx_ctx.state = RX_STATE_PAIRING;
    pair_ctx.state = PAIR_STATE_LISTENING;
    pair_ctx.start_time = hal_millis();
    pair_ctx.timeout_ms = timeout_ms;
    
    return 0;
}

/**
 * @brief 停止配对模式
 */
int rf_receiver_stop_pairing(void)
{
    rx_ctx.state = RX_STATE_NORMAL;
    pair_ctx.state = PAIR_STATE_IDLE;
    
    // 恢复正常通道
    rf_hw_set_channel(RF_DEFAULT_CHANNEL);
    
    return 0;
}

/**
 * @brief 处理配对请求
 */
static int handle_pair_request(const uint8_t *data, uint8_t len)
{
    if (len < 10) return -1;
    
    // 解析配对请求
    uint8_t *mac = (uint8_t*)&data[2];
    
    // 查找空闲 slot 或已有 MAC
    int8_t slot = -1;
    bool already_paired = false;
    
    for (int i = 0; i < RF_MAX_TRACKERS; i++) {
        if (memcmp(rx_ctx.trackers[i].mac_address, mac, 6) == 0) {
            slot = i;
            already_paired = rx_ctx.trackers[i].active;
            break;
        } else if (!rx_ctx.trackers[i].active && slot < 0) {
            slot = i;
        }
    }
    
    if (slot < 0) {
        // 无空闲 slot
        return -2;
    }
    
    // 保存待配对信息
    memcpy(pair_ctx.pending_mac, mac, 6);
    pair_ctx.pending_id = slot;
    pair_ctx.state = PAIR_STATE_RESPONDING;
    
    // 构建并发送响应
    uint8_t response[16];
    response[0] = 0x81;  // Pair response type
    response[1] = slot;
    memcpy(&response[2], rx_ctx.mac_address, 6);
    memcpy(&response[8], &rx_ctx.network_key, 4);
    
    uint16_t crc = crc16_fast(response, 12);
    response[12] = crc & 0xFF;
    response[13] = crc >> 8;
    
    rf_hw_transmit(response, 14);
    
    return 0;
}

/**
 * @brief 处理配对确认
 */
static int handle_pair_confirm(const uint8_t *data, uint8_t len)
{
    if (len < 8) return -1;
    if (pair_ctx.state != PAIR_STATE_RESPONDING) return -2;
    
    uint8_t tracker_id = data[1];
    uint8_t *mac = (uint8_t*)&data[2];
    
    // 验证 MAC 和 ID
    if (tracker_id != pair_ctx.pending_id) return -3;
    if (memcmp(mac, pair_ctx.pending_mac, 6) != 0) return -4;
    
    // 激活追踪器
    tracker_info_t *tracker = &rx_ctx.trackers[tracker_id];
    memcpy(tracker->mac_address, mac, 6);
    tracker->active = 1;
    tracker->last_seen_ms = hal_millis();
    
    rx_ctx.active_count++;
    
    // 🔴 保存配对信息到存储
    save_tracker_pairing(tracker_id);
    
    pair_ctx.state = PAIR_STATE_COMPLETE;
    
    return 0;
}

/**
 * @brief 配对状态机更新 (非阻塞)
 */
void rf_receiver_process_pairing(void)
{
    if (rx_ctx.state != RX_STATE_PAIRING) return;
    
    uint32_t now = hal_millis();
    uint32_t elapsed = now - pair_ctx.start_time;
    
    // 检查超时
    if (elapsed > pair_ctx.timeout_ms) {
        pair_ctx.state = PAIR_STATE_TIMEOUT;
        rf_receiver_stop_pairing();
        return;
    }
    
    // LED 指示配对模式 (闪烁)
    if ((elapsed / 250) % 2 == 0) {
        hal_gpio_write(PIN_LED, true);
    } else {
        hal_gpio_write(PIN_LED, false);
    }
    
    // 处理接收数据
    if (rf_hw_rx_available()) {
        uint8_t buf[32];
        int8_t rssi;
        int len = rf_hw_receive(buf, sizeof(buf), &rssi);
        
        if (len > 0) {
            uint8_t pkt_type = buf[0] & 0xF0;
            
            switch (pkt_type) {
                case 0x80:  // Pair request
                    handle_pair_request(buf, len);
                    break;
                    
                case 0x82:  // Pair confirm
                    handle_pair_confirm(buf, len);
                    break;
            }
        }
    }
    
    // 配对完成检查
    if (pair_ctx.state == PAIR_STATE_COMPLETE) {
        // LED 快闪 3 次表示成功
        for (int i = 0; i < 3; i++) {
            hal_gpio_write(PIN_LED, true);
            hal_delay_ms(100);
            hal_gpio_write(PIN_LED, false);
            hal_delay_ms(100);
        }
        rf_receiver_stop_pairing();
    }
}

/*============================================================================
 * 🟠 修复4: 改进的数据接收 (非阻塞)
 * Fix 4: Improved data reception (non-blocking)
 *============================================================================*/

/**
 * @brief 处理接收到的数据包 (非阻塞)
 */
int rf_receiver_process(void)
{
    if (rx_ctx.state == RX_STATE_PAIRING) {
        rf_receiver_process_pairing();
        return 0;
    }
    
    if (rx_ctx.state != RX_STATE_NORMAL) {
        return -1;
    }
    
    // 非阻塞检查数据
    if (!rf_hw_rx_available()) {
        return 0;
    }
    
    uint8_t buf[32];
    int8_t rssi;
    int len = rf_hw_receive(buf, sizeof(buf), &rssi);
    
    if (len <= 0) {
        return 0;
    }
    
    // 验证 CRC
    uint16_t recv_crc = buf[len-2] | (buf[len-1] << 8);
    uint16_t calc_crc = crc16_fast(buf, len - 2);
    
    if (recv_crc != calc_crc) {
        rx_ctx.crc_errors++;
        return -2;
    }
    
    // 解析 tracker_id
    uint8_t tracker_id = buf[0] & 0x3F;
    
    if (tracker_id >= RF_MAX_TRACKERS) {
        return -3;
    }
    
    // 验证 tracker 是否已配对
    tracker_info_t *tracker = &rx_ctx.trackers[tracker_id];
    if (!tracker->active) {
        return -4;
    }
    
    // 更新统计
    tracker->last_seen_ms = hal_millis();
    tracker->rssi = rssi;
    rx_ctx.rx_count++;
    
    return len;
}

/*============================================================================
 * API 函数 / API Functions
 *============================================================================*/

/**
 * @brief 初始化接收器
 */
int rf_receiver_init(void)
{
    memset(&rx_ctx, 0, sizeof(rx_ctx));
    memset(&pair_ctx, 0, sizeof(pair_ctx));
    
    // 获取 MAC 地址
    hal_get_mac_address(rx_ctx.mac_address);
    
    // 🔴 加载或生成 network_key
    int key_status = load_or_generate_network_key(&rx_ctx);
    if (key_status == 1) {
        // 新生成的 key，可能需要通知用户
    }
    
    // 🔴 加载已配对的追踪器
    int loaded = load_paired_trackers(&rx_ctx);
    
    // 初始化 RF 硬件
    rf_hw_init();
    rf_hw_set_channel(RF_DEFAULT_CHANNEL);
    rf_hw_rx_mode();
    
    rx_ctx.state = RX_STATE_NORMAL;
    
    return loaded;
}

/**
 * @brief 获取接收器状态
 */
void rf_receiver_get_status(uint8_t *active_count, uint32_t *rx_count, 
                            uint32_t *crc_errors)
{
    if (active_count) *active_count = rx_ctx.active_count;
    if (rx_count) *rx_count = rx_ctx.rx_count;
    if (crc_errors) *crc_errors = rx_ctx.crc_errors;
}

/**
 * @brief 获取 network_key (用于调试)
 */
uint32_t rf_receiver_get_network_key(void)
{
    return rx_ctx.network_key;
}

/**
 * @brief 清除所有配对信息
 */
int rf_receiver_clear_all_pairings(void)
{
    for (int i = 0; i < RF_MAX_TRACKERS; i++) {
        if (rx_ctx.trackers[i].active) {
            clear_tracker_pairing(i);
        }
    }
    rx_ctx.active_count = 0;
    return 0;
}

/**
 * @brief 获取追踪器信息
 */
bool rf_receiver_get_tracker(uint8_t id, uint8_t *mac, int8_t *rssi, 
                             uint32_t *last_seen)
{
    if (id >= RF_MAX_TRACKERS) return false;
    
    tracker_info_t *tracker = &rx_ctx.trackers[id];
    if (!tracker->active) return false;
    
    if (mac) memcpy(mac, tracker->mac_address, 6);
    if (rssi) *rssi = tracker->rssi;
    if (last_seen) *last_seen = tracker->last_seen_ms;
    
    return true;
}
