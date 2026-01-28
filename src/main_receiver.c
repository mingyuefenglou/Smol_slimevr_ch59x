/**
 * @file main_receiver_v2.c
 * @brief SlimeVR RF 接收器 - 完整实现版本
 * 
 * 功能:
 * - USB HID 设备 (与 SlimeVR Server 兼容)
 * - 多追踪器管理 (最多 24 个)
 * - RF 同步信标广播
 * - 配对流程
 * - 信道质量监控
 * - UF2 固件更新 (MSC 模式)
 * - v0.6.2: RF Ultra高效数据包支持
 * 
 * RAM 使用: ~2KB
 * Flash 使用: ~40KB
 */

#include "config.h"
#include "hal.h"
#include "rf_protocol.h"
#include "rf_protocol_enhanced.h"
#include "rf_hw.h"
#include "usb_hid_slime.h"
#include "usb_bootloader.h"
#include "version.h"
#include "error_codes.h"
#include "slime_packet.h"  // v0.4.25: nRF packet兼容层
#include "watchdog.h"      // v0.6.2: 看门狗和故障恢复

// v0.6.2: RF Ultra支持
#if defined(USE_RF_ULTRA) && USE_RF_ULTRA
#include "rf_ultra.h"
#endif

#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"
#endif

/*============================================================================
 * 配置常量
 *============================================================================*/

// 追踪器管理
#include "config.h"  // 使用统一的 MAX_TRACKERS 定义

// MAX_TRACKERS 在 config.h 中定义
#define TRACKER_TIMEOUT_MS      1000    // 1秒无数据认为离线

// TDMA 时序
#define SUPERFRAME_PERIOD_MS    5       // 5ms 超帧
#define BEACON_DURATION_US      250     // 信标持续时间
#define SLOT_DURATION_US        400     // 每个 slot 时长
#define GUARD_TIME_US           50      // 保护时间

// 配对
#define PAIR_MODE_TIMEOUT_MS    60000   // 配对模式超时
#define PAIR_CHANNEL            RF_PAIRING_CHANNEL   // 使用统一定义的配对信道

// LED
#define LED_BLINK_FAST_MS       100
#define LED_BLINK_SLOW_MS       500

/*============================================================================
 * 追踪器数据结构 - Receiver本地使用，避免与rf_protocol.h中的tracker_info_t冲突
 *============================================================================*/

typedef struct {
    bool active;
    bool paired;
    uint8_t mac[6];
    
    // 最新数据
    int16_t quat[4];            // Q15 格式
    int16_t accel[3];           // mg
    uint8_t battery;
    uint8_t status;
    int8_t rssi;
    
    // 统计
    uint32_t last_seen;
    uint32_t packet_count;
    uint8_t packet_loss;
    
    // v0.4.23: 详细统计信息
    uint32_t total_packets;     // 总接收包数
    uint32_t lost_packets;      // 丢失包数
    uint32_t retransmit_count;  // 重传次数
    uint32_t timeout_count;     // 超时次数
    uint32_t crc_error_count;   // CRC错误次数
    uint8_t loss_rate_pct;      // 丢包率百分比 (滑动平均)
    uint8_t last_sequence;      // 上一个序列号 (用于检测丢包)
} receiver_tracker_t;  // 重命名避免冲突

/*============================================================================
 * 运行状态
 *============================================================================*/

typedef enum {
    STATE_INIT = 0,
    STATE_RUNNING,
    STATE_PAIRING,
    STATE_BOOTLOADER,
    STATE_ERROR
} receiver_state_t;

/*============================================================================
 * 全局状态
 *============================================================================*/

// 系统状态
static receiver_state_t state = STATE_INIT;
static uint32_t state_enter_time = 0;
static uint8_t error_code = 0;

// RF 接收器模块上下文
static rf_receiver_ctx_t rf_ctx;

// 追踪器
static receiver_tracker_t trackers[MAX_TRACKERS];
static uint8_t active_tracker_count = 0;
static uint32_t tracker_mask = 0;       // 活跃追踪器位图

// RF
static uint8_t network_key[4];
static uint8_t frame_number = 0;
static uint32_t last_beacon_time = 0;

// 配对
static uint32_t pair_mode_start = 0;

// LED
static uint32_t led_toggle_time = 0;
static bool led_state = false;

// USB HID 发送缓冲区
static uint8_t usb_tx_buffer[64];

/*============================================================================
 * Flash 存储 - 使用 hal_storage API
 *============================================================================*/

