/**
 * @file error_codes.h
 * @brief SlimeVR CH59X 错误码定义
 * 
 * 统一的错误码定义，用于所有模块的错误处理
 */

#ifndef __ERROR_CODES_H__
#define __ERROR_CODES_H__

#include <stdint.h>

/*============================================================================
 * 错误码定义 / Error Code Definitions
 * 
 * 错误码格式: 0xMMEE
 *   MM = 模块ID (00-FF)
 *   EE = 错误编号 (00-FF)
 * 
 * 模块ID分配:
 *   0x00: 通用错误
 *   0x01: HAL层
 *   0x02: RF通信
 *   0x03: USB
 *   0x04: IMU传感器
 *   0x05: 传感器融合
 *   0x06: 存储
 *   0x07: 电源管理
 *============================================================================*/

typedef int16_t err_t;

/*----------------------------------------------------------------------------
 * 通用错误码 (0x00xx)
 *----------------------------------------------------------------------------*/
#define ERR_OK                      0       // 成功
#define ERR_FAIL                    -1      // 通用失败
#define ERR_INVALID_PARAM           -2      // 无效参数
#define ERR_NULL_POINTER            -3      // 空指针
#define ERR_TIMEOUT                 -4      // 超时
#define ERR_BUSY                    -5      // 忙
#define ERR_NOT_INITIALIZED         -6      // 未初始化
#define ERR_ALREADY_INITIALIZED     -7      // 已初始化
#define ERR_NOT_SUPPORTED           -8      // 不支持
#define ERR_OUT_OF_MEMORY           -9      // 内存不足
#define ERR_BUFFER_OVERFLOW         -10     // 缓冲区溢出

/*----------------------------------------------------------------------------
 * HAL层错误码 (0x01xx)
 *----------------------------------------------------------------------------*/
#define ERR_HAL_BASE                (-0x0100)
#define ERR_HAL_GPIO_FAIL           (ERR_HAL_BASE - 1)   // GPIO操作失败
#define ERR_HAL_SPI_FAIL            (ERR_HAL_BASE - 2)   // SPI操作失败
#define ERR_HAL_I2C_FAIL            (ERR_HAL_BASE - 3)   // I2C操作失败
#define ERR_HAL_I2C_NACK            (ERR_HAL_BASE - 4)   // I2C无应答
#define ERR_HAL_TIMER_FAIL          (ERR_HAL_BASE - 5)   // 定时器操作失败
#define ERR_HAL_ADC_FAIL            (ERR_HAL_BASE - 6)   // ADC操作失败

/*----------------------------------------------------------------------------
 * RF通信错误码 (0x02xx)
 *----------------------------------------------------------------------------*/
#define ERR_RF_BASE                 (-0x0200)
#define ERR_RF_INIT_FAIL            (ERR_RF_BASE - 1)    // RF初始化失败
#define ERR_RF_TX_FAIL              (ERR_RF_BASE - 2)    // RF发送失败
#define ERR_RF_RX_FAIL              (ERR_RF_BASE - 3)    // RF接收失败
#define ERR_RF_CRC_ERROR            (ERR_RF_BASE - 4)    // RF CRC错误
#define ERR_RF_NO_ACK               (ERR_RF_BASE - 5)    // RF无应答
#define ERR_RF_CHANNEL_BUSY         (ERR_RF_BASE - 6)    // RF信道繁忙
#define ERR_RF_NOT_PAIRED           (ERR_RF_BASE - 7)    // RF未配对
#define ERR_RF_PAIRING_FAIL         (ERR_RF_BASE - 8)    // RF配对失败
#define ERR_RF_SYNC_LOST            (ERR_RF_BASE - 9)    // RF同步丢失
#define ERR_RF_BUFFER_FULL          (ERR_RF_BASE - 10)   // RF缓冲区满

// 兼容宏 (向后兼容简写形式)
#define ERR_RF_INIT                 ERR_RF_INIT_FAIL

/*----------------------------------------------------------------------------
 * USB错误码 (0x03xx)
 *----------------------------------------------------------------------------*/
#define ERR_USB_BASE                (-0x0300)
#define ERR_USB_INIT_FAIL           (ERR_USB_BASE - 1)   // USB初始化失败
#define ERR_USB_NOT_CONNECTED       (ERR_USB_BASE - 2)   // USB未连接
#define ERR_USB_TX_FAIL             (ERR_USB_BASE - 3)   // USB发送失败
#define ERR_USB_RX_FAIL             (ERR_USB_BASE - 4)   // USB接收失败
#define ERR_USB_STALL               (ERR_USB_BASE - 5)   // USB端点停止
#define ERR_USB_BUFFER_FULL         (ERR_USB_BASE - 6)   // USB缓冲区满

// 兼容宏 (向后兼容简写形式)
#define ERR_USB_INIT                ERR_USB_INIT_FAIL

/*----------------------------------------------------------------------------
 * IMU传感器错误码 (0x04xx)
 *----------------------------------------------------------------------------*/
#define ERR_IMU_BASE                (-0x0400)
#define ERR_IMU_NOT_FOUND           (ERR_IMU_BASE - 1)   // IMU未找到
#define ERR_IMU_INIT_FAIL           (ERR_IMU_BASE - 2)   // IMU初始化失败
#define ERR_IMU_READ_FAIL           (ERR_IMU_BASE - 3)   // IMU读取失败
#define ERR_IMU_WRITE_FAIL          (ERR_IMU_BASE - 4)   // IMU写入失败
#define ERR_IMU_WRONG_ID            (ERR_IMU_BASE - 5)   // IMU ID错误
#define ERR_IMU_CALIBRATION_FAIL    (ERR_IMU_BASE - 6)   // IMU校准失败
#define ERR_IMU_DATA_NOT_READY      (ERR_IMU_BASE - 7)   // IMU数据未就绪
#define ERR_IMU_SELF_TEST_FAIL      (ERR_IMU_BASE - 8)   // IMU自检失败

