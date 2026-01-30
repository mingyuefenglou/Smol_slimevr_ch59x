/**
 * @file main_tracker.c
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

#include "board.h"
#include "hal.h"
#include "rf_protocol.h"
#include "rf_hw.h"

// v0.6.2: 根据FUSION_TYPE选择融合算法
#if FUSION_TYPE == FUSION_VQF_ULTRA
#include "vqf_ultra.h"
#elif FUSION_TYPE == FUSION_VQF_ADVANCED
#include "vqf_advanced.h"
#elif FUSION_TYPE == FUSION_VQF_OPT
#include "vqf_opt.h"
#elif FUSION_TYPE == FUSION_VQF_SIMPLE
#include "vqf_simple.h"
#elif FUSION_TYPE == FUSION_EKF
#include "ekf_ahrs.h"
#else
#include "vqf_advanced.h"  // 默认使用VQF Advanced
#endif

#include "imu_interface.h"
#include "usb_bootloader.h"
#include "version.h"
#include "error_codes.h"
#include "watchdog.h"      // v0.6.2: 看门狗和故障恢复
#include "event_logger.h"  // v0.6.2: 事件日志
#include "gyro_noise_filter.h"  // v0.6.2: 陀螺仪滤波和静止检测
#include "retained_state.h"     // v0.6.2: 睡眠状态保持
#include "auto_calibration.h"   // v0.6.2: 运动中自动校准
#include "temp_compensation.h"  // v0.6.2: 温度漂移补偿
#include "sensor_dma.h"         // v0.6.2: DMA传感器读取
#include "power_optimizer.h"    // v0.6.2: 功耗优化
#include "usb_debug.h"          // v0.6.2: USB调试接口
#include "diagnostics.h"        // v0.6.2: 诊断统计
#include "channel_manager.h"    // v0.6.2: 智能信道管理
#include "rf_recovery.h"        // v0.6.2: RF自愈机制
#include "rf_slot_optimizer.h"  // v0.6.2: 时隙优化
#include "rf_timing_opt.h"      // v0.6.2: 时序优化
#include "rf_ultra.h"           // v0.6.2: RF Ultra模式
#include "sensor_optimized.h"   // v0.6.2: 优化传感器读取
#include "usb_msc.h"            // v0.6.2: USB大容量存储
#include "mag_interface.h"      // v0.6.2: 磁力计支持
#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"
#include "CH59x_adc.h"
#include "CH59x_gpio.h"
#endif

#ifdef CH59X
static inline bool is_port_a_pin(uint8_t pin)
{
    return pin < 16;
}

static inline uint32_t gpio_pin_mask(uint8_t pin)
{
    return 1UL << (pin & 0x0F);
}

static void gpio_config_input(uint8_t pin, GPIOModeTypeDef mode)
{
    if (is_port_a_pin(pin)) {
        GPIOA_ModeCfg(gpio_pin_mask(pin), mode);
    } else {
        GPIOB_ModeCfg(gpio_pin_mask(pin), mode);
    }
}

static void gpio_config_interrupt(uint8_t pin, GPIOITModeTpDef mode)
{
    if (is_port_a_pin(pin)) {
        GPIOA_ITModeCfg(gpio_pin_mask(pin), mode);
        PFIC_EnableIRQ(GPIO_A_IRQn);
    } else {
        GPIOB_ITModeCfg(gpio_pin_mask(pin), mode);
        PFIC_EnableIRQ(GPIO_B_IRQn);
    }
}

static void gpio_disable_interrupt(uint8_t pin)
{
    if (is_port_a_pin(pin)) {
        PFIC_DisableIRQ(GPIO_A_IRQn);
    } else {
        PFIC_DisableIRQ(GPIO_B_IRQn);
    }
}
#endif

/*============================================================================
 * v0.6.2: 融合算法统一接口宏
 * 根据FUSION_TYPE自动选择正确的API
 *============================================================================*/

