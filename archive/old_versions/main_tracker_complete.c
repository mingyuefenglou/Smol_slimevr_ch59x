/**
 * @file main_tracker.c
 * @brief SlimeVR CH59X Tracker Main Application
 * 
 * 完整的追踪器主循环实现:
 * - 传感器读取
 * - VQF 传感器融合
 * - RF 数据发送
 * - 电源管理
 * - 状态指示
 */

#include "config.h"
#include "hal.h"
#include "imu_interface.h"
#include "vqf_ultra.h"
#include "rf_protocol.h"
#include "rf_transmitter.h"
#include "error_codes.h"
#include "version.h"
#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"
#endif

/*============================================================================
 * 配置常量
 *============================================================================*/

#define SENSOR_SAMPLE_RATE_HZ       200
#define SENSOR_SAMPLE_INTERVAL_US   (1000000 / SENSOR_SAMPLE_RATE_HZ)
#define RF_REPORT_RATE_HZ           200
#define LED_BLINK_INTERVAL_MS       500
#define BATTERY_CHECK_INTERVAL_MS   10000
#define PAIRING_TIMEOUT_MS          30000
#define SYNC_LOST_TIMEOUT_MS        5000
#define SLEEP_TIMEOUT_MS            60000

/*============================================================================
 * 追踪器状态机
 *============================================================================*/

typedef enum {
    TRACKER_STATE_INIT = 0,
    TRACKER_STATE_PAIRING,
    TRACKER_STATE_SYNCING,
    TRACKER_STATE_RUNNING,
    TRACKER_STATE_SLEEP,
    TRACKER_STATE_ERROR
} tracker_state_t;

/*============================================================================
 * 全局变量
 *============================================================================*/

// 状态
static tracker_state_t tracker_state = TRACKER_STATE_INIT;
static uint32_t state_enter_time = 0;
static uint32_t last_sync_time = 0;
static uint32_t last_activity_time = 0;

// 传感器
static vqf_ultra_state_t vqf_state;
static float gyro[3], accel[3];
static float quaternion[4] = {1.0f, 0.0f, 0.0f, 0.0f};
static int16_t accel_raw[3];

// RF
static rf_transmitter_ctx_t rf_ctx;
static uint8_t tracker_id = 0xFF;
static bool rf_synced = false;

// 电池
static uint8_t battery_percent = 100;
static uint16_t battery_mv = 4200;
static bool is_charging = false;

// LED
static uint32_t last_led_toggle = 0;
static bool led_state = false;

// 按键
static bool button_pressed = false;
static uint32_t button_press_time = 0;

/*============================================================================
 * LED 控制
 *============================================================================*/

static void led_on(void)
{
#ifdef CH59X
    GPIOA_SetBits(GPIO_Pin_9);
#endif
    led_state = true;
}

static void led_off(void)
{
#ifdef CH59X
    GPIOA_ResetBits(GPIO_Pin_9);
#endif
    led_state = false;
}

static void led_toggle(void)
{
    if (led_state) led_off();
    else led_on();
}

// LED 闪烁模式
static void led_update(uint32_t now)
{
    switch (tracker_state) {
        case TRACKER_STATE_INIT:
            // 快速闪烁
            if (now - last_led_toggle > 100) {
                led_toggle();
                last_led_toggle = now;
            }
            break;
            
        case TRACKER_STATE_PAIRING:
            // 慢速闪烁
            if (now - last_led_toggle > 500) {
                led_toggle();
                last_led_toggle = now;
            }
            break;
            
        case TRACKER_STATE_SYNCING:
            // 双闪
            {
                uint32_t phase = (now / 100) % 10;
                if (phase == 0 || phase == 2) led_on();
                else led_off();
            }
            break;
            
        case TRACKER_STATE_RUNNING:
            // 常亮或呼吸
            led_on();
            break;
            
        case TRACKER_STATE_SLEEP:
            led_off();
            break;
            
        case TRACKER_STATE_ERROR:
            // SOS 闪烁
            {
                uint32_t phase = (now / 200) % 6;
                if (phase < 3) led_toggle();
                else led_off();
            }
            break;
    }
}