// 使用hal_storage的相对地址 (CONFIG_STORAGE_ADDR = 0x0010)
#define STORAGE_CONFIG_OFFSET   0x0010
#define FLASH_MAGIC             0x52584E    // "NXR"

typedef struct {
    uint32_t magic;
    uint8_t network_key[4];
    uint8_t tracker_count;
    struct {
        uint8_t mac[6];
        uint8_t id;
    } paired_trackers[MAX_TRACKERS];
    uint16_t crc;
} __attribute__((packed)) receiver_config_t;

/*============================================================================
 * 内部函数声明
 *============================================================================*/

static void enter_state(receiver_state_t new_state);
static void process_button(void);
static void update_led(void);
static void send_usb_report(void);
static void send_status_packets(void);   // v0.4.25: packet3状态包
static void send_info_packets(void);     // v0.5.0: packet0设备信息
static void save_config(void);
static bool load_config(void);
static uint8_t find_or_create_tracker(const uint8_t *mac);

/*============================================================================
 * 状态切换
 *============================================================================*/

static void enter_state(receiver_state_t new_state)
{
    state = new_state;
    state_enter_time = hal_get_tick_ms();
    
    switch (new_state) {
        case STATE_RUNNING:
            led_state = true;
            hal_gpio_write(PIN_LED, true);
            rf_hw_set_mode(RF_MODE_RX);
            break;
            
        case STATE_PAIRING:
            pair_mode_start = hal_get_tick_ms();
            rf_hw_set_channel(PAIR_CHANNEL);
            rf_hw_set_mode(RF_MODE_RX);
            break;
            
        case STATE_BOOTLOADER:
            hal_gpio_write(PIN_LED, false);
            rf_hw_set_mode(RF_MODE_SLEEP);
            bootloader_enter_update_mode();
            break;
            
        case STATE_ERROR:
            rf_hw_set_mode(RF_MODE_SLEEP);
            break;
            
        default:
            break;
    }
}

/*============================================================================
 * 同步信标广播
 *============================================================================*/


static void send_usb_report(void)
{
    static uint32_t last_report_time = 0;
    static uint32_t status_packet_counter = 0;  // v0.4.25: 状态包计数
    static uint32_t info_packet_counter = 0;    // v0.5.0: 设备信息包计数
    
    // 5ms 周期 (200Hz)
    if ((hal_get_tick_ms() - last_report_time) < 5) return;
    last_report_time = hal_get_tick_ms();
    
    if (!usb_hid_ready()) return;
    if (usb_hid_busy()) return;
    
    // 构建 USB HID 报告
    // 格式: 64 字节
    // [0]    : Report ID (0x01=legacy, 0x10=packet1, 0x13=packet3, 0x10=packet0)
    // [1]    : Tracker count
    // [2-63] : Tracker data (每个追踪器 10 字节)
    //          [0]   : Tracker ID
    //          [1]   : Status (bit0=active, bit1=charging)
    //          [2-3] : Quat W (Q15 >> 1)
    //          [4-5] : Quat X (Q15 >> 1)
    //          [6-7] : Quat Y (Q15 >> 1)
    //          [8]   : Battery %
    //          [9]   : RSSI (offset 100)
    
    memset(usb_tx_buffer, 0, 64);
    
    // v0.4.25: 检查是否应该发送状态包 (每40帧约5Hz)
    status_packet_counter++;
    bool send_status = (status_packet_counter >= 40);
    if (send_status) status_packet_counter = 0;
    
    // v0.5.0: 检查是否应该发送设备信息包 (每200帧约1Hz)
    info_packet_counter++;
    bool send_info = (info_packet_counter >= 200);
    if (send_info) info_packet_counter = 0;
    
    // 发送数据包 (packet1格式或legacy格式)
    usb_tx_buffer[0] = 0x01;  // Report ID (legacy)
    
    uint8_t count = 0;
    uint8_t *ptr = &usb_tx_buffer[2];
    
    for (int i = 0; i < MAX_TRACKERS && count < 6; i++) {
        receiver_tracker_t *tr = &trackers[i];
        
        if (!tr->paired) continue;
        
        // 检查超时
        uint32_t now = hal_get_tick_ms();
        if ((now - tr->last_seen) > TRACKER_TIMEOUT_MS) {
            tr->active = false;
        }
        
        // v0.4.23: 更新丢包率统计 (滑动平均)
        if (tr->total_packets > 0) {
            uint32_t loss_pct = (tr->lost_packets * 100) / tr->total_packets;
            tr->loss_rate_pct = (uint8_t)((tr->loss_rate_pct * 7 + loss_pct) / 8);
        }
        
        ptr[0] = i;
        ptr[1] = (tr->active ? 0x01 : 0x00) | (tr->status & 0xFE);
        ptr[2] = (tr->quat[0] >> 1) & 0xFF;
        ptr[3] = (tr->quat[0] >> 9) & 0xFF;
        ptr[4] = (tr->quat[1] >> 1) & 0xFF;
        ptr[5] = (tr->quat[1] >> 9) & 0xFF;
        ptr[6] = (tr->quat[2] >> 1) & 0xFF;
        ptr[7] = (tr->quat[2] >> 9) & 0xFF;
        ptr[8] = tr->battery;
        ptr[9] = (uint8_t)(tr->rssi + 100);
        
        ptr += 10;
        count++;
    }
    
    usb_tx_buffer[1] = count;
    
    usb_hid_write(usb_tx_buffer, 64);
    
    // v0.4.25: 发送packet3状态包 (低频)
    if (send_status) {
        send_status_packets();
    }
    
    // v0.5.0: 发送packet0设备信息 (极低频)
    if (send_info) {
        send_info_packets();
    }
}

