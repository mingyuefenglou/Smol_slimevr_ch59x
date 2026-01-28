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
#define PAIR_CHANNEL            2       // 固定配对信道

// LED
#define LED_BLINK_FAST_MS       100
#define LED_BLINK_SLOW_MS       500

/*============================================================================
 * 追踪器数据结构
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
} tracker_info_t;

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

// 追踪器
static tracker_info_t trackers[MAX_TRACKERS];
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
 * Flash 存储
 *============================================================================*/

#define FLASH_CONFIG_ADDR       0x3E000     // 配置扇区
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
static void broadcast_beacon(void);
static void receive_tracker_data(void);
static void process_tracker_packet(const uint8_t *data, uint8_t len, int8_t rssi);
static void send_usb_report(void);
static void save_config(void);
static bool load_config(void);
static void process_pairing(void);
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

static void broadcast_beacon(void)
{
    // 5ms 周期
    if ((hal_get_tick_ms() - last_beacon_time) < SUPERFRAME_PERIOD_MS) {
        return;
    }
    last_beacon_time = hal_get_tick_ms();
    
    // 构建信标
    // [0]    : 0xBE (Beacon marker)
    // [1]    : Frame number
    // [2-3]  : Timestamp (低 16 位 us)
    // [4-7]  : Network key
    // [8-15] : Hop sequence (8 channels)
    // [16-18]: Tracker mask (24 bits)
    // [19]   : Reserved
    
    uint8_t beacon[20];
    uint32_t now_us = hal_get_tick_us();
    
    beacon[0] = 0xBE;
    beacon[1] = frame_number++;
    beacon[2] = now_us & 0xFF;
    beacon[3] = (now_us >> 8) & 0xFF;
    memcpy(&beacon[4], network_key, 4);
    
    // 生成跳频序列
    static uint8_t hop_seq[8];
    uint32_t seed = *(uint32_t*)network_key ^ frame_number;
    for (int i = 0; i < 8; i++) {
        seed = ((seed >> 1) ^ (-(seed & 1) & 0xB400u));
        hop_seq[i] = seed % 40;
    }
    memcpy(&beacon[8], hop_seq, 8);
    
    // 追踪器位图
    beacon[16] = tracker_mask & 0xFF;
    beacon[17] = (tracker_mask >> 8) & 0xFF;
    beacon[18] = (tracker_mask >> 16) & 0xFF;
    beacon[19] = 0;
    
    // 发送
    rf_hw_set_mode(RF_MODE_TX);
    rf_hw_transmit(beacon, 20);
    rf_hw_set_mode(RF_MODE_RX);
}

/*============================================================================
 * 追踪器数据接收
 *============================================================================*/

static void receive_tracker_data(void)
{
    if (!rf_hw_data_ready()) return;
    
    uint8_t data[32];
    int8_t rssi;
    int len = rf_hw_receive(data, sizeof(data), &rssi);
    
    if (len > 0) {
        process_tracker_packet(data, len, rssi);
    }
}

static void process_tracker_packet(const uint8_t *data, uint8_t len, int8_t rssi)
{
    // 检查包类型
    uint8_t pkt_type = data[0];
    
    // 配对请求
    if (pkt_type == 0xPA && state == STATE_PAIRING && len >= 12) {
        process_pairing_request(data, len);
        return;
    }
    
    // 追踪器数据包
    if (pkt_type == 0x01 && len >= 16) {
        uint8_t tracker_id = data[1];
        
        if (tracker_id >= MAX_TRACKERS) return;
        if (!trackers[tracker_id].paired) return;
        
        tracker_info_t *tr = &trackers[tracker_id];
        
        // 解析数据
        tr->quat[0] = data[3] | (data[4] << 8);
        tr->quat[1] = data[5] | (data[6] << 8);
        tr->quat[2] = data[7] | (data[8] << 8);
        tr->quat[3] = data[9] | (data[10] << 8);
        tr->accel[0] = data[11] | (data[12] << 8);
        tr->battery = data[13];
        tr->status = data[14];
        tr->rssi = rssi;
        
        tr->last_seen = hal_get_tick_ms();
        tr->packet_count++;
        tr->active = true;
        
        // 更新信道质量
        rf_channel_update(rf_hw_get_channel(), true, rssi);
        
        // 发送 ACK
        uint8_t ack[4];
        rf_build_ack_packet(ack, tracker_id, 0);  // 0 = OK
        rf_hw_set_mode(RF_MODE_TX);
        rf_hw_transmit(ack, 4);
        rf_hw_set_mode(RF_MODE_RX);
    }
}

/*============================================================================
 * 配对处理
 *============================================================================*/

