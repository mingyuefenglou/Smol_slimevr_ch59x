/**
 * @file sensor_optimized.c
 * @brief 优化的传感器读取 - DMA + 中断驱动
 * 
 * 目标: 传感器延迟 < 3ms
 * 
 * 架构:
 * 1. IMU 中断触发数据就绪
 * 2. DMA 传输读取数据
 * 3. 后台处理融合算法
 */

#include "config.h"
#include "hal.h"
#include "imu_interface.h"
#include "vqf_ultra.h"
#include <string.h>

/*============================================================================
 * 配置
 *============================================================================*/

#define SENSOR_FIFO_SIZE        16      // 数据 FIFO 深度
#define SENSOR_READ_TIMEOUT_US  2000    // 读取超时 2ms

/*============================================================================
 * 数据结构
 *============================================================================*/

typedef struct {
    float gyro[3];
    float accel[3];
    uint32_t timestamp_us;
    bool valid;
} sensor_sample_t;

typedef struct {
    sensor_sample_t samples[SENSOR_FIFO_SIZE];
    volatile uint8_t write_idx;
    volatile uint8_t read_idx;
    volatile uint8_t count;
    
    // 状态
    volatile bool data_ready;
    volatile bool reading;
    volatile uint32_t last_read_us;
    volatile uint32_t read_latency_us;
    
    // 统计
    uint32_t total_samples;
    uint32_t dropped_samples;
    uint32_t max_latency_us;
    float avg_latency_us;
} sensor_fifo_t;

static sensor_fifo_t sensor_fifo = {0};

// DMA 读取缓冲区
static uint8_t imu_dma_buf[16] __attribute__((aligned(4)));

/*============================================================================
 * IMU 数据就绪中断
 *============================================================================*/

static void imu_data_ready_callback(void)
{
    uint32_t now_us = hal_micros();
    
    sensor_fifo.data_ready = true;
    
    // 立即启动 DMA 读取
    if (!sensor_fifo.reading) {
        sensor_fifo.reading = true;
        sensor_fifo.last_read_us = now_us;
        
        // 触发 DMA 读取 (非阻塞)
        // 实际数据在 DMA 完成回调中处理
        // TODO: 实现 DMA 异步读取功能
        // imu_read_dma_async(imu_dma_buf, sizeof(imu_dma_buf), NULL);
    }
}

/*============================================================================
 * DMA 完成回调
 *============================================================================*/

static void imu_dma_complete_callback(uint8_t *data, uint16_t len)
{
    uint32_t now_us = hal_micros();
    
    // 计算延迟
    sensor_fifo.read_latency_us = now_us - sensor_fifo.last_read_us;
    
    // 更新统计
    if (sensor_fifo.read_latency_us > sensor_fifo.max_latency_us) {
        sensor_fifo.max_latency_us = sensor_fifo.read_latency_us;
    }
    
    // 滑动平均
    sensor_fifo.avg_latency_us = sensor_fifo.avg_latency_us * 0.95f + 
                                  sensor_fifo.read_latency_us * 0.05f;
    
    // 解析数据
    sensor_sample_t sample;
    sample.timestamp_us = now_us;
    sample.valid = true;
    
    // 根据 IMU 类型解析 (假设 ICM42688 格式)
    int16_t raw_accel[3], raw_gyro[3];
    raw_accel[0] = (data[0] << 8) | data[1];
    raw_accel[1] = (data[2] << 8) | data[3];
    raw_accel[2] = (data[4] << 8) | data[5];
    raw_gyro[0] = (data[6] << 8) | data[7];
    raw_gyro[1] = (data[8] << 8) | data[9];
    raw_gyro[2] = (data[10] << 8) | data[11];
    
    // 转换为物理单位
    float accel_scale = 9.81f / 2048.0f;  // ±16g
    float gyro_scale = 0.001065264f;       // ±2000dps in rad/s
    
    sample.accel[0] = raw_accel[0] * accel_scale;
    sample.accel[1] = raw_accel[1] * accel_scale;
    sample.accel[2] = raw_accel[2] * accel_scale;
    sample.gyro[0] = raw_gyro[0] * gyro_scale;
    sample.gyro[1] = raw_gyro[1] * gyro_scale;
    sample.gyro[2] = raw_gyro[2] * gyro_scale;
    
    // 写入 FIFO
    if (sensor_fifo.count < SENSOR_FIFO_SIZE) {
        sensor_fifo.samples[sensor_fifo.write_idx] = sample;
        sensor_fifo.write_idx = (sensor_fifo.write_idx + 1) % SENSOR_FIFO_SIZE;
        sensor_fifo.count++;
        sensor_fifo.total_samples++;
    } else {
        sensor_fifo.dropped_samples++;
    }
    
    sensor_fifo.reading = false;
    sensor_fifo.data_ready = false;
}

