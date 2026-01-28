/**
 * @file main_tracker_v2.c
 * @brief SlimeVR RF Tracker - 完整实现版本
 * 
 * 功能:
 * - 200Hz 传感器采样和融合
 * - RF 数据发送 (重传 + RSSI)
 * - 配对流程
 * - 低功耗睡眠
 * - Bootloader 支持
 * 
 * RAM Usage: ~800 bytes
 * Flash Usage: ~60KB
 */

#include "config.h"
#include "hal.h"
#include "rf_protocol.h"
#include "rf_hw.h"
#include "vqf_ultra.h"
#include "imu_interface.h"
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

// 传感器
#define SENSOR_ODR_HZ           200
#define SENSOR_PERIOD_US        (1000000 / SENSOR_ODR_HZ)

// 按键
#define BTN_DEBOUNCE_MS         50
#define BTN_LONG_PRESS_MS       3000
#define BTN_DOUBLE_CLICK_MS     400

// LED
#define LED_BLINK_FAST_MS       100
#define LED_BLINK_SLOW_MS       500
#define LED_BLINK_PAIR_MS       200

// 配对
#define PAIR_TIMEOUT_MS         60000
#define PAIR_CHANNEL            2       // 固定配对信道

// 同步
#define SYNC_LOST_THRESHOLD     10
#define SYNC_SEARCH_TIMEOUT_MS  5000

// 低功耗
#define IDLE_TIMEOUT_MS         300000  // 5分钟无同步进入睡眠

/*============================================================================
 * 运行状态
 *============================================================================*/

typedef enum {
    STATE_INIT = 0,
    STATE_SEARCH_SYNC,      // 搜索同步信标
    STATE_RUNNING,          // 正常运行
    STATE_PAIRING,          // 配对中
    STATE_CALIBRATING,      // 校准中
    STATE_SLEEPING,         // 低功耗睡眠
    STATE_BOOTLOADER,       // Bootloader 模式
    STATE_ERROR             // 错误状态
} tracker_state_t;

/*============================================================================
 * 按键状态机
 *============================================================================*/

typedef struct {
    uint32_t press_time;
    uint32_t release_time;
    uint8_t state;          // 0=idle, 1=pressed, 2=wait_double, 3=long_hold
    uint8_t click_count;
    bool long_press_fired;
} button_state_t;

/*============================================================================
 * 全局状态
 *============================================================================*/

// 系统状态
static tracker_state_t state = STATE_INIT;
static uint32_t state_enter_time = 0;
static uint8_t error_code = 0;

// 传感器
static vqf_ultra_state_t vqf_state;
static float quaternion[4] = {1, 0, 0, 0};
static float accel_linear[3] = {0, 0, 0};
static float gyro_bias[3] = {0, 0, 0};
static uint32_t last_sensor_time_us = 0;

// RF
static uint8_t tracker_id = 0xFF;
static uint8_t network_key[4] = {0};
static uint8_t sequence_num = 0;
static uint8_t sync_lost_count = 0;
static uint32_t last_sync_time = 0;
static int8_t last_rssi = 0;
static uint8_t channel_map[5] = {0};

// 配对数据
static uint8_t mac_address[6];
static bool is_paired = false;

// 电池
static uint8_t battery_percent = 100;
static bool is_charging = false;

// 按键
static button_state_t btn_main;
static button_state_t btn_reset;

// LED
static uint32_t led_toggle_time = 0;
static bool led_state = false;

// 校准
static float calib_gyro_sum[3] = {0, 0, 0};
static uint16_t calib_sample_count = 0;
#define CALIB_SAMPLES           500     // 2.5秒 @ 200Hz

/*============================================================================
 * Flash 存储偏移
 *============================================================================*/

#define FLASH_PAIRING_ADDR      0x3F000     // 最后一个扇区
#define FLASH_MAGIC             0x534C494D  // "SLIM"

typedef struct {
    uint32_t magic;
    uint8_t tracker_id;
    uint8_t network_key[4];
    float gyro_bias[3];
    uint16_t crc;
} __attribute__((packed)) pairing_data_t;

/*============================================================================
 * 内部函数声明
 *============================================================================*/

static void enter_state(tracker_state_t new_state);
static void process_button(button_state_t *btn, uint8_t pin, 
                          bool *single, bool *double_click, bool *long_press);
