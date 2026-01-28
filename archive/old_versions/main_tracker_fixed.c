/**
 * @file main_tracker.c
 * @brief SlimeVR CH59X Tracker Main Application (Fixed Version)
 * 
 * 修复版本:
 * - 修复休眠唤醒逻辑
 * - 修复 ADC 计算溢出
 * - 修复时间比较逻辑
 * - 添加 volatile 同步
 * - 增加错误检查
 * - 初始化所有变量
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
#define SENSOR_SAMPLE_INTERVAL_US   (1000000UL / SENSOR_SAMPLE_RATE_HZ)  // 5000us
#define RF_REPORT_RATE_HZ           200
#define RF_REPORT_INTERVAL_MS       5   // 预计算: 1000/200 = 5ms (避免运行时整数除法)
#define LED_BLINK_INTERVAL_MS       500
#define BATTERY_CHECK_INTERVAL_MS   10000
#define PAIRING_TIMEOUT_MS          30000
#define SYNC_LOST_TIMEOUT_MS        5000
#define SLEEP_TIMEOUT_MS            60000

// 时间溢出安全的比较宏 (适用于 uint32_t)
// 当 elapsed 在合理范围内 (<2^31) 时, 无符号减法自动处理溢出
#define TIME_ELAPSED_MS(now, last)  ((uint32_t)((now) - (last)))
#define TIME_ELAPSED_US(now, last)  ((uint32_t)((now) - (last)))

/*============================================================================
 * 追踪器状态机
 *============================================================================*/

typedef enum {
    TRACKER_STATE_INIT = 0,
    TRACKER_STATE_PAIRING,
    TRACKER_STATE_SYNCING,
    TRACKER_STATE_RUNNING,
    TRACKER_STATE_SLEEP,
    TRACKER_STATE_WAKEUP,       // 新增: 唤醒过渡状态
    TRACKER_STATE_ERROR
} tracker_state_t;

/*============================================================================
 * 全局变量 (volatile 用于中断共享)
 *============================================================================*/

// 状态
static volatile tracker_state_t tracker_state = TRACKER_STATE_INIT;
static volatile bool wakeup_pending = false;    // 中断唤醒标志
static uint32_t state_enter_time = 0;
static uint32_t last_sync_time = 0;             // 将在初始化时正确设置
static uint32_t last_activity_time = 0;

// 传感器
static vqf_ultra_state_t vqf_state;
static float gyro[3], accel[3];
static float quaternion[4] = {1.0f, 0.0f, 0.0f, 0.0f};

// RF
static rf_transmitter_ctx_t rf_ctx;
static uint8_t tracker_id = 0xFF;
static volatile bool rf_synced = false;         // 中断回调中修改

// 电池
static uint8_t battery_percent = 100;
static uint16_t battery_mv = 4200;
static bool is_charging = false;

// LED
static uint32_t last_led_toggle = 0;
static bool led_state = false;

// 按键
static volatile bool button_pressed = false;
static uint32_t button_press_time = 0;

// 错误计数
static uint8_t sensor_error_count = 0;
static uint8_t rf_error_count = 0;

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