/*----------------------------------------------------------------------------
 * 传感器融合错误码 (0x05xx)
 *----------------------------------------------------------------------------*/
#define ERR_FUSION_BASE             (-0x0500)
#define ERR_FUSION_INIT_FAIL        (ERR_FUSION_BASE - 1)   // 融合初始化失败
#define ERR_FUSION_DIVERGED         (ERR_FUSION_BASE - 2)   // 融合发散
#define ERR_FUSION_INVALID_QUAT     (ERR_FUSION_BASE - 3)   // 无效四元数

/*----------------------------------------------------------------------------
 * 存储错误码 (0x06xx)
 *----------------------------------------------------------------------------*/
#define ERR_STORAGE_BASE            (-0x0600)
#define ERR_STORAGE_READ_FAIL       (ERR_STORAGE_BASE - 1)  // 存储读取失败
#define ERR_STORAGE_WRITE_FAIL      (ERR_STORAGE_BASE - 2)  // 存储写入失败
#define ERR_STORAGE_ERASE_FAIL      (ERR_STORAGE_BASE - 3)  // 存储擦除失败
#define ERR_STORAGE_FULL            (ERR_STORAGE_BASE - 4)  // 存储已满
#define ERR_STORAGE_CORRUPT         (ERR_STORAGE_BASE - 5)  // 存储数据损坏

/*----------------------------------------------------------------------------
 * 电源管理错误码 (0x07xx)
 *----------------------------------------------------------------------------*/
#define ERR_POWER_BASE              (-0x0700)
#define ERR_POWER_LOW_BATTERY       (ERR_POWER_BASE - 1)    // 电池电量低
#define ERR_POWER_CRITICAL          (ERR_POWER_BASE - 2)    // 电池电量严重不足
#define ERR_POWER_CHARGING_FAIL     (ERR_POWER_BASE - 3)    // 充电失败

/*============================================================================
 * 错误处理宏
 *============================================================================*/

// 检查错误并返回
#define CHECK_ERR(expr) \
    do { \
        err_t _err = (expr); \
        if (_err != ERR_OK) return _err; \
    } while(0)

// 检查错误并跳转到标签
#define CHECK_ERR_GOTO(expr, label) \
    do { \
        err_t _err = (expr); \
        if (_err != ERR_OK) { \
            ret = _err; \
            goto label; \
        } \
    } while(0)

// 检查指针
#define CHECK_NULL(ptr) \
    do { \
        if ((ptr) == NULL) return ERR_NULL_POINTER; \
    } while(0)

// 检查参数范围
#define CHECK_RANGE(val, min, max) \
    do { \
        if ((val) < (min) || (val) > (max)) return ERR_INVALID_PARAM; \
    } while(0)

/*============================================================================
 * 错误码转字符串 (调试用)
 *============================================================================*/

#ifdef DEBUG_ENABLE
static inline const char* err_to_str(err_t err)
{
    switch(err) {
        case ERR_OK:                return "OK";
        case ERR_FAIL:              return "FAIL";
        case ERR_INVALID_PARAM:     return "INVALID_PARAM";
        case ERR_NULL_POINTER:      return "NULL_POINTER";
        case ERR_TIMEOUT:           return "TIMEOUT";
        case ERR_BUSY:              return "BUSY";
        case ERR_NOT_INITIALIZED:   return "NOT_INITIALIZED";
        case ERR_RF_TX_FAIL:        return "RF_TX_FAIL";
        case ERR_RF_RX_FAIL:        return "RF_RX_FAIL";
        case ERR_RF_NO_ACK:         return "RF_NO_ACK";
        case ERR_USB_NOT_CONNECTED: return "USB_NOT_CONNECTED";
        case ERR_IMU_NOT_FOUND:     return "IMU_NOT_FOUND";
        case ERR_IMU_READ_FAIL:     return "IMU_READ_FAIL";
        default:                    return "UNKNOWN";
    }
}
#endif

/*============================================================================
 * v0.6.2: 调试日志宏 (参考优化手册)
 * 
 * 用法:
 *   LOG_ERR("Storage read failed: %d", ret);
 *   LOG_WARN("Battery low: %d%%", battery_pct);
 *   LOG_INFO("IMU initialized: %s", imu_name);
 *============================================================================*/

#ifdef DEBUG_ENABLE
    #include <stdio.h>
    #define LOG_ERR(fmt, ...)   printf("[ERR] " fmt "\n", ##__VA_ARGS__)
    #define LOG_WARN(fmt, ...)  printf("[WARN] " fmt "\n", ##__VA_ARGS__)
    #define LOG_INFO(fmt, ...)  printf("[INFO] " fmt "\n", ##__VA_ARGS__)
    #define LOG_DBG(fmt, ...)   printf("[DBG] " fmt "\n", ##__VA_ARGS__)
#else
    // Release模式下禁用日志输出以节省Flash/RAM
    #define LOG_ERR(fmt, ...)   ((void)0)
    #define LOG_WARN(fmt, ...)  ((void)0)
    #define LOG_INFO(fmt, ...)  ((void)0)
    #define LOG_DBG(fmt, ...)   ((void)0)
#endif

#endif /* __ERROR_CODES_H__ */