/**
 * @brief v0.4.25: 发送packet3状态包给所有活跃tracker
 */
static void send_status_packets(void)
{
    if (!usb_hid_ready() || usb_hid_busy()) return;
    
    for (int i = 0; i < MAX_TRACKERS; i++) {
        receiver_tracker_t *tr = &trackers[i];
        if (!tr->paired || !tr->active) continue;
        
        // 构建slime_tracker_data_t
        slime_tracker_data_t data = {
            .tracker_id = i,
            .battery_pct = tr->battery,
            .flags = tr->status,
            .rssi = tr->rssi
        };
        
        // 生成packet3
        uint8_t pkt[SLIME_PACKET_SIZE];
        slime_make_packet3(pkt, &data);
        
        // 发送 (使用report ID 0x13标识packet3)
        uint8_t report[17];
        report[0] = 0x13;  // packet3 report ID
        memcpy(&report[1], pkt, SLIME_PACKET_SIZE);
        
        // 等待HID可用
        uint32_t timeout = hal_get_tick_ms() + 10;
        while (usb_hid_busy() && hal_get_tick_ms() < timeout);
        
        if (!usb_hid_busy()) {
            usb_hid_write(report, 17);
        }
    }
}

/**
 * @brief v0.5.0: 发送packet0设备信息包
 */
static void send_info_packets(void)
{
    if (!usb_hid_ready() || usb_hid_busy()) return;
    
    for (int i = 0; i < MAX_TRACKERS; i++) {
        receiver_tracker_t *tr = &trackers[i];
        if (!tr->paired) continue;
        
        // 构建设备信息
        slime_device_info_t info = {
            .tracker_id = i,
            .fw_version_major = 0,
            .fw_version_minor = 5,  // v0.5.0
            .board_id = 0x59,       // CH59X
#ifdef CH591
            .mcu_id = 0x91,
#else
            .mcu_id = 0x92,
#endif
            .imu_id = 0x45,         // 假设ICM-45686
            .mag_id = 0,
            .battery_pct = tr->battery,
            .battery_mv = 0,        // 需要从tracker获取
            .imu_temp = 0
        };
        
        // 生成packet0
        uint8_t pkt[SLIME_PACKET_SIZE];
        slime_make_packet0(pkt, &info);
        
        // 发送
        uint8_t report[17];
        report[0] = 0x10;  // packet0 report ID
        memcpy(&report[1], pkt, SLIME_PACKET_SIZE);
        
        uint32_t timeout = hal_get_tick_ms() + 10;
        while (usb_hid_busy() && hal_get_tick_ms() < timeout);
        
        if (!usb_hid_busy()) {
            usb_hid_write(report, 17);
        }
    }
}

/*============================================================================
 * Flash 存储
 *============================================================================*/