#if FUSION_TYPE == FUSION_VQF_ULTRA
#define FUSION_INIT(state, odr)         vqf_ultra_init(state, odr)
#define FUSION_UPDATE(state, g, a)      vqf_ultra_update(state, g, a)
#define FUSION_GET_QUAT(state, q)       vqf_ultra_get_quat(state, q)
#define FUSION_RESET(state)             vqf_ultra_reset(state)
#define FUSION_SET_QUAT(state, q)       vqf_ultra_set_quat(state, q)

#elif FUSION_TYPE == FUSION_VQF_ADVANCED
#define FUSION_INIT(state, odr)         vqf_advanced_init(state, 1.0f/(odr), 3.0f, 9.0f)
#define FUSION_UPDATE(state, g, a)      vqf_advanced_update(state, g, a)
#define FUSION_GET_QUAT(state, q)       vqf_advanced_get_quat(state, q)
#define FUSION_RESET(state)             vqf_advanced_reset(state)
#define FUSION_SET_QUAT(state, q)       do { (state)->quat[0]=(q)[0]; (state)->quat[1]=(q)[1]; \
                                             (state)->quat[2]=(q)[2]; (state)->quat[3]=(q)[3]; } while(0)

#elif FUSION_TYPE == FUSION_VQF_OPT
#define FUSION_INIT(state, odr)         vqf_opt_init(state, 1.0f/(odr))
#define FUSION_UPDATE(state, g, a)      vqf_opt_update(state, g, a)
#define FUSION_GET_QUAT(state, q)       vqf_opt_get_quat(state, q)
#define FUSION_RESET(state)             vqf_opt_reset(state)
#define FUSION_SET_QUAT(state, q)       vqf_opt_set_quat(state, q)

#elif FUSION_TYPE == FUSION_VQF_SIMPLE
#define FUSION_INIT(state, odr)         vqf_simple_init(state, 1.0f/(odr))
#define FUSION_UPDATE(state, g, a)      vqf_simple_update(state, g, a)
#define FUSION_GET_QUAT(state, q)       vqf_simple_get_quat(state, q)
#define FUSION_RESET(state)             vqf_simple_reset(state)
#define FUSION_SET_QUAT(state, q)       vqf_simple_set_quat(state, q)

#elif FUSION_TYPE == FUSION_EKF
#define FUSION_INIT(state, odr)         ekf_ahrs_init(state, 1.0f/(odr))
#define FUSION_UPDATE(state, g, a)      ekf_ahrs_update(state, g, a)
#define FUSION_GET_QUAT(state, q)       ekf_ahrs_get_quat(state, q)
#define FUSION_RESET(state)             ekf_ahrs_reset(state)
#define FUSION_SET_QUAT(state, q)       ekf_ahrs_set_quat(state, q)

#else
// 默认使用VQF Advanced
#define FUSION_INIT(state, odr)         vqf_advanced_init(state, 1.0f/(odr), 3.0f, 9.0f)
#define FUSION_UPDATE(state, g, a)      vqf_advanced_update(state, g, a)
#define FUSION_GET_QUAT(state, q)       vqf_advanced_get_quat(state, q)
#define FUSION_RESET(state)             vqf_advanced_reset(state)
#define FUSION_SET_QUAT(state, q)       do { (state)->quat[0]=(q)[0]; (state)->quat[1]=(q)[1]; \
                                             (state)->quat[2]=(q)[2]; (state)->quat[3]=(q)[3]; } while(0)
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
#define PAIR_CHANNEL            RF_PAIRING_CHANNEL   // 使用统一定义的配对信道

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

// 传感器 - v0.6.2: 根据FUSION_TYPE选择状态结构
#if FUSION_TYPE == FUSION_VQF_ULTRA
static vqf_ultra_state_t vqf_state;
#elif FUSION_TYPE == FUSION_VQF_ADVANCED
static vqf_state_t vqf_state;
#elif FUSION_TYPE == FUSION_VQF_OPT
static vqf_opt_state_t vqf_state;
#elif FUSION_TYPE == FUSION_VQF_SIMPLE
static vqf_simple_state_t vqf_state;
#elif FUSION_TYPE == FUSION_EKF
static ekf_ahrs_state_t vqf_state;
#else
static vqf_state_t vqf_state;  // 默认VQF Advanced
#endif