static void update_led(void);
static void read_battery(void);
static void save_pairing_data(void);
static bool load_pairing_data(void);
static void build_tracker_packet(uint8_t *packet);
static void process_sync_beacon(const uint8_t *data, uint8_t len);
static void process_pairing_response(const uint8_t *data, uint8_t len);
static void start_calibration(void);
static void process_calibration(float gyro[3]);

/*============================================================================
 * 状态切换
 *============================================================================*/

static void enter_state(tracker_state_t new_state)
{
    tracker_state_t old_state = state;
    state = new_state;
    state_enter_time = hal_get_tick_ms();
    
    switch (new_state) {
        case STATE_SEARCH_SYNC:
            sync_lost_count = 0;
            rf_hw_set_mode(RF_MODE_RX);
            rf_hw_set_channel(channel_map[0]);
            break;
            
        case STATE_RUNNING:
            led_state = true;
            hal_gpio_write(PIN_LED, true);
            break;
            
        case STATE_PAIRING:
            rf_hw_set_channel(PAIR_CHANNEL);
            rf_hw_set_mode(RF_MODE_RX);
            break;
            
        case STATE_CALIBRATING:
            calib_gyro_sum[0] = calib_gyro_sum[1] = calib_gyro_sum[2] = 0;
            calib_sample_count = 0;
            break;
            
        case STATE_SLEEPING:
            hal_gpio_write(PIN_LED, false);
            imu_suspend();
            rf_hw_set_mode(RF_MODE_SLEEP);
            break;
            
        case STATE_BOOTLOADER:
            hal_gpio_write(PIN_LED, false);
            imu_suspend();
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
 * 传感器处理
 *============================================================================*/

static void sensor_task(void)
{
    uint32_t now_us = hal_get_tick_us();
    if ((now_us - last_sensor_time_us) < SENSOR_PERIOD_US) {
        return;
    }
    last_sensor_time_us = now_us;
    
    // 读取 IMU
    float gyro[3], accel[3];
    if (imu_read_all(gyro, accel) != 0) {
        return;
    }
    
    // 减去陀螺仪偏置
    gyro[0] -= gyro_bias[0];
    gyro[1] -= gyro_bias[1];
    gyro[2] -= gyro_bias[2];
    
    // 校准模式: 累积陀螺仪数据
    if (state == STATE_CALIBRATING) {
        process_calibration(gyro);
        return;
    }
    
    // 正常模式: 传感器融合
    vqf_ultra_update(&vqf_state, gyro, accel);
    vqf_ultra_get_quat(&vqf_state, quaternion);
    
    // 计算线性加速度 (移除重力)
    float gravity[3];
    float gv[3] = {0, 0, 1};
    
    // 使用四元数旋转重力向量
    float qw = quaternion[0], qx = quaternion[1];
    float qy = quaternion[2], qz = quaternion[3];
    
    gravity[0] = 2.0f * (qx*qz - qw*qy) * 1.0f;
    gravity[1] = 2.0f * (qy*qz + qw*qx) * 1.0f;
    gravity[2] = (qw*qw - qx*qx - qy*qy + qz*qz) * 1.0f;
    
    accel_linear[0] = accel[0] - gravity[0];
    accel_linear[1] = accel[1] - gravity[1];
    accel_linear[2] = accel[2] - gravity[2];
}

/*============================================================================
 * 校准处理
 *============================================================================*/

static void start_calibration(void)
{
    enter_state(STATE_CALIBRATING);
}

static void process_calibration(float gyro[3])
{
    // 累积 (加回偏置以获取原始值)
    calib_gyro_sum[0] += gyro[0] + gyro_bias[0];
    calib_gyro_sum[1] += gyro[1] + gyro_bias[1];
    calib_gyro_sum[2] += gyro[2] + gyro_bias[2];
    calib_sample_count++;
    
    // LED 快闪指示校准中
    if ((hal_get_tick_ms() - led_toggle_time) > 50) {
        led_toggle_time = hal_get_tick_ms();
        led_state = !led_state;
        hal_gpio_write(PIN_LED, led_state);
    }
    
    // 完成
    if (calib_sample_count >= CALIB_SAMPLES) {
        gyro_bias[0] = calib_gyro_sum[0] / CALIB_SAMPLES;
        gyro_bias[1] = calib_gyro_sum[1] / CALIB_SAMPLES;
        gyro_bias[2] = calib_gyro_sum[2] / CALIB_SAMPLES;
        
        // 重置 VQF
        vqf_ultra_reset(&vqf_state);
        
        // 保存校准数据
        save_pairing_data();
        
        // 返回运行状态
        enter_state(is_paired ? STATE_RUNNING : STATE_SEARCH_SYNC);
    }
}

/*============================================================================
 * RF 发送
 *============================================================================*/

static void build_tracker_packet(uint8_t *packet)
{
    // SlimeVR RF 16字节数据包格式
    // [0]     : Packet type (0x01 = tracker data)
    // [1]     : Tracker ID
    // [2]     : Sequence number
    // [3-4]   : Quat W (Q15)
    // [5-6]   : Quat X (Q15)
    // [7-8]   : Quat Y (Q15)
    // [9-10]  : Quat Z (Q15)
    // [11-12] : Accel X (mg)
    // [13]    : Battery %
    // [14]    : Status flags
    // [15]    : CRC8
    
    packet[0] = 0x01;   // Tracker data
    packet[1] = tracker_id;
    packet[2] = sequence_num++;
    
    // 四元数转 Q15 (乘以 32767)
    int16_t qw = (int16_t)(quaternion[0] * 32767.0f);
    int16_t qx = (int16_t)(quaternion[1] * 32767.0f);
    int16_t qy = (int16_t)(quaternion[2] * 32767.0f);
    int16_t qz = (int16_t)(quaternion[3] * 32767.0f);
    
    packet[3] = qw & 0xFF;
    packet[4] = (qw >> 8) & 0xFF;
    packet[5] = qx & 0xFF;
    packet[6] = (qx >> 8) & 0xFF;
    packet[7] = qy & 0xFF;
    packet[8] = (qy >> 8) & 0xFF;
    packet[9] = qz & 0xFF;
    packet[10] = (qz >> 8) & 0xFF;
    
    // 线性加速度 (mg)
    int16_t ax = (int16_t)(accel_linear[0] * 1000.0f);
    packet[11] = ax & 0xFF;
    packet[12] = (ax >> 8) & 0xFF;
    
    packet[13] = battery_percent;
    packet[14] = (is_charging ? 0x01 : 0x00);
    
    // CRC8
    uint8_t crc = 0;
    for (int i = 0; i < 15; i++) {
        crc ^= packet[i];
    }
    packet[15] = crc;
}

static void rf_transmit_task(void)
{
    if (state != STATE_RUNNING) return;
    
    // 构建数据包
    uint8_t packet[16];
    build_tracker_packet(packet);
    
    // 发送 (带重传)
    bool ack_received = false;
    for (int retry = 0; retry < 3 && !ack_received; retry++) {
        rf_hw_set_mode(RF_MODE_TX);
        if (rf_hw_transmit(packet, 16) == 0) {
            // 等待 ACK (最多 500us)
            rf_hw_set_mode(RF_MODE_RX);
            uint32_t start = hal_get_tick_us();
            while ((hal_get_tick_us() - start) < 500) {
                if (rf_hw_data_ready()) {
                    uint8_t ack[4];
                    int8_t rssi;
                    if (rf_hw_receive(ack, sizeof(ack), &rssi) > 0) {
                        if (ack[0] == 0xAC && ack[1] == tracker_id) {
                            ack_received = true;
                            last_rssi = rssi;
                            sync_lost_count = 0;
                            break;
                        }
                    }
                }
            }
        }
        
        if (!ack_received) {
            hal_delay_us(100 + (retry * 50));  // 退避
        }
    }
    
    if (!ack_received) {
        sync_lost_count++;
        if (sync_lost_count >= SYNC_LOST_THRESHOLD) {
            enter_state(STATE_SEARCH_SYNC);
        }
    }
}

/*============================================================================
 * 同步信标处理
 *============================================================================*/

static void sync_search_task(void)
{
    if (state != STATE_SEARCH_SYNC) return;
    
    // 检查超时
    if ((hal_get_tick_ms() - state_enter_time) > SYNC_SEARCH_TIMEOUT_MS) {
        // 进入睡眠
        enter_state(STATE_SLEEPING);
        return;
    }
    
    // 接收同步信标
    if (rf_hw_data_ready()) {
        uint8_t data[32];
        int8_t rssi;
        int len = rf_hw_receive(data, sizeof(data), &rssi);
        
        if (len > 0 && data[0] == 0xBE) {  // Beacon
            process_sync_beacon(data, len);
            last_sync_time = hal_get_tick_ms();
            enter_state(STATE_RUNNING);
        }
    }
    
    // LED 慢闪
    if ((hal_get_tick_ms() - led_toggle_time) > LED_BLINK_SLOW_MS) {
        led_toggle_time = hal_get_tick_ms();
        led_state = !led_state;
        hal_gpio_write(PIN_LED, led_state);
    }
}

static void process_sync_beacon(const uint8_t *data, uint8_t len)
{
    if (len < 10) return;
    
    // 解析信标
    // [0]    : 0xBE (Beacon marker)
    // [1-4]  : Network key
    // [5-9]  : Channel map (5 channels)
    
    memcpy(network_key, &data[1], 4);
    memcpy(channel_map, &data[5], 5);
}

/*============================================================================
 * 配对处理
 *============================================================================*/

static void pairing_task(void)
{
    if (state != STATE_PAIRING) return;
    
    // 检查超时
    if ((hal_get_tick_ms() - state_enter_time) > PAIR_TIMEOUT_MS) {
        enter_state(is_paired ? STATE_RUNNING : STATE_SEARCH_SYNC);
        return;
    }
    
    // LED 快闪
    if ((hal_get_tick_ms() - led_toggle_time) > LED_BLINK_PAIR_MS) {
        led_toggle_time = hal_get_tick_ms();
        led_state = !led_state;
        hal_gpio_write(PIN_LED, led_state);
    }
    
    // 发送配对请求
    static uint32_t last_pair_tx = 0;
    if ((hal_get_tick_ms() - last_pair_tx) > 500) {
        last_pair_tx = hal_get_tick_ms();
        
        uint8_t pair_req[12];
        pair_req[0] = 0xPA;  // Pair request
        memcpy(&pair_req[1], mac_address, 6);
        pair_req[7] = FIRMWARE_VERSION_MAJOR;
        pair_req[8] = FIRMWARE_VERSION_MINOR;
        pair_req[9] = FIRMWARE_VERSION_PATCH;
        pair_req[10] = imu_get_type();
        pair_req[11] = 0;   // Reserved
        
        rf_hw_set_mode(RF_MODE_TX);
        rf_hw_transmit(pair_req, 12);
        rf_hw_set_mode(RF_MODE_RX);
    }
    
    // 接收配对响应
    if (rf_hw_data_ready()) {
        uint8_t data[16];
        int8_t rssi;
        int len = rf_hw_receive(data, sizeof(data), &rssi);
        
        if (len > 0 && data[0] == 0xPR) {  // Pair response
            process_pairing_response(data, len);
        }
    }
}

static void process_pairing_response(const uint8_t *data, uint8_t len)
{
    if (len < 12) return;  // 需要: marker(1) + MAC(6) + id(1) + key(4) = 12
    
    // 检查 MAC 匹配
    if (memcmp(&data[1], mac_address, 6) != 0) return;
    
    // 解析配对数据
    tracker_id = data[7];
    memcpy(network_key, &data[8], 4);
    
    is_paired = true;
    save_pairing_data();
    
    enter_state(STATE_SEARCH_SYNC);
}

/*============================================================================
 * Flash 存储
 *============================================================================*/

static void save_pairing_data(void)
{
    pairing_data_t data;
    data.magic = FLASH_MAGIC;
    data.tracker_id = tracker_id;
    memcpy(data.network_key, network_key, 4);
    memcpy(data.gyro_bias, gyro_bias, sizeof(gyro_bias));
    
    // 计算 CRC
    uint16_t crc = 0;
    uint8_t *p = (uint8_t*)&data;
    for (int i = 0; i < sizeof(data) - 2; i++) {
        crc ^= p[i];
        crc = (crc << 1) | (crc >> 15);
    }
    data.crc = crc;
    
    // 写入 Flash
#ifdef CH59X
    FLASH_ROM_ERASE(FLASH_PAIRING_ADDR, 256);
    FLASH_ROM_WRITE(FLASH_PAIRING_ADDR, &data, sizeof(data));
#endif
}

static bool load_pairing_data(void)
{
    pairing_data_t data;
    
#ifdef CH59X
    FLASH_ROM_READ(FLASH_PAIRING_ADDR, &data, sizeof(data));
#endif
    
    if (data.magic != FLASH_MAGIC) return false;
    
    // 验证 CRC
    uint16_t crc = 0;
    uint8_t *p = (uint8_t*)&data;
    for (int i = 0; i < sizeof(data) - 2; i++) {
        crc ^= p[i];
        crc = (crc << 1) | (crc >> 15);
    }
    if (crc != data.crc) return false;
    
    tracker_id = data.tracker_id;
    memcpy(network_key, data.network_key, 4);
    memcpy(gyro_bias, data.gyro_bias, sizeof(gyro_bias));
    
    return true;
}

/*============================================================================
 * 按键处理
 *============================================================================*/

static void process_button(button_state_t *btn, uint8_t pin,
                          bool *single, bool *double_click, bool *long_press)
{
    uint32_t now = hal_get_tick_ms();
    bool pressed = (hal_gpio_read(pin) == 0);
    
    *single = *double_click = *long_press = false;
    
    switch (btn->state) {
        case 0: // IDLE
            if (pressed) {
                btn->state = 1;
                btn->press_time = now;
                btn->long_press_fired = false;
            }
            break;
            
        case 1: // PRESSED
            if (!pressed) {
                if ((now - btn->press_time) >= BTN_DEBOUNCE_MS && !btn->long_press_fired) {
                    btn->click_count = 1;
                    btn->release_time = now;
                    btn->state = 2;
                } else {
                    btn->state = 0;
                }
            } else if ((now - btn->press_time) >= BTN_LONG_PRESS_MS && !btn->long_press_fired) {
                btn->long_press_fired = true;
                *long_press = true;
                btn->state = 3;
            }
            break;
            
        case 2: // WAIT_DOUBLE
            if (pressed && (now - btn->release_time) <= BTN_DOUBLE_CLICK_MS) {
                btn->click_count = 2;
                btn->press_time = now;
                btn->state = 1;
            } else if ((now - btn->release_time) > BTN_DOUBLE_CLICK_MS) {
                *single = true;
                btn->state = 0;
                btn->click_count = 0;
            }
            break;
            
        case 3: // LONG_HOLD
            if (!pressed) {
                btn->state = 0;
                btn->click_count = 0;
            }
            break;
    }
    
    // 双击完成检测
    if (btn->state == 1 && !pressed && btn->click_count == 2) {
        if ((now - btn->press_time) >= BTN_DEBOUNCE_MS) {
            *double_click = true;
            btn->state = 0;
            btn->click_count = 0;
        }
    }
}

/*============================================================================
 * LED 更新
 *============================================================================*/

static void update_led(void)
{
    uint32_t now = hal_get_tick_ms();
    uint32_t period;
    
    switch (state) {
        case STATE_RUNNING:
            hal_gpio_write(PIN_LED, true);
            return;
            
        case STATE_SEARCH_SYNC:
            period = LED_BLINK_SLOW_MS;
            break;
            
        case STATE_PAIRING:
            period = LED_BLINK_PAIR_MS;
            break;
            
        case STATE_CALIBRATING:
            period = 50;
            break;
            
        case STATE_BOOTLOADER:
            period = LED_BLINK_FAST_MS;
            break;
            
        case STATE_ERROR:
            // 快速闪烁 error_code 次
            period = 100;
            break;
            
        default:
            hal_gpio_write(PIN_LED, false);
            return;
    }
    
    if ((now - led_toggle_time) > period) {
        led_toggle_time = now;
        led_state = !led_state;
        hal_gpio_write(PIN_LED, led_state);
    }
}

/*============================================================================
 * 电池检测
 *============================================================================*/

static void read_battery(void)
{
    static uint32_t last_read = 0;
    if ((hal_get_tick_ms() - last_read) < 1000) return;
    last_read = hal_get_tick_ms();
    
    // 读取 ADC
    uint16_t adc_val = 0;
#ifdef CH59X
    adc_val = ADC_ExcutSingleConver();
#endif
    
    // 转换为电压 (假设分压比 2:1, 参考电压 1.2V)
    float voltage = (adc_val / 4096.0f) * 1.2f * 2.0f;
    
    // 电量估算 (3.0V-4.2V 线性)
    if (voltage >= 4.2f) battery_percent = 100;
    else if (voltage <= 3.0f) battery_percent = 0;
    else battery_percent = (uint8_t)((voltage - 3.0f) / 1.2f * 100.0f);
    
    // 充电检测
    is_charging = (hal_gpio_read(PIN_CHRG_DET) == 0);
}

/*============================================================================
 * 睡眠处理
 *============================================================================*/

static void enter_sleep_mode(void)
{
    enter_state(STATE_SLEEPING);
    
#ifdef CH59X
    // 配置唤醒源 (按键)
    GPIOA_ModeCfg(GPIO_Pin_4, GPIO_ModeIN_PU);
    GPIOA_ITModeCfg(GPIO_Pin_4, GPIO_ITMode_FallEdge);
    PFIC_EnableIRQ(GPIO_A_IRQn);
    
    // 进入 Halt 模式
    LowPower_Halt(0);
    
    // 唤醒后重新初始化
    PFIC_DisableIRQ(GPIO_A_IRQn);
    hal_timer_init();
    imu_init();
    vqf_ultra_init(&vqf_state, SENSOR_ODR_HZ);
    
    enter_state(is_paired ? STATE_SEARCH_SYNC : STATE_INIT);
#endif
}

/*============================================================================
 * GPIO 中断处理
 *============================================================================*/

#ifdef CH59X
__attribute__((interrupt("WCH-Interrupt-fast")))
void GPIOA_IRQHandler(void)
{
    GPIOA_ClearITFlagBit(GPIO_Pin_4);
}
#endif

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
    hal_gpio_config(PIN_CHRG_DET, HAL_GPIO_INPUT_PULLUP);
    
    // 启动闪烁 (版本指示)
    for (int i = 0; i < 3; i++) {
        hal_gpio_write(PIN_LED, true);
        hal_delay_ms(100);
        hal_gpio_write(PIN_LED, false);
        hal_delay_ms(100);
    }
    
    // 获取 MAC 地址
#ifdef CH59X
    GetMACAddress(mac_address);
#endif
    
    // 初始化 Bootloader
    bootloader_init();
    
    // 初始化 RF
    if (rf_hw_init() != 0) {
        error_code = ERR_RF_INIT;
        enter_state(STATE_ERROR);
    }
    
    // 初始化 IMU
    if (imu_init() != 0) {
        error_code = ERR_IMU_NOT_FOUND;
        enter_state(STATE_ERROR);
    }
    
    // 初始化 VQF
    vqf_ultra_init(&vqf_state, SENSOR_ODR_HZ);
    
    // 加载配对数据
    is_paired = load_pairing_data();
    
    // 初始状态
    if (is_paired) {
        enter_state(STATE_SEARCH_SYNC);
    } else {
        enter_state(STATE_PAIRING);
    }
    
    // ADC 初始化 (电池监测)
#ifdef CH59X
    ADC_ExtSingleChSampInit(SampleFreq_3_2, ADC_PGA_0);
    ADC_ChannelCfg(PIN_VBAT_ADC_CHANNEL);
#endif
    
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
        
        // 睡眠模式
        if (state == STATE_SLEEPING) {
            // 已在 enter_sleep_mode() 中处理唤醒
            continue;
        }
        
        // 传感器任务
        sensor_task();
        
        // RF 任务
        if (state == STATE_RUNNING) {
            rf_transmit_task();
        } else if (state == STATE_SEARCH_SYNC) {
            sync_search_task();
        } else if (state == STATE_PAIRING) {
            pairing_task();
        }
        
        // 按键处理
        bool single1, double1, long1;
        bool single2, double2, long2;
        
        process_button(&btn_main, PIN_SW0, &single1, &double1, &long1);
        
        // 长按: 进入睡眠
        if (long1) {
            enter_sleep_mode();
        }
        
        // 双击: 进入配对 / 校准
        if (double1) {
            if (state == STATE_RUNNING) {
                start_calibration();
            } else if (state != STATE_PAIRING) {
                enter_state(STATE_PAIRING);
            }
        }
        
        // 单击: (保留)
        
        // LED 更新
        update_led();
        
        // 电池检测
        read_battery();
        
        // 短延时
        hal_delay_us(100);
    }
    
    return 0;
}
