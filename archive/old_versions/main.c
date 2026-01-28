/**
 * @file main.c
 * @brief SlimeVR Tracker Main Application for CH59X
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "hal.h"
#include "bmi270.h"
#include "icm42688.h"
#include "vqf_simple.h"
#include "ble_slimevr.h"

/*============================================================================
 * Configuration
 *============================================================================*/

#define SENSOR_UPDATE_RATE_HZ   200
#define SENSOR_UPDATE_MS        (1000 / SENSOR_UPDATE_RATE_HZ)
#define BLE_SEND_RATE_HZ        50
#define BLE_SEND_INTERVAL_MS    (1000 / BLE_SEND_RATE_HZ)

#define LED_PIN                 9
#define IMU_INT_PIN             8
#define IMU_SPI_CS_PIN          15
#define USE_SENSOR_INTERRUPT    1

/*============================================================================
 * Types
 *============================================================================*/

typedef enum { LED_OFF, LED_ON, LED_BLINK_SLOW, LED_BLINK_FAST, LED_PULSE } led_pattern_t;
typedef enum { IMU_NONE = 0, IMU_BMI270, IMU_ICM42688 } imu_type_t;

typedef struct {
    float quaternion[4];
    float linear_accel[3];
    imu_type_t imu_type;
    float temperature;
    uint8_t battery_level;
    bool charging;
    uint16_t battery_mv;
    bool connected;
    bool imu_ok;
    uint8_t error_code;
    bool calibrating;
    float gyro_bias[3];
} tracker_state_t;

/*============================================================================
 * Static Variables
 *============================================================================*/

static tracker_state_t tracker = {
    .quaternion = {1.0f, 0.0f, 0.0f, 0.0f},
    .imu_type = IMU_NONE,
    .battery_level = 100,
    .battery_mv = 4200,
};

static led_pattern_t current_led_pattern = LED_OFF;
static volatile bool sensor_data_ready_flag = false;
static float calib_gyro_sum[3];
static uint16_t calib_sample_count;
#define CALIB_SAMPLES 500

/*============================================================================
 * Forward Declarations
 *============================================================================*/

static void system_init(void);
static int sensor_init(void);
static void sensor_task(void);
static void ble_task(void);
static void led_task(void);
static void battery_task(void);
static void calibration_task(void);
static void sensor_interrupt_handler(void);
static void compute_linear_accel(const float q[4], const float accel[3], float lin_a[3]);
static void on_ble_connect(void);
static void on_ble_disconnect(void);
static void on_ble_command(uint8_t cmd, const uint8_t *data, uint8_t len);

/*============================================================================
 * Main
 *============================================================================*/

int main(void)
{
    system_init();
    
    hal_gpio_config(LED_PIN, HAL_GPIO_OUTPUT);
    hal_gpio_write(LED_PIN, true);
    hal_delay_ms(100);
    hal_gpio_write(LED_PIN, false);
    
    if (sensor_init() == 0) {
        tracker.imu_ok = true;
        current_led_pattern = LED_BLINK_SLOW;
    } else {
        tracker.imu_ok = false;
        tracker.error_code = 1;
        current_led_pattern = LED_BLINK_FAST;
    }
    
    vqf_init(1.0f / SENSOR_UPDATE_RATE_HZ, 3.0f, 9.0f);
    
    ble_slimevr_init();
    ble_slimevr_set_connect_callback(on_ble_connect);
    ble_slimevr_set_disconnect_callback(on_ble_disconnect);
    ble_slimevr_set_command_callback(on_ble_command);
    ble_slimevr_start_advertising();
    
#if USE_SENSOR_INTERRUPT
    hal_gpio_set_interrupt(IMU_INT_PIN, HAL_GPIO_INT_RISING, sensor_interrupt_handler);
#endif
    
    uint32_t last_sensor_time = 0, last_ble_time = 0, last_led_time = 0, last_battery_time = 0;
    
    while (1) {
        uint32_t now = hal_millis();
        ble_slimevr_process();
        
#if USE_SENSOR_INTERRUPT
        if (sensor_data_ready_flag) {
            sensor_data_ready_flag = false;
            if (tracker.imu_ok && !tracker.calibrating) sensor_task();
        }
#else
        if (now - last_sensor_time >= SENSOR_UPDATE_MS) {
            last_sensor_time = now;
            if (tracker.imu_ok && !tracker.calibrating) sensor_task();
        }
#endif
        
        if (now - last_ble_time >= BLE_SEND_INTERVAL_MS) { last_ble_time = now; ble_task(); }
        if (tracker.calibrating) calibration_task();
        if (now - last_led_time >= 100) { last_led_time = now; led_task(); }
        if (now - last_battery_time >= 10000) { last_battery_time = now; battery_task(); }
        
        hal_delay_ms(1);
    }
    return 0;
}

/*============================================================================
 * Implementation
 *============================================================================*/

static void system_init(void)
{
    hal_timer_init();
    hal_i2c_config_t i2c_cfg = { .addr = 0, .speed_hz = 400000 };
    hal_i2c_init(&i2c_cfg);
    hal_spi_config_t spi_cfg = { .speed_hz = 10000000, .mode = 3, .cs_pin = IMU_SPI_CS_PIN };
    hal_spi_init(&spi_cfg);
}