// 这些变量被usb_debug.c引用，不能是static
float quaternion[4] = {1, 0, 0, 0};
float accel_linear[3] = {0, 0, 0};
float gyro[3] = {0, 0, 0};  // 全局变量供usb_debug.c使用
float accel[3] = {0, 0, 0};  // 全局变量供usb_debug.c使用
static float gyro_bias[3] = {0, 0, 0};
static uint32_t last_sensor_time_us = 0;

// RF - 使用模块化 rf_transmitter
static rf_transmitter_ctx_t rf_ctx;
// tracker_id被usb_debug.c引用，不能是static
uint8_t tracker_id = 0xFF;
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
// 这些变量被usb_debug.c引用，不能是static
uint8_t battery_percent = 100;
bool is_charging = false;

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

// v0.6.2: 新增模块的全局状态
#if defined(USE_CHANNEL_MANAGER) && USE_CHANNEL_MANAGER
// 这些变量被rf_transmitter.c引用，不能是static
channel_manager_t ch_manager;
#endif

#if defined(USE_RF_RECOVERY) && USE_RF_RECOVERY
// 这些变量被rf_transmitter.c引用，不能是static
rf_recovery_state_t rf_recovery_state;
#endif

/*============================================================================
 * Flash 存储 - 使用 hal_storage API
 *============================================================================*/

// 使用hal_storage的相对地址 (PAIR_STORAGE_ADDR = 0x0200)
#define STORAGE_PAIRING_OFFSET  0x0200
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
    
    // 读取 IMU (使用全局变量供usb_debug.c访问)
    float temp = 25.0f;  // 默认温度
    
#if defined(USE_SENSOR_OPTIMIZED) && USE_SENSOR_OPTIMIZED
    // v0.6.2: 使用优化传感器读取 (FIFO+后台处理)
    uint32_t sample_ts;
    if (!sensor_optimized_get_sample(gyro, accel, &sample_ts)) {
        return;
    }
#elif defined(USE_SENSOR_DMA) && USE_SENSOR_DMA
    // v0.6.2: 使用DMA读取
    if (sensor_dma_data_ready()) {
        if (sensor_dma_get_data(gyro, accel, &temp) != 0) {
            return;
        }
    } else {
        return;
    }
#else
    // 标准读取 (使用全局变量)
    if (imu_read_all(gyro, accel) != 0) {
        return;
    }
#endif
    
    // v0.6.2: 更新温度补偿 (使用温度补偿模块内部的温度估计)
    temp = temp_comp_get_temp();  // 获取当前估计温度
    temp_comp_apply(gyro);  // 应用温度补偿到陀螺仪
    
    // v0.6.2: 更新陀螺仪滤波器 (用于静止检测)
    float gyro_filtered[3];
    gyro_filter_process(gyro, gyro_filtered, accel, temp);
    
    // v0.6.2: 更新自动校准 (运动中校准)
    auto_calib_update(gyro, accel);
    
    // 应用偏置补偿
    if (auto_calib_is_valid()) {
        // 使用自动校准的偏移
        float auto_offset[3];
        auto_calib_get_gyro_offset(auto_offset);
        gyro[0] -= auto_offset[0];
        gyro[1] -= auto_offset[1];
        gyro[2] -= auto_offset[2];
    } else {
        // 仅使用手动校准的偏置
        gyro[0] -= gyro_bias[0];
        gyro[1] -= gyro_bias[1];
        gyro[2] -= gyro_bias[2];
    }
    
    // v0.6.2: 读取磁力计 (用于航向校正)
#if defined(USE_MAGNETOMETER) && USE_MAGNETOMETER
    static float mag_data_f[3] = {0};
    static bool mag_available = false;
    
    if (mag_is_enabled()) {
        mag_data_t mag_data;
        if (mag_read(&mag_data) == 0) {
            // 转换为float数组
            mag_data_f[0] = mag_data.x;
            mag_data_f[1] = mag_data.y;
            mag_data_f[2] = mag_data.z;
            mag_available = true;
        }
    }