/*============================================================================
 * 按键处理
 *============================================================================*/

static void button_check(uint32_t now)
{
#ifdef CH59X
    bool btn = (GPIOB_ReadPortPin(GPIO_Pin_4) == 0);  // 低电平有效
    
    if (btn && !button_pressed) {
        // 按下
        button_pressed = true;
        button_press_time = now;
    } else if (!btn && button_pressed) {
        // 释放
        uint32_t press_duration = now - button_press_time;
        button_pressed = false;
        
        if (press_duration > 5000) {
            // 长按 5 秒: 进入配对模式
            tracker_state = TRACKER_STATE_PAIRING;
            state_enter_time = now;
        } else if (press_duration > 1000) {
            // 长按 1 秒: 进入休眠
            tracker_state = TRACKER_STATE_SLEEP;
            state_enter_time = now;
        } else if (press_duration > 50) {
            // 短按: 触发校准或其他操作
            // 可以在这里添加校准逻辑
        }
    }
#endif
}

/*============================================================================
 * 电池监测
 *============================================================================*/

static void battery_update(void)
{
#ifdef CH59X
    // 读取 ADC
    ADC_ChannelCfg(11);  // PA7/AIN11
    uint16_t adc_raw = ADC_ExcutSingleConver();
    
    // 转换为电压 (假设分压比 2:1, VREF = 1.05V)
    battery_mv = (adc_raw * 1050 * 2) / 512;
    
    // 转换为百分比 (3.3V = 0%, 4.2V = 100%)
    if (battery_mv >= 4200) battery_percent = 100;
    else if (battery_mv <= 3300) battery_percent = 0;
    else battery_percent = (battery_mv - 3300) * 100 / 900;
    
    // 充电检测
    is_charging = (GPIOA_ReadPortPin(GPIO_Pin_5) == 0);
#endif
}

/*============================================================================
 * 传感器融合
 *============================================================================*/

static err_t sensor_init(void)
{
    // 初始化 IMU
    int ret = imu_init();
    if (ret != 0) {
        return ERR_IMU_NOT_FOUND;
    }
    
    // 初始化 VQF
    vqf_ultra_init(&vqf_state, SENSOR_SAMPLE_RATE_HZ);
    
    return ERR_OK;
}

static void sensor_read_and_fuse(void)
{
    // 检查数据就绪
    if (!imu_data_ready()) {
        return;
    }
    
    // 读取传感器数据
    if (imu_read_all(gyro, accel) != 0) {
        return;
    }
    
    // 运行 VQF 融合
    vqf_ultra_update(&vqf_state, gyro, accel);
    
    // 获取四元数
    vqf_ultra_get_quat(&vqf_state, quaternion);
    
    // 保存原始加速度 (用于 RF 发送)
    accel_raw[0] = (int16_t)(accel[0] * 1000.0f);
    accel_raw[1] = (int16_t)(accel[1] * 1000.0f);
    accel_raw[2] = (int16_t)(accel[2] * 1000.0f);
}

/*============================================================================
 * RF 通信
 *============================================================================*/

// 同步回调
static void on_rf_sync(bool synced)
{
    rf_synced = synced;
    if (synced) {
        last_sync_time = hal_get_tick_ms();
        if (tracker_state == TRACKER_STATE_SYNCING) {
            tracker_state = TRACKER_STATE_RUNNING;
        }
    }
}

// ACK 回调
static void on_rf_ack(uint8_t status, uint8_t rssi)
{
    last_activity_time = hal_get_tick_ms();
}

static err_t rf_init_tracker(void)
{
    // 加载配对信息
    if (!hal_storage_load_pairing(&rf_ctx)) {
        // 未配对
        tracker_id = 0xFF;
        return ERR_RF_NOT_PAIRED;
    }
    
    tracker_id = rf_ctx.tracker_id;
    
    // 初始化 RF 发送器
    rf_transmitter_init(&rf_ctx, on_rf_sync, on_rf_ack);
    
    return ERR_OK;
}