static void led_update(uint32_t now)
{
    switch (tracker_state) {
        case TRACKER_STATE_INIT:
            // 快速闪烁
            if (TIME_ELAPSED_MS(now, last_led_toggle) > 100) {
                led_toggle();
                last_led_toggle = now;
            }
            break;
            
        case TRACKER_STATE_PAIRING:
            // 慢速闪烁
            if (TIME_ELAPSED_MS(now, last_led_toggle) > 500) {
                led_toggle();
                last_led_toggle = now;
            }
            break;
            
        case TRACKER_STATE_SYNCING:
            // 双闪模式
            {
                uint32_t phase = (now / 100) % 10;
                if (phase == 0 || phase == 2) led_on();
                else led_off();
            }
            break;
            
        case TRACKER_STATE_RUNNING:
            led_on();
            break;
            
        case TRACKER_STATE_SLEEP:
        case TRACKER_STATE_WAKEUP:
            led_off();
            break;
            
        case TRACKER_STATE_ERROR:
            // SOS 模式
            {
                uint32_t phase = (now / 200) % 6;
                if (phase < 3) {
                    if ((phase & 1) == 0) led_on();
                    else led_off();
                } else {
                    led_off();
                }
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
    bool btn = (GPIOB_ReadPortPin(GPIO_Pin_4) == 0);
    
    if (btn && !button_pressed) {
        button_pressed = true;
        button_press_time = now;
    } else if (!btn && button_pressed) {
        uint32_t press_duration = TIME_ELAPSED_MS(now, button_press_time);
        button_pressed = false;
        
        if (press_duration > 5000) {
            tracker_state = TRACKER_STATE_PAIRING;
            state_enter_time = now;
        } else if (press_duration > 1000) {
            tracker_state = TRACKER_STATE_SLEEP;
            state_enter_time = now;
        } else if (press_duration > 50) {
            // 短按: 可添加校准等功能
        }
    }
#endif
}

/*============================================================================
 * 电池监测 (修复溢出问题)
 *============================================================================*/

static void battery_update(void)
{
#ifdef CH59X
    ADC_ChannelCfg(11);  // PA7/AIN11
    uint16_t adc_raw = ADC_ExcutSingleConver();
    
    // 修复: 使用 uint32_t 避免中间计算溢出
    // 假设: 12位ADC (0-4095), 分压比 2:1, VREF = 1.05V
    // 计算: V = (adc_raw / 4096) * 1.05V * 2 * 1000 (mV)
    uint32_t calc = (uint32_t)adc_raw * 1050UL * 2UL;
    battery_mv = (uint16_t)(calc / 512UL);  // 512 近似于 4096/8
    
    // 限制范围 (防止异常值)
    if (battery_mv > 5000) battery_mv = 5000;
    
    // 转换为百分比
    if (battery_mv >= 4200) {
        battery_percent = 100;
    } else if (battery_mv <= 3300) {
        battery_percent = 0;
    } else {
        battery_percent = (uint8_t)(((uint32_t)(battery_mv - 3300) * 100UL) / 900UL);
    }
    
    // 充电检测
    is_charging = (GPIOA_ReadPortPin(GPIO_Pin_5) == 0);
#endif
}

/*============================================================================
 * 传感器融合
 *============================================================================*/

static err_t sensor_init(void)
{
    int ret = imu_init();
    if (ret != 0) {
        return ERR_IMU_NOT_FOUND;
    }
    
    vqf_ultra_init(&vqf_state, SENSOR_SAMPLE_RATE_HZ);
    sensor_error_count = 0;
    
    return ERR_OK;
}

static void sensor_read_and_fuse(void)
{
    // 检查数据就绪
    if (!imu_data_ready()) {
        return;
    }
    
    // 读取传感器数据 (单次读取)
    int ret = imu_read_all(gyro, accel);
    if (ret != 0) {
        sensor_error_count++;
        if (sensor_error_count > 10) {
            // 连续错误过多，尝试重新初始化
            sensor_init();
        }
        return;
    }
    sensor_error_count = 0;
    
    // 运行 VQF 融合
    vqf_ultra_update(&vqf_state, gyro, accel);
    vqf_ultra_get_quat(&vqf_state, quaternion);
}

/*============================================================================
 * RF 通信
 *============================================================================*/

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

static void on_rf_ack(uint8_t status, uint8_t rssi)
{
    if (status == 0) {
        last_activity_time = hal_get_tick_ms();
        rf_error_count = 0;
    } else {
        rf_error_count++;
    }
}

static err_t rf_init_tracker(void)
{
    if (!hal_storage_load_pairing(&rf_ctx)) {
        tracker_id = 0xFF;
        return ERR_RF_NOT_PAIRED;
    }
    
    tracker_id = rf_ctx.tracker_id;
    
    int ret = rf_transmitter_init(&rf_ctx, on_rf_sync, on_rf_ack);
    if (ret != 0) {
        return ERR_RF_INIT_FAIL;
    }
    
    rf_error_count = 0;
    return ERR_OK;
}

static void rf_send_data(void)
{
    if (!rf_synced || tracker_id == 0xFF) {
        return;
    }
    
    rf_ctx.quaternion[0] = quaternion[0];
    rf_ctx.quaternion[1] = quaternion[1];
    rf_ctx.quaternion[2] = quaternion[2];
    rf_ctx.quaternion[3] = quaternion[3];
    
    rf_ctx.acceleration[0] = accel[0];
    rf_ctx.acceleration[1] = accel[1];
    rf_ctx.acceleration[2] = accel[2];
    
    rf_ctx.battery = battery_percent;
    rf_ctx.flags = is_charging ? 0x01 : 0x00;
    
    int ret = rf_transmitter_send_data(&rf_ctx);
    if (ret != 0) {
        rf_error_count++;
    }
}

/*============================================================================
 * 休眠管理 (修复唤醒逻辑)
 *============================================================================*/

static void enter_sleep(void)
{
    led_off();
    imu_suspend();
    rf_transmitter_stop(&rf_ctx);
    
#ifdef CH59X
    // 清除唤醒标志
    wakeup_pending = false;
    
    // 配置唤醒源
    GPIOB_ModeCfg(GPIO_Pin_4, GPIO_ModeIN_PU);
    GPIOB_ITModeCfg(GPIO_Pin_4, GPIO_ITMode_FallEdge);
    PFIC_EnableIRQ(GPIO_B_IRQn);
    
    // 内存屏障确保标志已清除
    __asm volatile ("fence" ::: "memory");
    
    // 进入睡眠 (唤醒后从此处继续)
    LowPower_Sleep(RB_PWR_RAM24K | RB_PWR_RAM2K);
    
    // === 唤醒后从这里继续执行 ===
    
    // 禁用唤醒中断
    PFIC_DisableIRQ(GPIO_B_IRQn);
    
    // 设置唤醒标志 (即使中断已设置)
    wakeup_pending = true;
#endif
}

static void exit_sleep(void)
{
    // 重新初始化系统时钟 (可能被睡眠影响)
#ifdef CH59X
    SetSysClock(CLK_SOURCE_PLL_60MHz);
#endif
    
    // 重新初始化外设
    err_t err = sensor_init();
    if (err != ERR_OK) {
        tracker_state = TRACKER_STATE_ERROR;
        return;
    }
    
    err = rf_init_tracker();
    if (err != ERR_OK && err != ERR_RF_NOT_PAIRED) {
        tracker_state = TRACKER_STATE_ERROR;
        return;
    }
    
    // 进入同步状态
    uint32_t now = hal_get_tick_ms();
    tracker_state = TRACKER_STATE_SYNCING;
    state_enter_time = now;
    last_sync_time = now;       // 重置同步时间
    last_activity_time = now;   // 重置活动时间
}

/*============================================================================
 * 配对模式
 *============================================================================*/

static void handle_pairing(uint32_t now)
{
    if (TIME_ELAPSED_MS(now, state_enter_time) > PAIRING_TIMEOUT_MS) {
        if (tracker_id != 0xFF) {
            tracker_state = TRACKER_STATE_SYNCING;
        } else {
            tracker_state = TRACKER_STATE_ERROR;
        }
        return;
    }
    
    static uint32_t last_pair_request = 0;
    if (TIME_ELAPSED_MS(now, last_pair_request) > 100) {
        rf_transmitter_send_pair_request(&rf_ctx);
        last_pair_request = now;
    }
    
    if (rf_ctx.tracker_id != 0xFF && rf_ctx.tracker_id != tracker_id) {
        tracker_id = rf_ctx.tracker_id;
        hal_storage_save_pairing(&rf_ctx);
        tracker_state = TRACKER_STATE_SYNCING;
        state_enter_time = now;
        last_sync_time = now;
    }
}

/*============================================================================
 * 主状态机 (修复休眠逻辑)
 *============================================================================*/

static void state_machine_update(uint32_t now)
{
    switch (tracker_state) {
        case TRACKER_STATE_INIT:
            if (tracker_id != 0xFF) {
                tracker_state = TRACKER_STATE_SYNCING;
            } else {
                tracker_state = TRACKER_STATE_PAIRING;
            }
            state_enter_time = now;
            last_sync_time = now;       // 初始化同步时间
            break;
            
        case TRACKER_STATE_PAIRING:
            handle_pairing(now);
            break;
            
        case TRACKER_STATE_SYNCING:
            if (rf_synced) {
                tracker_state = TRACKER_STATE_RUNNING;
                state_enter_time = now;
            } else if (TIME_ELAPSED_MS(now, state_enter_time) > SYNC_LOST_TIMEOUT_MS) {
                rf_transmitter_resync(&rf_ctx);
                state_enter_time = now;
            }
            break;
            
        case TRACKER_STATE_RUNNING:
            // 检查同步丢失 (仅在 last_sync_time 有效时)
            if (!rf_synced && TIME_ELAPSED_MS(now, last_sync_time) > SYNC_LOST_TIMEOUT_MS) {
                tracker_state = TRACKER_STATE_SYNCING;
                state_enter_time = now;
            }
            
            // 检查休眠超时
            if (TIME_ELAPSED_MS(now, last_activity_time) > SLEEP_TIMEOUT_MS) {
                tracker_state = TRACKER_STATE_SLEEP;
                state_enter_time = now;
            }
            break;
            
        case TRACKER_STATE_SLEEP:
            // 进入睡眠 (阻塞直到唤醒)
            enter_sleep();
            // 唤醒后继续执行
            tracker_state = TRACKER_STATE_WAKEUP;
            break;
            
        case TRACKER_STATE_WAKEUP:
            // 唤醒过渡状态
            if (wakeup_pending) {
                wakeup_pending = false;
                exit_sleep();
            }
            break;
            
        case TRACKER_STATE_ERROR:
            // 等待按键重启或长按重置
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
    
    // ADC 初始化
#ifdef CH59X
    ADC_ExtSingleChSampInit(SampleFreq_3_2, ADC_PGA_0);
#endif
    
    // 获取初始时间
    uint32_t init_time = hal_get_tick_ms();
    
    // 传感器初始化
    err_t err = sensor_init();
    if (err != ERR_OK) {
        tracker_state = TRACKER_STATE_ERROR;
    }
    
    // RF 初始化
    if (tracker_state != TRACKER_STATE_ERROR) {
        err = rf_init_tracker();
        if (err == ERR_RF_NOT_PAIRED) {
            tracker_state = TRACKER_STATE_PAIRING;
        } else if (err != ERR_OK) {
            tracker_state = TRACKER_STATE_ERROR;
        }
    }
    
    // 初始状态设置
    if (tracker_state != TRACKER_STATE_ERROR && tracker_state != TRACKER_STATE_PAIRING) {
        tracker_state = TRACKER_STATE_SYNCING;
    }
    
    // 正确初始化所有时间变量
    state_enter_time = init_time;
    last_activity_time = init_time;
    last_sync_time = init_time;     // 修复: 初始化为当前时间
    
    // 电池初始检测
    battery_update();
    
    // 主循环变量
    uint32_t last_sensor_time_us = 0;
    uint32_t last_rf_time_ms = 0;
    uint32_t last_battery_time = 0;
    
    // 主循环
    while (1) {
        uint32_t now_ms = hal_get_tick_ms();
        
        // 按键检测
        button_check(now_ms);
        
        // LED 更新
        led_update(now_ms);
        
        // 状态机更新
        state_machine_update(now_ms);
        
        // 传感器读取和融合 (200Hz)
        if (tracker_state == TRACKER_STATE_RUNNING || tracker_state == TRACKER_STATE_SYNCING) {
            uint32_t now_us = hal_get_tick_us();
            // 时间溢出安全: 无符号减法在C中自动处理溢出
            if (TIME_ELAPSED_US(now_us, last_sensor_time_us) >= SENSOR_SAMPLE_INTERVAL_US) {
                sensor_read_and_fuse();
                last_sensor_time_us = now_us;
            }
        }
        
        // RF 发送 (使用预计算常量，避免运行时除法)
        if (tracker_state == TRACKER_STATE_RUNNING) {
            if (TIME_ELAPSED_MS(now_ms, last_rf_time_ms) >= RF_REPORT_INTERVAL_MS) {
                rf_send_data();
                last_rf_time_ms = now_ms;
            }
        }
        
        // RF 任务处理
        if (tracker_state != TRACKER_STATE_SLEEP && tracker_state != TRACKER_STATE_WAKEUP) {
            rf_transmitter_task(&rf_ctx);
        }
        
        // 电池检测
        if (TIME_ELAPSED_MS(now_ms, last_battery_time) >= BATTERY_CHECK_INTERVAL_MS) {
            battery_update();
            last_battery_time = now_ms;
        }
    }
    
    return 0;
}

/*============================================================================
 * 中断处理 (修复同步问题)
 *============================================================================*/

#ifdef CH59X
__attribute__((interrupt("WCH-Interrupt-fast")))
void GPIOB_IRQHandler(void)
{
    if (GPIOB_ReadITFlagPort(GPIO_Pin_4)) {
        GPIOB_ClearITFlagBit(GPIO_Pin_4);
        // 设置唤醒标志 (volatile 确保主循环可见)
        wakeup_pending = true;
    }
}
#endif