#endif
    
    // v0.6.2: 更新功耗优化 (根据运动状态调整时钟)
    float gyro_mag = gyro[0]*gyro[0] + gyro[1]*gyro[1] + gyro[2]*gyro[2];
    float accel_mag = accel[0]*accel[0] + accel[1]*accel[1] + accel[2]*accel[2];
    // 使用平方值避免sqrt运算
    power_update(gyro_mag, accel_mag);
    
    // 校准模式: 累积陀螺仪数据
    if (state == STATE_CALIBRATING) {
        process_calibration(gyro);
        return;
    }
    
    // 正常模式: 传感器融合
#if defined(USE_MAGNETOMETER) && USE_MAGNETOMETER && VQF_USE_MAGNETOMETER
    // 有磁力计数据时使用9DOF融合
    if (mag_available && mag_is_calibrated()) {
        vqf_advanced_update_mag(&vqf_state, gyro, accel, mag_data_f);
    } else {
        FUSION_UPDATE(&vqf_state, gyro, accel);
    }
#else
    FUSION_UPDATE(&vqf_state, gyro, accel);
#endif
    FUSION_GET_QUAT(&vqf_state, quaternion);
    
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
        
        // 重置融合算法
        FUSION_RESET(&vqf_state);
        
        // 保存校准数据
        save_pairing_data();
        
        // 返回运行状态
        enter_state(is_paired ? STATE_RUNNING : STATE_SEARCH_SYNC);
    }
}

/*============================================================================
 * RF 发送
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
    
    // v0.6.2: 按照优化手册建议，添加错误返回值检查
#ifdef CH59X
    int ret = hal_storage_erase(STORAGE_PAIRING_OFFSET, 256);
    if (ret != 0) {
        LOG_ERR("Storage erase failed: %d", ret);
        return;
    }
    
    ret = hal_storage_write(STORAGE_PAIRING_OFFSET, &data, sizeof(data));
    if (ret != 0) {
        LOG_ERR("Storage write failed: %d", ret);
        return;
    }
    
    LOG_INFO("Pairing data saved: tracker_id=%d", tracker_id);
#endif
}

static bool load_pairing_data(void)
{
    pairing_data_t data;
    
#ifdef CH59X
    // v0.6.2: 按照优化手册建议，添加错误返回值检查
    int ret = hal_storage_read(STORAGE_PAIRING_OFFSET, &data, sizeof(data));
    if (ret != 0) {
        LOG_ERR("Storage read failed: %d", ret);
        return false;  // 读取失败，返回未配对状态
    }
#endif
    
    if (data.magic != FLASH_MAGIC) {
        LOG_INFO("No valid pairing data (magic mismatch)");
        return false;
    }
    
    // 验证 CRC
    uint16_t crc = 0;
    uint8_t *p = (uint8_t*)&data;
    for (int i = 0; i < sizeof(data) - 2; i++) {
        crc ^= p[i];
        crc = (crc << 1) | (crc >> 15);
    }
    if (crc != data.crc) {
        LOG_WARN("Pairing data CRC mismatch");
        return false;
    }
    
    // 数据有效性检查
    if (data.tracker_id > MAX_TRACKERS) {
        LOG_WARN("Invalid tracker_id: %d, resetting", data.tracker_id);
        data.tracker_id = 0;
    }
    
    tracker_id = data.tracker_id;
    memcpy(network_key, data.network_key, 4);
    memcpy(gyro_bias, data.gyro_bias, sizeof(gyro_bias));
    
    LOG_INFO("Pairing data loaded: tracker_id=%d", tracker_id);
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
    
    // 转换为电压 (按分压电阻与参考电压计算)
    const float divider_ratio = (VBAT_DIVIDER_R1_OHMS + VBAT_DIVIDER_R2_OHMS) / VBAT_DIVIDER_R2_OHMS;
    float voltage = (adc_val / 4096.0f) * ADC_REF_VOLTAGE * divider_ratio;
    
    // 电量估算 (3.0V-4.2V 线性)
    // P0-6: 添加范围钳位防止截断
    if (voltage >= 4.2f) {
        battery_percent = 100;
    } else if (voltage <= 3.0f) {
        battery_percent = 0;
    } else {
        float percent = (voltage - 3.0f) / 1.2f * 100.0f;
        if (percent > 100.0f) percent = 100.0f;
        if (percent < 0.0f) percent = 0.0f;
        battery_percent = (uint8_t)percent;
    }
    
    // 充电检测
    is_charging = (hal_gpio_read(PIN_CHRG_DET) == 0);
    
    // v0.6.2: 更新功耗优化模块的电池状态
    power_optimizer_set_battery(battery_percent, is_charging);
}

/*============================================================================
 * 睡眠处理 (v0.4.24 WOM深睡眠唤醒闭环)
 *============================================================================*/