static void rf_send_data(void)
{
    if (!rf_synced || tracker_id == 0xFF) {
        return;
    }
    
    // 更新上下文数据
    rf_ctx.quaternion[0] = quaternion[0];
    rf_ctx.quaternion[1] = quaternion[1];
    rf_ctx.quaternion[2] = quaternion[2];
    rf_ctx.quaternion[3] = quaternion[3];
    
    rf_ctx.acceleration[0] = accel[0];
    rf_ctx.acceleration[1] = accel[1];
    rf_ctx.acceleration[2] = accel[2];
    
    rf_ctx.battery = battery_percent;
    rf_ctx.flags = is_charging ? 0x01 : 0x00;
    
    // 发送数据
    rf_transmitter_send_data(&rf_ctx);
}

/*============================================================================
 * 休眠管理
 *============================================================================*/

static void enter_sleep(void)
{
    // 关闭 LED
    led_off();
    
    // 关闭 IMU
    imu_suspend();
    
    // 关闭 RF
    rf_transmitter_stop(&rf_ctx);
    
#ifdef CH59X
    // 配置唤醒源 (按键)
    GPIOB_ModeCfg(GPIO_Pin_4, GPIO_ModeIN_PU);
    GPIOB_ITModeCfg(GPIO_Pin_4, GPIO_ITMode_FallEdge);
    PFIC_EnableIRQ(GPIO_B_IRQn);
    
    // 进入深度睡眠
    LowPower_Sleep(RB_PWR_RAM24K | RB_PWR_RAM2K);
#endif
}

static void exit_sleep(void)
{
#ifdef CH59X
    // 禁用唤醒中断
    PFIC_DisableIRQ(GPIO_B_IRQn);
#endif
    
    // 重新初始化
    sensor_init();
    rf_init_tracker();
    
    tracker_state = TRACKER_STATE_SYNCING;
    state_enter_time = hal_get_tick_ms();
}

/*============================================================================
 * 配对模式
 *============================================================================*/

static void handle_pairing(uint32_t now)
{
    // 检查超时
    if (now - state_enter_time > PAIRING_TIMEOUT_MS) {
        // 配对超时，退出
        if (tracker_id != 0xFF) {
            tracker_state = TRACKER_STATE_SYNCING;
        } else {
            tracker_state = TRACKER_STATE_ERROR;
        }
        return;
    }
    
    // 发送配对请求
    static uint32_t last_pair_request = 0;
    if (now - last_pair_request > 100) {
        rf_transmitter_send_pair_request(&rf_ctx);
        last_pair_request = now;
    }
    
    // 检查配对响应
    if (rf_ctx.tracker_id != 0xFF && rf_ctx.tracker_id != tracker_id) {
        tracker_id = rf_ctx.tracker_id;
        hal_storage_save_pairing(&rf_ctx);
        tracker_state = TRACKER_STATE_SYNCING;
        state_enter_time = now;
    }
}

/*============================================================================
 * 主状态机
 *============================================================================*/

static void state_machine_update(uint32_t now)
{
    switch (tracker_state) {
        case TRACKER_STATE_INIT:
            // 初始化完成后进入同步或配对
            if (tracker_id != 0xFF) {
                tracker_state = TRACKER_STATE_SYNCING;
            } else {
                tracker_state = TRACKER_STATE_PAIRING;
            }
            state_enter_time = now;
            break;
            
        case TRACKER_STATE_PAIRING:
            handle_pairing(now);
            break;
            
        case TRACKER_STATE_SYNCING:
            // 检查同步超时
            if (rf_synced) {
                tracker_state = TRACKER_STATE_RUNNING;
                state_enter_time = now;
            } else if (now - state_enter_time > SYNC_LOST_TIMEOUT_MS) {
                // 重新尝试同步
                rf_transmitter_resync(&rf_ctx);
                state_enter_time = now;
            }
            break;
            
        case TRACKER_STATE_RUNNING:
            // 检查同步丢失
            if (!rf_synced && now - last_sync_time > SYNC_LOST_TIMEOUT_MS) {
                tracker_state = TRACKER_STATE_SYNCING;
                state_enter_time = now;
            }
            
            // 检查休眠超时
            if (now - last_activity_time > SLEEP_TIMEOUT_MS) {
                tracker_state = TRACKER_STATE_SLEEP;
                state_enter_time = now;
            }
            break;
            
        case TRACKER_STATE_SLEEP:
            enter_sleep();
            // 唤醒后会回到这里
            exit_sleep();
            break;
            
        case TRACKER_STATE_ERROR:
            // 错误状态，等待按键重启
            break;
    }
}