static void save_config(void)
{
    receiver_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    
    cfg.magic = FLASH_MAGIC;
    memcpy(cfg.network_key, network_key, 4);
    
    cfg.tracker_count = 0;
    for (int i = 0; i < MAX_TRACKERS && cfg.tracker_count < MAX_TRACKERS; i++) {
        if (trackers[i].paired) {
            memcpy(cfg.paired_trackers[cfg.tracker_count].mac, 
                   trackers[i].mac, 6);
            cfg.paired_trackers[cfg.tracker_count].id = i;
            cfg.tracker_count++;
        }
    }
    
    // CRC
    uint16_t crc = 0;
    uint8_t *p = (uint8_t*)&cfg;
    for (int i = 0; i < sizeof(cfg) - 2; i++) {
        crc ^= p[i];
        crc = (crc << 1) | (crc >> 15);
    }
    cfg.crc = crc;
    
    // v0.6.2: 按照优化手册建议，添加错误返回值检查
#ifdef CH59X
    int ret = hal_storage_erase(STORAGE_CONFIG_OFFSET, 256);
    if (ret != 0) {
        LOG_ERR("Config erase failed: %d", ret);
        return;
    }
    
    ret = hal_storage_write(STORAGE_CONFIG_OFFSET, &cfg, sizeof(cfg));
    if (ret != 0) {
        LOG_ERR("Config write failed: %d", ret);
        return;
    }
    
    LOG_INFO("Config saved: %d trackers", cfg.tracker_count);
#endif
}

static bool load_config(void)
{
    receiver_config_t cfg;
    
#ifdef CH59X
    // v0.6.2: 按照优化手册建议，添加错误返回值检查
    int ret = hal_storage_read(STORAGE_CONFIG_OFFSET, &cfg, sizeof(cfg));
    if (ret != 0) {
        LOG_ERR("Config read failed: %d", ret);
        return false;
    }
#endif
    
    if (cfg.magic != FLASH_MAGIC) {
        LOG_INFO("No valid config (magic mismatch)");
        return false;
    }
    
    // 验证 CRC
    uint16_t crc = 0;
    uint8_t *p = (uint8_t*)&cfg;
    for (int i = 0; i < sizeof(cfg) - 2; i++) {
        crc ^= p[i];
        crc = (crc << 1) | (crc >> 15);
    }
    if (crc != cfg.crc) {
        LOG_WARN("Config CRC mismatch");
        return false;
    }
    
    // 数据有效性检查
    if (cfg.tracker_count > MAX_TRACKERS) {
        LOG_WARN("Invalid tracker_count: %d, capping to %d", cfg.tracker_count, MAX_TRACKERS);
        cfg.tracker_count = MAX_TRACKERS;
    }
    
    // 恢复配置
    memcpy(network_key, cfg.network_key, 4);
    
    for (int i = 0; i < cfg.tracker_count; i++) {
        uint8_t id = cfg.paired_trackers[i].id;
        if (id < MAX_TRACKERS) {
            memcpy(trackers[id].mac, cfg.paired_trackers[i].mac, 6);
            trackers[id].paired = true;
            tracker_mask |= (1 << id);
            active_tracker_count++;
        }
    }
    
    LOG_INFO("Config loaded: %d trackers", active_tracker_count);
    return true;
}

/*============================================================================
 * 按键处理
 *============================================================================*/

static void process_button(void)
{
    static uint32_t press_time = 0;
    static bool last_state = true;
    
    bool pressed = (hal_gpio_read(PIN_SW0) == 0);
    
    if (pressed && !last_state) {
        // 按下
        press_time = hal_get_tick_ms();
    }
    else if (!pressed && last_state) {
        // 释放
        uint32_t duration = hal_get_tick_ms() - press_time;
        
        if (duration > 3000) {
            // 长按: 清除所有配对
            memset(trackers, 0, sizeof(trackers));
            tracker_mask = 0;
            active_tracker_count = 0;
            save_config();
            
            // LED 闪烁确认
            for (int i = 0; i < 5; i++) {
                hal_gpio_write(PIN_LED, true);
                hal_delay_ms(100);
                hal_gpio_write(PIN_LED, false);
                hal_delay_ms(100);
            }
        }
        else if (duration > 50) {
            // 短按: 进入配对模式
            if (state == STATE_RUNNING) {
                enter_state(STATE_PAIRING);
            } else if (state == STATE_PAIRING) {
                enter_state(STATE_RUNNING);
            }
        }
    }
    
    last_state = pressed;
}

/*============================================================================
 * LED 更新
 *============================================================================*/