// v0.6.2: includes已移到文件顶部

// 静止检测计时器
static uint32_t stationary_start_time = 0;
static bool was_stationary = false;

/**
 * @brief 配置IMU WOM中断用于深睡眠唤醒
 */
static void configure_wom_wake(void)
{
#ifdef CH59X
    // 配置IMU的WOM (Wake on Motion)中断
    // 阈值约 200mg，足够检测抬手动作
    imu_enable_wom(200);  // 需要IMU驱动支持
#endif
}

/**
 * @brief 保存状态并进入深睡眠
 */
static void enter_deep_sleep(void)
{
    // v0.5.0: 保存当前状态用于快速恢复
    retained_save(quaternion, gyro_bias);
    retained_increment_sleep_count();
    
    enter_state(STATE_SLEEPING);
    
#ifdef CH59X
    // 配置WOM唤醒
    configure_wom_wake();
    
    // 配置IMU INT引脚为唤醒源
    gpio_config_input(PIN_IMU_INT1, GPIO_ModeIN_PD);  // 下拉，高电平唤醒
    gpio_config_interrupt(PIN_IMU_INT1, GPIO_ITMode_RiseEdge);
    
    // 同时配置按键作为备用唤醒源
    gpio_config_input(PIN_SW0, GPIO_ModeIN_PU);
    
    // 进入 Shutdown 模式 (最低功耗，复位唤醒)
    LowPower_Shutdown(0);
    
    // 不会执行到这里 - Shutdown会复位
#endif
}

/**
 * @brief 进入轻度睡眠 (未配对/未同步时使用)
 */
static void enter_light_sleep(void)
{
    enter_state(STATE_SLEEPING);
    
#ifdef CH59X
    // 配置唤醒源 (按键)
    gpio_config_input(PIN_SW0, GPIO_ModeIN_PU);
    gpio_config_interrupt(PIN_SW0, GPIO_ITMode_FallEdge);
    
    // 进入 Halt 模式 (可快速唤醒)
    LowPower_Halt(0);
    
    // 唤醒后重新初始化
    gpio_disable_interrupt(PIN_SW0);
    hal_timer_init();
    imu_init();
    FUSION_INIT(&vqf_state, SENSOR_ODR_HZ);
    
    enter_state(is_paired ? STATE_SEARCH_SYNC : STATE_INIT);
#endif
}

/**
 * @brief 处理睡眠模式选择
 * 
 * v0.4.24规则:
 * - 未配对/无beacon: 只允许light sleep (避免刚启动就睡死)
 * - 已配对且长时间静止: 可以deep sleep (WOM唤醒)
 */
static void enter_sleep_mode(void)
{
    if (!is_paired || sync_lost_count > SYNC_LOST_THRESHOLD) {
        // 未配对或失去同步: 使用轻度睡眠
        enter_light_sleep();
    } else {
        // 已配对且同步正常: 可以深睡眠
        enter_deep_sleep();
    }
}

