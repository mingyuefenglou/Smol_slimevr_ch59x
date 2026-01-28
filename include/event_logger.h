/**
 * @file event_logger.h
 * @brief v0.6.0 事件环形缓冲区与崩溃快照
 * 
 * 功能:
 * - 环形缓冲区记录最近50条事件
 * - 崩溃快照保存到Flash
 * - 导出JSON格式诊断报告
 */

#ifndef EVENT_LOGGER_H
#define EVENT_LOGGER_H

#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * 配置
 *============================================================================*/

#define EVENT_RING_SIZE         50      // 最近50条事件
#define EVENT_FLASH_OFFSET      0x0600  // Flash存储偏移
#define EVENT_MAX_DATA_LEN      8       // 每条事件最大附加数据

/*============================================================================
 * 事件类型
 *============================================================================*/

typedef enum {
    // 系统事件 (0x00-0x0F)
    EVT_BOOT                = 0x01,     // 系统启动
    EVT_SHUTDOWN            = 0x02,     // 正常关机
    EVT_CRASH               = 0x03,     // 崩溃/异常复位
    EVT_WATCHDOG            = 0x04,     // 看门狗复位
    EVT_LOW_BATTERY         = 0x05,     // 低电量
    
    // RF事件 (0x10-0x1F)
    EVT_RF_SYNC_LOST        = 0x10,     // 同步丢失
    EVT_RF_SYNC_FOUND       = 0x11,     // 同步恢复
    EVT_RF_TIMEOUT          = 0x12,     // RF超时
    EVT_RF_CRC_FAIL         = 0x13,     // CRC错误
    EVT_RF_CHANNEL_SWITCH   = 0x14,     // 信道切换
    EVT_RF_BLACKLIST        = 0x15,     // 信道黑名单
    
    // 配对事件 (0x20-0x2F)
    EVT_PAIR_START          = 0x20,     // 开始配对
    EVT_PAIR_SUCCESS        = 0x21,     // 配对成功
    EVT_PAIR_FAIL           = 0x22,     // 配对失败
    EVT_PAIR_CLEAR          = 0x23,     // 清除配对
    
    // 睡眠事件 (0x30-0x3F)
    EVT_SLEEP_ENTER         = 0x30,     // 进入睡眠
    EVT_SLEEP_EXIT          = 0x31,     // 退出睡眠
    EVT_WOM_TRIGGER         = 0x32,     // WOM触发
    EVT_BTN_WAKE            = 0x33,     // 按键唤醒
    
    // IMU事件 (0x40-0x4F)
    EVT_IMU_ERROR           = 0x40,     // IMU错误
    EVT_IMU_CALIB_START     = 0x41,     // 校准开始
    EVT_IMU_CALIB_DONE      = 0x42,     // 校准完成
    EVT_IMU_WOM_SET         = 0x43,     // WOM配置
    
    // 用户事件 (0x50-0x5F)
    EVT_BTN_PRESS           = 0x50,     // 按键按下
    EVT_BTN_LONG            = 0x51,     // 长按
    EVT_BTN_DOUBLE          = 0x52,     // 双击
} event_type_t;

/*============================================================================
 * 事件结构
 *============================================================================*/

typedef struct {
    uint32_t timestamp_ms;              // 时间戳
    uint8_t type;                       // 事件类型
    uint8_t data_len;                   // 附加数据长度
    uint8_t data[EVENT_MAX_DATA_LEN];   // 附加数据
} __attribute__((packed)) event_entry_t;

/*============================================================================
 * 崩溃快照结构
 *============================================================================*/

typedef struct {
    uint32_t magic;                     // 0x43525348 "CRSH"
    uint32_t timestamp_ms;              // 崩溃时间
    uint32_t pc;                        // 程序计数器
    uint32_t sp;                        // 栈指针
    uint32_t ra;                        // 返回地址
    uint8_t crash_type;                 // 崩溃类型
    uint8_t rf_state;                   // RF状态
    uint8_t tracker_state;              // Tracker状态
    uint8_t reserved;
    
    // 计数器快照
    uint32_t rf_timeout_count;
    uint32_t miss_sync_count;
    uint32_t crc_fail_count;
    uint32_t sleep_count;
    uint32_t wake_count;
    
    uint16_t crc;                       // CRC校验
} __attribute__((packed)) event_crash_snapshot_t;

/*============================================================================
 * 诊断报告结构 (JSON导出用)
 *============================================================================*/

typedef struct {
    // 版本信息
    uint8_t fw_major;
    uint8_t fw_minor;
    uint8_t fw_patch;
    uint8_t board_id;
    uint8_t imu_type;
    uint8_t mag_type;
    
    // 配置信息
    uint16_t superframe_us;
    uint16_t slot_us;
    uint8_t max_trackers;
    uint8_t sleep_mode;
    
    // 计数器
    uint32_t rf_timeout_count;
    uint32_t miss_sync_count;
    uint32_t crc_fail_count;
    uint32_t sleep_count;
    uint32_t wake_count;
    uint32_t uptime_sec;
    
    // 最近事件数
    uint8_t event_count;
} diag_report_header_t;

/*============================================================================
 * API
 *============================================================================*/

/**
 * @brief 初始化事件日志模块
 */
void event_logger_init(void);

/**
 * @brief 记录事件
 * @param type 事件类型
 * @param data 附加数据 (可为NULL)
 * @param data_len 数据长度
 */
void event_log(event_type_t type, const uint8_t *data, uint8_t data_len);

/**
 * @brief 记录简单事件 (无附加数据)
 */
void event_log_simple(event_type_t type);

/**
 * @brief 记录带1字节数据的事件
 */
void event_log_u8(event_type_t type, uint8_t value);

/**
 * @brief 记录带4字节数据的事件
 */
void event_log_u32(event_type_t type, uint32_t value);

/**
 * @brief 获取最近的事件
 * @param events 输出缓冲区
 * @param max_count 最大数量
 * @return 实际返回的事件数
 */
uint8_t event_get_recent(event_entry_t *events, uint8_t max_count);

/**
 * @brief 保存崩溃快照到Flash
 * @param pc 程序计数器
 * @param sp 栈指针
 * @param ra 返回地址
 * @param crash_type 崩溃类型
 */
void crash_snapshot_save(uint32_t pc, uint32_t sp, uint32_t ra, uint8_t crash_type);

/**
 * @brief 检查是否有崩溃快照
 * @return true 有快照
 */
bool crash_snapshot_exists(void);

/**
 * @brief 读取崩溃快照
 * @param snapshot 输出
 * @return 0成功，负值失败
 */
int event_crash_snapshot_read(event_crash_snapshot_t *snapshot);

/**
 * @brief 清除崩溃快照
 */
void event_crash_snapshot_clear(void);

/**
 * @brief 生成JSON格式诊断报告
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 实际写入字节数
 */
uint16_t diag_report_generate_json(char *buf, uint16_t buf_size);

/**
 * @brief 生成二进制格式诊断报告 (紧凑)
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 实际写入字节数
 */
uint16_t diag_report_generate_bin(uint8_t *buf, uint16_t buf_size);

#endif // EVENT_LOGGER_H