/*============================================================================
 * 初始化
 *============================================================================*/

int sensor_optimized_init(void)
{
    memset(&sensor_fifo, 0, sizeof(sensor_fifo));
    
    // 初始化 DMA
    hal_dma_init();
    
    // 配置 IMU 中断回调
    // 使用hal_gpio_set_interrupt注册回调，这样GPIOA_IRQHandler会自动调用它
#ifdef PIN_IMU_INT1
    hal_gpio_set_interrupt(PIN_IMU_INT1, HAL_GPIO_INT_RISING, imu_data_ready_callback);
#elif defined(CH59X)
    // 如果没有定义PIN_IMU_INT1，使用默认配置 (PA11)
    // 注意：这种直接配置方式需要hal_gpio.c中的回调机制支持
    GPIOA_ModeCfg(GPIO_Pin_11, GPIO_ModeIN_Floating);
    GPIOA_ITModeCfg(GPIO_Pin_11, GPIO_ITMode_RiseEdge);
    PFIC_EnableIRQ(GPIO_A_IRQn);
    // 警告: 这种方式需要手动在hal_gpio.c中处理PA11的中断
#endif
    
    return 0;
}

/*============================================================================
 * 获取传感器数据
 *============================================================================*/

bool sensor_optimized_get_sample(float gyro[3], float accel[3], uint32_t *timestamp_us)
{
    if (!gyro || !accel) {
        return false;
    }
    
    if (sensor_fifo.count == 0) {
        return false;
    }
    
    // 从 FIFO 读取
    sensor_sample_t *sample = &sensor_fifo.samples[sensor_fifo.read_idx];
    
    if (!sample->valid) {
        return false;
    }
    
    memcpy(gyro, sample->gyro, sizeof(float) * 3);
    memcpy(accel, sample->accel, sizeof(float) * 3);
    
    if (timestamp_us) {
        *timestamp_us = sample->timestamp_us;
    }
    
    // 更新读指针
    sensor_fifo.read_idx = (sensor_fifo.read_idx + 1) % SENSOR_FIFO_SIZE;
    sensor_fifo.count--;
    
    return true;
}

/*============================================================================
 * 获取统计信息
 *============================================================================*/

void sensor_optimized_get_stats(uint32_t *total, uint32_t *dropped, 
                                 float *avg_latency, uint32_t *max_latency)
{
    if (total) *total = sensor_fifo.total_samples;
    if (dropped) *dropped = sensor_fifo.dropped_samples;
    if (avg_latency) *avg_latency = sensor_fifo.avg_latency_us;
    if (max_latency) *max_latency = sensor_fifo.max_latency_us;
}

/*============================================================================
 * 数据就绪检查
 *============================================================================*/

uint8_t sensor_optimized_available(void)
{
    return sensor_fifo.count;
}

/*============================================================================
 * GPIO 中断处理 (IMU 数据就绪)
 * 注意: GPIOA_IRQHandler 已在 hal_gpio.c 中定义为 weak
 * IMU 数据就绪中断应通过 hal_gpio_set_interrupt() 设置回调
 * 
 * 初始化时调用:
 *   hal_gpio_set_interrupt(PIN_IMU_INT1, HAL_GPIO_INT_RISING, imu_data_ready_callback);
 *============================================================================*/