/**
 * @brief 检查是否应该进入睡眠 (v0.4.24静止超时检测)
 */
static void check_sleep_condition(void)
{
    // 检查静止状态
    bool is_stationary = gyro_filter_is_stationary();
    uint32_t now = hal_get_tick_ms();
    
    if (is_stationary && !was_stationary) {
        // 刚进入静止状态
        stationary_start_time = now;
        was_stationary = true;
    } else if (!is_stationary) {
        // 运动中
        was_stationary = false;
        stationary_start_time = 0;
    }
    
    // 检查静止超时
    if (was_stationary && AUTO_SLEEP_TIMEOUT_MS > 0) {
        uint32_t stationary_duration = now - stationary_start_time;
        if (stationary_duration >= AUTO_SLEEP_TIMEOUT_MS) {
            // 静止足够长时间，进入睡眠
            enter_sleep_mode();
        }
    }
}

/**
 * @brief 从睡眠唤醒后的恢复处理 (v0.5.0)
 */
static void handle_wake_from_sleep(void)
{
    // 增加唤醒计数
    retained_increment_wake_count();
    
    // 尝试恢复保存的状态
    float saved_quat[4], saved_bias[3];
    int ret = retained_restore(saved_quat, saved_bias);
    
    if (ret >= 0) {
        // 恢复成功
        memcpy(quaternion, saved_quat, sizeof(quaternion));
        memcpy(gyro_bias, saved_bias, sizeof(gyro_bias));
        
        // 用保存的四元数初始化融合算法
        FUSION_SET_QUAT(&vqf_state, saved_quat);
    }
    
    // 重置静止检测
    was_stationary = false;
    stationary_start_time = 0;
}