/*============================================================================
 * 主函数
 *============================================================================*/

int main(void)
{
    // 系统初始化
    hal_init();
    
    // LED 初始化
#ifdef CH59X
    GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
#endif
    led_on();
    
    // 按键初始化
#ifdef CH59X
    GPIOB_ModeCfg(GPIO_Pin_4, GPIO_ModeIN_PU);
#endif
    
    // ADC 初始化 (电池检测)
#ifdef CH59X
    ADC_ExtSingleChSampInit(SampleFreq_3_2, ADC_PGA_0);
#endif
    
    // 传感器初始化
    err_t err = sensor_init();
    if (err != ERR_OK) {
        tracker_state = TRACKER_STATE_ERROR;
    }
    
    // RF 初始化
    err = rf_init_tracker();
    if (err == ERR_RF_NOT_PAIRED) {
        tracker_state = TRACKER_STATE_PAIRING;
    } else if (err != ERR_OK) {
        tracker_state = TRACKER_STATE_ERROR;
    }
    
    // 初始状态
    if (tracker_state != TRACKER_STATE_ERROR && tracker_state != TRACKER_STATE_PAIRING) {
        tracker_state = TRACKER_STATE_SYNCING;
    }
    state_enter_time = hal_get_tick_ms();
    last_activity_time = state_enter_time;
    
    // 电池初始检测
    battery_update();
    
    // 主循环变量
    uint32_t last_sensor_time = 0;
    uint32_t last_rf_time = 0;
    uint32_t last_battery_time = 0;
    
    // 主循环
    while (1) {
        uint32_t now = hal_get_tick_ms();
        
        // 按键检测
        button_check(now);
        
        // LED 更新
        led_update(now);
        
        // 状态机更新
        state_machine_update(now);
        
        // 传感器读取和融合 (200Hz)
        if (tracker_state == TRACKER_STATE_RUNNING || tracker_state == TRACKER_STATE_SYNCING) {
            uint32_t now_us = hal_get_tick_us();
            if (now_us - last_sensor_time >= SENSOR_SAMPLE_INTERVAL_US) {
                sensor_read_and_fuse();
                last_sensor_time = now_us;
            }
        }
        
        // RF 发送 (200Hz)
        if (tracker_state == TRACKER_STATE_RUNNING) {
            if (now - last_rf_time >= (1000 / RF_REPORT_RATE_HZ)) {
                rf_send_data();
                last_rf_time = now;
            }
        }
        
        // RF 任务处理
        rf_transmitter_task(&rf_ctx);
        
        // 电池检测 (10秒一次)
        if (now - last_battery_time >= BATTERY_CHECK_INTERVAL_MS) {
            battery_update();
            last_battery_time = now;
        }
    }
    
    return 0;
}

/*============================================================================
 * 中断处理
 *============================================================================*/

#ifdef CH59X
// GPIO B 中断 (用于休眠唤醒)
__attribute__((interrupt("WCH-Interrupt-fast")))
void GPIOB_IRQHandler(void)
{
    if (GPIOB_ReadITFlagPort(GPIO_Pin_4)) {
        GPIOB_ClearITFlagBit(GPIO_Pin_4);
        // 唤醒处理在主循环中
    }
}
#endif