static int sensor_init(void)
{
    if (bmi270_detect(0x68) == 0 || bmi270_detect(0x69) == 0) {
        if (bmi270_init() == 0) {
            bmi270_set_odr(SENSOR_UPDATE_RATE_HZ, SENSOR_UPDATE_RATE_HZ);
            tracker.imu_type = IMU_BMI270;
            return 0;
        }
    }
    if (icm42688_detect_i2c(0x68) == 0 || icm42688_detect_i2c(0x69) == 0 || icm42688_detect_spi() == 0) {
        if (icm42688_init() == 0) {
            icm42688_set_odr(SENSOR_UPDATE_RATE_HZ, SENSOR_UPDATE_RATE_HZ);
            tracker.imu_type = IMU_ICM42688;
            return 0;
        }
    }
    return -1;
}

static void sensor_interrupt_handler(void) { sensor_data_ready_flag = true; }

static void sensor_task(void)
{
    float accel[3], gyro[3];
    
    if (tracker.imu_type == IMU_BMI270) {
        bmi270_data_t data;
        if (bmi270_read_data(&data) != 0) return;
        memcpy(accel, data.accel, sizeof(accel));
        memcpy(gyro, data.gyro, sizeof(gyro));
    } else if (tracker.imu_type == IMU_ICM42688) {
        icm42688_data_t data;
        if (icm42688_read_data(&data) != 0) return;
        memcpy(accel, data.accel, sizeof(accel));
        memcpy(gyro, data.gyro, sizeof(gyro));
        tracker.temperature = data.temperature;
    } else return;
    
    gyro[0] -= tracker.gyro_bias[0];
    gyro[1] -= tracker.gyro_bias[1];
    gyro[2] -= tracker.gyro_bias[2];
    
    vqf_update(gyro, accel, NULL);
    vqf_get_quaternion(tracker.quaternion);
    compute_linear_accel(tracker.quaternion, accel, tracker.linear_accel);
}

static void ble_task(void)
{
    if (tracker.connected && tracker.imu_ok && ble_slimevr_can_notify()) {
        ble_slimevr_send_sensor_data(tracker.quaternion, tracker.linear_accel);
    }
}

static void compute_linear_accel(const float q[4], const float accel[3], float lin_a[3])
{
    float w = q[0], x = q[1], y = q[2], z = q[3];
    lin_a[0] = accel[0] - 2.0f * (x*z - w*y);
    lin_a[1] = accel[1] - 2.0f * (w*x + y*z);
    lin_a[2] = accel[2] - (w*w - x*x - y*y + z*z);
}

static void on_ble_connect(void) { tracker.connected = true; current_led_pattern = LED_PULSE; }
static void on_ble_disconnect(void) { tracker.connected = false; current_led_pattern = LED_BLINK_SLOW; }

static void on_ble_command(uint8_t cmd, const uint8_t *data, uint8_t len)
{
    (void)data; (void)len;
    if (cmd == BLE_CMD_CALIBRATE_GYRO) {
        tracker.calibrating = true;
        calib_gyro_sum[0] = calib_gyro_sum[1] = calib_gyro_sum[2] = 0;
        calib_sample_count = 0;
        current_led_pattern = LED_ON;
    } else if (cmd == BLE_CMD_TARE) {
        vqf_reset();
    }
}

static void calibration_task(void)
{
    float gyro[3];
    if (tracker.imu_type == IMU_BMI270) {
        bmi270_data_t data; if (bmi270_read_data(&data) != 0) return;
        memcpy(gyro, data.gyro, sizeof(gyro));
    } else if (tracker.imu_type == IMU_ICM42688) {
        icm42688_data_t data; if (icm42688_read_data(&data) != 0) return;
        memcpy(gyro, data.gyro, sizeof(gyro));
    } else return;
    
    calib_gyro_sum[0] += gyro[0]; calib_gyro_sum[1] += gyro[1]; calib_gyro_sum[2] += gyro[2];
    if (++calib_sample_count >= CALIB_SAMPLES) {
        tracker.gyro_bias[0] = calib_gyro_sum[0] / CALIB_SAMPLES;
        tracker.gyro_bias[1] = calib_gyro_sum[1] / CALIB_SAMPLES;
        tracker.gyro_bias[2] = calib_gyro_sum[2] / CALIB_SAMPLES;
        vqf_reset();
        tracker.calibrating = false;
        current_led_pattern = tracker.connected ? LED_PULSE : LED_BLINK_SLOW;
    }
    hal_delay_ms(SENSOR_UPDATE_MS);
}

static void led_task(void)
{
    static uint8_t cnt = 0; static bool state = false; cnt++;
    switch (current_led_pattern) {
        case LED_OFF: state = false; break;
        case LED_ON: state = true; break;
        case LED_BLINK_SLOW: if (cnt >= 5) { cnt = 0; state = !state; } break;
        case LED_BLINK_FAST: state = !state; break;
        case LED_PULSE: if (cnt >= 20) { cnt = 0; state = true; } else if (cnt == 1) state = false; break;
    }
    hal_gpio_write(LED_PIN, state);
}

static void battery_task(void)
{
    if (tracker.connected) ble_slimevr_update_battery(tracker.battery_level, tracker.charging, tracker.battery_mv);
}