/*============================================================================
 * GPIO 中断处理 - 使用hal_gpio.c中的weak定义和回调机制
 * 如需处理特定GPIO中断，使用 hal_gpio_set_interrupt() 设置回调
 *============================================================================*/

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
    
    // v0.6.2: 初始化事件日志 (必须在看门狗之前，用于记录启动事件)
    event_logger_init();
    
    // v0.6.2: 初始化看门狗 (2秒超时)
    // 必须在所有其他初始化之前，以便检测初始化死锁
    wdog_init(WDOG_TIMEOUT_MS);
    
    // v0.6.2: 检查并打印上次复位原因
    reset_reason_t reset_reason = wdog_get_reset_reason();
    if (reset_reason != RESET_REASON_POWER_ON) {
        LOG_WARN("Last reset: %s", wdog_reset_reason_str(reset_reason));
    }
    
    // v0.5.0: 初始化retained state模块
    retained_init();
    
    // v0.6.2: 初始化陀螺仪滤波器 (用于静止检测)
    // 必须在IMU初始化之前调用
    gyro_filter_config_t gyro_cfg = {
        .enable_median = true,
        .enable_moving_avg = true,
        .enable_kalman = false,           // 卡尔曼滤波可选
        .enable_rest_detection = true,    // 启用静止检测
        .enable_temp_comp = false,        // 温度补偿单独模块
        .process_noise = 0.001f,
        .measurement_noise = 0.01f
    };
    gyro_filter_init(&gyro_cfg);
    
    // v0.6.2: 初始化自动校准模块 (运动中校准陀螺仪偏移)
    auto_calib_init();
    
    // v0.6.2: 初始化温度补偿模块 (陀螺仪温漂补偿)
    temp_comp_init();
    
    // v0.6.2: 初始化功耗优化模块 (动态时钟/睡眠调度)
    power_optimizer_init();
    power_optimizer_set_auto_sleep(true, AUTO_SLEEP_TIMEOUT_MS);
    
    // v0.6.2: 初始化DMA传感器模块 (可选，提高传感器读取效率)
    // 注意: 如果使用DMA模式，需要确保IMU的INT1引脚已正确配置
    #if defined(USE_SENSOR_DMA) && USE_SENSOR_DMA
    sensor_dma_init();
    LOG_INFO("Sensor DMA enabled");
    #endif
    
    // v0.6.2: 初始化USB调试模块 (用于实时数据流和诊断命令)
    #if defined(USE_USB_DEBUG) && USE_USB_DEBUG
    usb_debug_init();
    LOG_INFO("USB Debug enabled");
    #endif
    
    // v0.6.2: 初始化诊断统计模块 (用于性能监控和故障排查)
    #if defined(USE_DIAGNOSTICS) && USE_DIAGNOSTICS
    diag_init();
    LOG_INFO("Diagnostics enabled");
    #endif
    
    // v0.6.2: 初始化智能信道管理模块 (CCA检测+自动避让)
    #if defined(USE_CHANNEL_MANAGER) && USE_CHANNEL_MANAGER
    ch_mgr_init(&ch_manager);
    LOG_INFO("Channel Manager enabled");
    #endif
    
    // v0.6.2: 初始化RF自愈模块 (通信中断自动恢复)
    #if defined(USE_RF_RECOVERY) && USE_RF_RECOVERY
    rf_recovery_init(&rf_recovery_state);
    LOG_INFO("RF Recovery enabled");
    #endif
    
    // v0.6.2: 初始化时隙优化器 (动态分配+优先级调度)
    #if defined(USE_RF_SLOT_OPTIMIZER) && USE_RF_SLOT_OPTIMIZER
    slot_optimizer_init();
    LOG_INFO("Slot Optimizer enabled");
    #endif
    
    // v0.6.2: 初始化时序优化模块 (精确同步+延迟补偿)
    #if defined(USE_RF_TIMING_OPT) && USE_RF_TIMING_OPT
    rf_timing_init();
    LOG_INFO("RF Timing Opt enabled");
    #endif
    
    // v0.6.2: 初始化RF Ultra模块 (极限性能模式)
    #if defined(USE_RF_ULTRA) && USE_RF_ULTRA
    rf_ultra_tx_init(1);  // rate_divider=1 表示全速率
    LOG_INFO("RF Ultra enabled");
    #endif
    
    // v0.6.2: 初始化优化传感器读取模块 (FIFO+后台处理)
    #if defined(USE_SENSOR_OPTIMIZED) && USE_SENSOR_OPTIMIZED
    sensor_optimized_init();
    LOG_INFO("Sensor Optimized enabled");
    #endif
    
    // v0.6.2: 初始化USB大容量存储模块 (UF2拖放升级)
    #if defined(USE_USB_MSC) && USE_USB_MSC
    usb_msc_init();
    LOG_INFO("USB MSC enabled");
    #endif
    
    // v0.6.2: 初始化磁力计模块 (航向校正)
    #if defined(USE_MAGNETOMETER) && USE_MAGNETOMETER
    if (mag_init() == 0) {
        mag_enable();
        LOG_INFO("Magnetometer enabled");
    }
    #endif
    
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
    
    // 初始化 RF 发送器模块 (会自动初始化rf_hw)
    memset(&rf_ctx, 0, sizeof(rf_ctx));
    if (rf_transmitter_init(&rf_ctx) != 0) {
        error_code = ERR_RF_INIT;
        enter_state(STATE_ERROR);
    }
    
    // 初始化 IMU
    if (imu_init() != 0) {
        error_code = ERR_IMU_NOT_FOUND;
        enter_state(STATE_ERROR);
    }
    
    // 初始化融合算法 (v0.6.2: 默认VQF Advanced)
    FUSION_INIT(&vqf_state, SENSOR_ODR_HZ);
    
    // v0.5.0: 检查是否从睡眠唤醒，尝试恢复状态
    if (retained_is_valid()) {
        handle_wake_from_sleep();
    }
    
    // 加载配对数据
    is_paired = load_pairing_data();
    
    // 如果已配对，更新RF上下文
    if (is_paired) {
        rf_ctx.paired = true;
        rf_ctx.tracker_id = tracker_id;
        memcpy(&rf_ctx.network_key, network_key, 4);
        rf_transmitter_start(&rf_ctx);  // 开始搜索同步
        enter_state(STATE_SEARCH_SYNC);
    } else {
        enter_state(STATE_PAIRING);
    }
    
    // ADC 初始化 (电池监测)