static void update_led(void)
{
    uint32_t now = hal_get_tick_ms();
    
    switch (state) {
        case STATE_RUNNING:
            // 有活跃追踪器时常亮，否则慢闪
            if (active_tracker_count > 0) {
                hal_gpio_write(PIN_LED, true);
            } else {
                if ((now - led_toggle_time) > LED_BLINK_SLOW_MS) {
                    led_toggle_time = now;
                    led_state = !led_state;
                    hal_gpio_write(PIN_LED, led_state);
                }
            }
            break;
            
        case STATE_PAIRING:
            if ((now - led_toggle_time) > LED_BLINK_FAST_MS) {
                led_toggle_time = now;
                led_state = !led_state;
                hal_gpio_write(PIN_LED, led_state);
            }
            break;
            
        case STATE_BOOTLOADER:
            if ((now - led_toggle_time) > 50) {
                led_toggle_time = now;
                led_state = !led_state;
                hal_gpio_write(PIN_LED, led_state);
            }
            break;
            
        case STATE_ERROR:
            // 快速闪烁
            if ((now - led_toggle_time) > 100) {
                led_toggle_time = now;
                led_state = !led_state;
                hal_gpio_write(PIN_LED, led_state);
            }
            break;
            
        default:
            hal_gpio_write(PIN_LED, false);
            break;
    }
}

/*============================================================================
 * USB 接收回调
 *============================================================================*/

static void usb_rx_callback(const uint8_t *data, uint8_t len)
{
    if (len < 1) return;
    
    uint8_t cmd = data[0];
    
    switch (cmd) {
        case 0x10:  // 进入 Bootloader
            enter_state(STATE_BOOTLOADER);
            break;
            
        case 0x11:  // 进入配对模式
            enter_state(STATE_PAIRING);
            break;
            
        case 0x12:  // 退出配对模式
            enter_state(STATE_RUNNING);
            break;
            
        case 0x20:  // 请求版本信息
            {
                uint8_t resp[16];
                resp[0] = 0x20;
                resp[1] = FIRMWARE_VERSION_MAJOR;
                resp[2] = FIRMWARE_VERSION_MINOR;
                resp[3] = FIRMWARE_VERSION_PATCH;
                resp[4] = SLIMEVR_PROTOCOL_VERSION;
                resp[5] = active_tracker_count;
                usb_hid_write(resp, 16);
            }
            break;
    }
}

/*============================================================================
 * 主函数
 *============================================================================*/

