/**
 * @file sensor_dma.h
 * @brief 传感器DMA传输模块
 * 
 * 功能:
 * 1. 使用DMA异步读取IMU数据
 * 2. INT1中断触发 + 双缓冲
 * 3. 批量数据处理
 * 4. 零拷贝数据传递
 */

#ifndef __SENSOR_DMA_H__
#define __SENSOR_DMA_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 数据结构
 *============================================================================*/

typedef struct {
    float gyro[3];      // 陀螺仪 [rad/s]
    float accel[3];     // 加速度计 [m/s²]
    float temp;         // 温度 [°C]
    uint32_t timestamp; // 时间戳 [us]
} sensor_data_t;

typedef struct {
    sensor_data_t data[16];  // 批量数据缓冲
    uint8_t count;           // 有效数据数量
    uint32_t start_time;     // 批次开始时间
} sensor_batch_t;

/*============================================================================
 * API 函数
 *============================================================================*/

/**
 * @brief 初始化DMA传感器模块
 */
void sensor_dma_init(void);

/**
 * @brief INT1中断处理 (在GPIO中断中调用)
 */
void sensor_dma_int1_handler(void);

/**
 * @brief DMA传输完成处理 (在DMA中断中调用)
 */
void sensor_dma_complete_handler(void);

/**
 * @brief 启动DMA传输
 */
void sensor_dma_start_transfer(void);

/**
 * @brief 预取下一批数据
 */
void sensor_dma_prefetch_next(void);

/**
 * @brief 读取批量数据
 * @param batch 输出批量数据
 * @return 读取的数据数量
 */
int sensor_dma_read_batch(sensor_batch_t *batch);

/**
 * @brief 检查是否有数据就绪
 * @return true 如果有数据可读
 */
bool sensor_dma_data_ready(void);

/**
 * @brief 获取单次数据 (兼容接口)
 * @param gyro 陀螺仪输出
 * @param accel 加速度计输出
 * @param temp 温度输出 (可为NULL)
 * @return 0 成功
 */
int sensor_dma_get_data(float gyro[3], float accel[3], float *temp);

/**
 * @brief 设置数据回调
 * @param callback 数据就绪时的回调函数
 */
void sensor_dma_set_callback(void (*callback)(const sensor_data_t *));

/**
 * @brief 更新统计信息
 */
void sensor_dma_update_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* __SENSOR_DMA_H__ */