#ifdef CH59X
    ADC_BatteryInit();  // 使用SDK提供的电池ADC初始化函数
#endif
    
    // 主循环
    while (1) {
        // v0.6.2: 喂狗 (防止看门狗复位)
        wdog_feed();
        CHECKPOINT(CP_MAIN_LOOP_START);
        
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
        CHECKPOINT(CP_MAIN_LOOP_IMU);
        sensor_task();
        CHECKPOINT(CP_MAIN_LOOP_FUSION);
        
        // 更新RF发送器的传感器数据
        // 先计算flags，再传入函数
        bool is_stationary = gyro_filter_is_stationary();  // v0.4.24
        uint8_t rf_flags = (is_charging ? RF_FLAG_CHARGING : 0) |
                           (battery_percent < 20 ? RF_FLAG_LOW_BATTERY : 0) |
                           (state == STATE_CALIBRATING ? RF_FLAG_CALIBRATING : 0) |
                           (is_stationary ? RF_FLAG_STATIONARY : 0);  // v0.4.24
        rf_transmitter_set_data(&rf_ctx, quaternion, accel_linear, battery_percent, rf_flags);
        
        // RF 任务 - 使用模块化处理
        CHECKPOINT(CP_MAIN_LOOP_RF);
        if (state == STATE_RUNNING || state == STATE_SEARCH_SYNC) {
            rf_transmitter_process(&rf_ctx);
            
            // 检查RF状态并同步本地状态
            if (rf_ctx.state == TX_STATE_RUNNING && state == STATE_SEARCH_SYNC) {
                enter_state(STATE_RUNNING);
            } else if (rf_ctx.state == TX_STATE_SEARCHING && state == STATE_RUNNING) {
                enter_state(STATE_SEARCH_SYNC);
            }
        } else if (state == STATE_PAIRING) {
            // 配对模式 - 使用rf_transmitter的配对功能
            static bool pairing_requested = false;
            if (!pairing_requested) {
                rf_transmitter_request_pairing(&rf_ctx);
                pairing_requested = true;
            }
            rf_transmitter_process(&rf_ctx);
            
            // 检查配对是否完成
            if (rf_ctx.paired) {
                tracker_id = rf_ctx.tracker_id;
                memcpy(network_key, &rf_ctx.network_key, 4);
                is_paired = true;
                save_pairing_data();
                pairing_requested = false;
                enter_state(STATE_RUNNING);
            }
        }
        
        // 按键处理
        bool single1, double1, long1;
        bool single2, double2, long2;
        
        process_button(&btn_main, PIN_SW0, &single1, &double1, &long1);
        
        // 长按: 进入睡眠
        if (long1) {
            rf_transmitter_sleep(&rf_ctx);
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
        
        // v0.4.24: 检查是否应该进入睡眠 (静止超时)
        if (state == STATE_RUNNING) {
            check_sleep_condition();
        }
        
        // v0.6.2: USB调试处理 (处理调试命令和数据流)
        #if defined(USE_USB_DEBUG) && USE_USB_DEBUG
        usb_debug_process();
        #endif
        
        // v0.6.2: 诊断统计周期性更新
        #if defined(USE_DIAGNOSTICS) && USE_DIAGNOSTICS
        diag_periodic_update();
        #endif
        
        // v0.6.2: 信道管理器周期性更新 (信道质量评估)
        #if defined(USE_CHANNEL_MANAGER) && USE_CHANNEL_MANAGER
        ch_mgr_periodic_update(&ch_manager);
        #endif
        
        // v0.6.2: USB大容量存储任务
        #if defined(USE_USB_MSC) && USE_USB_MSC
        usb_msc_task();
        #endif
        
        // v0.6.2: 主循环结束检查点
        CHECKPOINT(CP_MAIN_LOOP_END);
        
        // 短延时
        hal_delay_us(100);
    }
    
    return 0;
}