int main(void)
{
    // 系统时钟初始化
#ifdef CH59X
    SetSysClock(CLK_SOURCE_PLL_60MHz);
#endif
    
    // HAL 初始化
    hal_timer_init();
    hal_storage_init();
    
    // v0.6.2: 初始化看门狗 (2秒超时)
    // 必须在所有其他初始化之前，以便检测初始化死锁
    wdog_init(WDOG_TIMEOUT_MS);
    
    // v0.6.2: 检查并打印上次复位原因
    reset_reason_t reset_reason = wdog_get_reset_reason();
    if (reset_reason != RESET_REASON_POWER_ON) {
        LOG_WARN("Last reset: %s", wdog_reset_reason_str(reset_reason));
    }
    
    // GPIO 初始化
    hal_gpio_config(PIN_LED, HAL_GPIO_OUTPUT);
    hal_gpio_config(PIN_SW0, HAL_GPIO_INPUT_PULLUP);
    
    // 启动闪烁
    for (int i = 0; i < 3; i++) {
        hal_gpio_write(PIN_LED, true);
        hal_delay_ms(100);
        hal_gpio_write(PIN_LED, false);
        hal_delay_ms(100);
    }
    
    // 初始化 Bootloader
    bootloader_init();
    
    // 初始化追踪器数组
    memset(trackers, 0, sizeof(trackers));
    
    // 加载配置
    if (!load_config()) {
        // 生成随机网络密钥
#ifdef CH59X
        *(uint32_t*)network_key = hal_get_random_u32();
#else
        *(uint32_t*)network_key = 0x12345678;
#endif
        save_config();
    }
    
    // 初始化 RF 接收器模块 (会自动初始化rf_hw)
    memset(&rf_ctx, 0, sizeof(rf_ctx));
    rf_ctx.network_key = *(uint32_t*)network_key;
    if (rf_receiver_init(&rf_ctx) != 0) {
        error_code = ERR_RF_INIT;
        enter_state(STATE_ERROR);
    }
    
    // 初始化信道质量跟踪 (必须在RF接收器初始化之后)
    rf_channel_init();
    
    // 启动 RF 接收器
    rf_receiver_start(&rf_ctx);
    
    // 初始化 USB HID
    if (usb_hid_init() != 0) {
        error_code = ERR_USB_INIT;
        enter_state(STATE_ERROR);
    }
    usb_hid_set_rx_callback(usb_rx_callback);
    
    // 进入运行状态
    enter_state(STATE_RUNNING);
    
    // 主循环
    while (1) {
        // v0.6.2: 喂狗 (防止看门狗复位)
        wdog_feed();
        
        // 错误状态
        if (state == STATE_ERROR) {
            update_led();
            hal_delay_ms(100);
            continue;
        }
        
        // Bootloader 模式
        if (state == STATE_BOOTLOADER) {
            bootloader_process();
            update_led();
            hal_delay_ms(10);
            continue;
        }
        
        // 按键处理
        process_button();
        
        // RF 接收器处理 (统一处理同步信标、数据接收、配对)
        rf_receiver_process(&rf_ctx);
        
        // 同步本地追踪器状态 (从rf_ctx获取数据)
        for (int i = 0; i < RF_MAX_TRACKERS && i < MAX_TRACKERS; i++) {
            if (rf_ctx.trackers[i].active) {
                receiver_tracker_t *local = &trackers[i];
                tracker_info_t *remote = &rf_ctx.trackers[i];
                
                local->active = remote->connected;
                local->paired = true;  // 已在rf_ctx.trackers[i].active条件内，表示已配对
                if (remote->connected) {
                    // v0.4.23: 丢包检测 (基于序列号)
                    uint8_t expected_seq = local->last_sequence + 1;
                    uint8_t actual_seq = remote->last_sequence;
                    
                    if (local->total_packets > 0 && actual_seq != expected_seq) {
                        // 检测到丢包
                        uint8_t lost = (uint8_t)(actual_seq - expected_seq);
                        local->lost_packets += lost;
                    }
                    local->last_sequence = actual_seq;
                    local->total_packets++;
                    
                    // 复制四元数和加速度数据
                    // P0-5: 添加范围钳位防止溢出
                    float q0 = remote->quaternion[0];
                    float q1 = remote->quaternion[1];
                    float q2 = remote->quaternion[2];
                    float q3 = remote->quaternion[3];
                    if (q0 > 1.0f) q0 = 1.0f; else if (q0 < -1.0f) q0 = -1.0f;
                    if (q1 > 1.0f) q1 = 1.0f; else if (q1 < -1.0f) q1 = -1.0f;
                    if (q2 > 1.0f) q2 = 1.0f; else if (q2 < -1.0f) q2 = -1.0f;
                    if (q3 > 1.0f) q3 = 1.0f; else if (q3 < -1.0f) q3 = -1.0f;
                    local->quat[0] = (int16_t)(q0 * 32767.0f);
                    local->quat[1] = (int16_t)(q1 * 32767.0f);
                    local->quat[2] = (int16_t)(q2 * 32767.0f);
                    local->quat[3] = (int16_t)(q3 * 32767.0f);
                    // 加速度范围钳位防止int16_t溢出 (±32.767g范围)
                    float ax = remote->acceleration[0];
                    float ay = remote->acceleration[1];
                    float az = remote->acceleration[2];
                    if (ax > 32.767f) ax = 32.767f; else if (ax < -32.768f) ax = -32.768f;
                    if (ay > 32.767f) ay = 32.767f; else if (ay < -32.768f) ay = -32.768f;
                    if (az > 32.767f) az = 32.767f; else if (az < -32.768f) az = -32.768f;
                    local->accel[0] = (int16_t)(ax * 1000.0f);
                    local->accel[1] = (int16_t)(ay * 1000.0f);
                    local->accel[2] = (int16_t)(az * 1000.0f);
                    local->battery = remote->battery;
                    local->status = remote->flags;
                    local->rssi = remote->rssi;
                    local->last_seen = remote->last_seen_ms;
                }
            }
        }
        active_tracker_count = rf_ctx.paired_count;
        
        // 配对模式处理
        if (state == STATE_PAIRING) {
            static bool pairing_started = false;
            if (!pairing_started) {
                rf_receiver_start_pairing(&rf_ctx);
                pairing_started = true;
            }
            
            // 检查配对超时
            if ((hal_get_tick_ms() - pair_mode_start) > PAIR_MODE_TIMEOUT_MS) {
                rf_receiver_stop_pairing(&rf_ctx);
                pairing_started = false;
                enter_state(STATE_RUNNING);
            }
        }
        
        // 运行模式 - 发送 USB 报告
        if (state == STATE_RUNNING) {
            send_usb_report();
        }
        
        // USB HID 任务
        usb_hid_task();
        
        // LED 更新
        update_led();
        
        // 短延时
        hal_delay_us(100);
    }
    
    return 0;
}