static void process_pairing_request(const uint8_t *data, uint8_t len)
{
    // 解析配对请求
    // [0]    : 0xPA
    // [1-6]  : MAC 地址
    // [7-9]  : 固件版本
    // [10]   : IMU 类型
    // [11]   : Reserved
    
    uint8_t mac[6];
    memcpy(mac, &data[1], 6);
    
    // 查找或分配 tracker ID
    uint8_t tracker_id = find_or_create_tracker(mac);
    
    if (tracker_id == 0xFF) {
        // 已满，无法配对
        return;
    }
    
    // 标记为已配对
    tracker_info_t *tr = &trackers[tracker_id];
    tr->paired = true;
    memcpy(tr->mac, mac, 6);
    
    // 更新追踪器位图
    tracker_mask |= (1 << tracker_id);
    active_tracker_count++;
    
    // 保存配置
    save_config();
    
    // 发送配对响应
    // [0]    : 0xPR (Pair Response)
    // [1-6]  : MAC 地址
    // [7]    : 分配的 Tracker ID
    // [8-11] : Network Key
    
    uint8_t response[12];
    response[0] = 0xPR;
    memcpy(&response[1], mac, 6);
    response[7] = tracker_id;
    memcpy(&response[8], network_key, 4);
    
    rf_hw_set_mode(RF_MODE_TX);
    rf_hw_transmit(response, 12);
    rf_hw_set_mode(RF_MODE_RX);
    
    // 闪烁 LED 确认
    for (int i = 0; i < 3; i++) {
        hal_gpio_write(PIN_LED, true);
        hal_delay_ms(100);
        hal_gpio_write(PIN_LED, false);
        hal_delay_ms(100);
    }
}

static uint8_t find_or_create_tracker(const uint8_t *mac)
{
    // 查找已存在的
    for (int i = 0; i < MAX_TRACKERS; i++) {
        if (trackers[i].paired && 
            memcmp(trackers[i].mac, mac, 6) == 0) {
            return i;
        }
    }
    
    // 分配新的
    for (int i = 0; i < MAX_TRACKERS; i++) {
        if (!trackers[i].paired) {
            return i;
        }
    }
    
    return 0xFF;  // 已满
}

static void process_pairing(void)
{
    if (state != STATE_PAIRING) return;
    
    // 超时检查
    if ((hal_get_tick_ms() - pair_mode_start) > PAIR_MODE_TIMEOUT_MS) {
        enter_state(STATE_RUNNING);
        return;
    }
    
    // LED 快闪
    if ((hal_get_tick_ms() - led_toggle_time) > LED_BLINK_FAST_MS) {
        led_toggle_time = hal_get_tick_ms();
        led_state = !led_state;
        hal_gpio_write(PIN_LED, led_state);
    }
}

/*============================================================================
 * USB HID 报告发送
 *============================================================================*/

static void send_usb_report(void)
{
    static uint32_t last_report_time = 0;
    
    // 5ms 周期 (200Hz)
    if ((hal_get_tick_ms() - last_report_time) < 5) return;
    last_report_time = hal_get_tick_ms();
    
    if (!usb_hid_ready()) return;
    if (usb_hid_busy()) return;
    
    // 构建 USB HID 报告
    // 格式: 64 字节
    // [0]    : Report ID (0x01)
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
    usb_tx_buffer[0] = 0x01;  // Report ID
    
    uint8_t count = 0;
    uint8_t *ptr = &usb_tx_buffer[2];
    
    for (int i = 0; i < MAX_TRACKERS && count < 6; i++) {
        tracker_info_t *tr = &trackers[i];
        
        if (!tr->paired) continue;
        
        // 检查超时
        if ((hal_get_tick_ms() - tr->last_seen) > TRACKER_TIMEOUT_MS) {
            tr->active = false;
        }
        
        ptr[0] = i;
        ptr[1] = (tr->active ? 0x01 : 0x00) | (tr->status & 0x02);
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
    
#ifdef CH59X
    FLASH_ROM_ERASE(FLASH_CONFIG_ADDR, 256);
    FLASH_ROM_WRITE(FLASH_CONFIG_ADDR, &cfg, sizeof(cfg));
#endif
}

static bool load_config(void)
{
    receiver_config_t cfg;
    
#ifdef CH59X
    FLASH_ROM_READ(FLASH_CONFIG_ADDR, &cfg, sizeof(cfg));
#endif
    
    if (cfg.magic != FLASH_MAGIC) return false;
    
    // 验证 CRC
    uint16_t crc = 0;
    uint8_t *p = (uint8_t*)&cfg;
    for (int i = 0; i < sizeof(cfg) - 2; i++) {
        crc ^= p[i];
        crc = (crc << 1) | (crc >> 15);
    }
    if (crc != cfg.crc) return false;
    
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
        *(uint32_t*)network_key = R16_RNG_DAT_0 | (R16_RNG_DAT_0 << 16);
#else
        *(uint32_t*)network_key = 0x12345678;
#endif
        save_config();
    }
    
    // 初始化信道质量跟踪
    rf_channel_init();
    
    // 初始化 RF
    if (rf_hw_init() != 0) {
        error_code = ERR_RF_INIT;
        enter_state(STATE_ERROR);
    }
    
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
        
        // 配对模式
        if (state == STATE_PAIRING) {
            process_pairing();
            receive_tracker_data();  // 监听配对请求
        }
        
        // 运行模式
        if (state == STATE_RUNNING) {
            // 广播同步信标
            broadcast_beacon();
            
            // 接收追踪器数据
            receive_tracker_data();
            
            // 发送 USB 报告
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
