/**
 * @file watchdog.h
 * @brief v0.6.2 看门狗与故障恢复机制
 * 
 * 功能:
 * 1. 硬件看门狗 (WDOG) - 检测无限循环/死锁
 * 2. HardFault处理 - 崩溃时保存现场
 * 3. 复位原因检测 - 区分上电/看门狗/软复位
 * 4. 任务监控 - 检测主循环卡死
 * 
 * 使用方法:
 *   // 初始化
 *   wdog_init(2000);  // 2秒超时
 *   
 *   // 主循环中喂狗
 *   while (1) {
 *       wdog_feed();
 *       // ... 主循环代码 ...
 *   }
 */

#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * 配置
 *============================================================================*/

#define WDOG_TIMEOUT_MS         2000    // 看门狗超时 (2秒)
#define WDOG_FEED_INTERVAL_MS   500     // 建议喂狗间隔 (500ms)
#define TASK_MONITOR_TIMEOUT_MS 100     // 任务监控超时 (100ms)

/*============================================================================
 * 复位原因
 *============================================================================*/

typedef enum {
    RESET_REASON_UNKNOWN    = 0x00,     // 未知
    RESET_REASON_POWER_ON   = 0x01,     // 上电复位
    RESET_REASON_SOFTWARE   = 0x02,     // 软件复位
    RESET_REASON_WATCHDOG   = 0x04,     // 看门狗复位
    RESET_REASON_HARDFAULT  = 0x08,     // HardFault复位
    RESET_REASON_LOCKUP     = 0x10,     // CPU锁死
    RESET_REASON_EXTERNAL   = 0x20,     // 外部复位 (RST引脚)
    RESET_REASON_BROWNOUT   = 0x40,     // 欠压复位
} reset_reason_t;

/*============================================================================
 * 崩溃快照结构 (保存在Retained RAM)
 *============================================================================*/

typedef struct __attribute__((packed)) {
    uint32_t magic;             // 0xDEADBEEF 表示有效
    uint32_t pc;                // 程序计数器
    uint32_t lr;                // 链接寄存器 (返回地址)
    uint32_t sp;                // 栈指针
    uint32_t r0, r1, r2, r3;    // 通用寄存器
    uint32_t r12;               // R12
    uint32_t xpsr;              // 状态寄存器
    uint32_t cfsr;              // 可配置故障状态寄存器 (如果有)
    uint32_t hfsr;              // HardFault状态寄存器 (如果有)
    uint32_t bfar;              // 总线故障地址 (如果有)
    uint32_t mmfar;             // 内存管理故障地址 (如果有)
    uint32_t timestamp_ms;      // 崩溃时间戳
    uint8_t  reset_reason;      // 复位原因
    uint8_t  fault_type;        // 故障类型
    uint8_t  reserved[2];
} crash_snapshot_t;

#define CRASH_SNAPSHOT_MAGIC    0xDEADBEEF

/*============================================================================
 * 任务监控结构
 *============================================================================*/

typedef struct {
    uint32_t last_feed_time;    // 上次喂狗时间
    uint32_t feed_count;        // 喂狗计数
    uint32_t timeout_count;     // 超时次数 (软检测)
    bool     enabled;           // 是否启用
} task_monitor_t;

/*============================================================================
 * API函数
 *============================================================================*/

/**
 * @brief 初始化看门狗
 * @param timeout_ms 超时时间 (毫秒)
 */
void wdog_init(uint32_t timeout_ms);

/**
 * @brief 喂狗 (必须在超时前调用)
 */
void wdog_feed(void);

/**
 * @brief 禁用看门狗 (仅用于调试)
 */
void wdog_disable(void);

/**
 * @brief 获取上次复位原因
 * @return 复位原因枚举
 */
reset_reason_t wdog_get_reset_reason(void);

/**
 * @brief 获取复位原因字符串
 * @param reason 复位原因
 * @return 字符串描述
 */
const char* wdog_reset_reason_str(reset_reason_t reason);

/**
 * @brief 检查是否有崩溃快照
 * @return true 如果有有效的崩溃快照
 */
bool wdog_has_crash_snapshot(void);

/**
 * @brief 获取崩溃快照
 * @param snapshot 输出崩溃快照
 * @return true 如果成功
 */
bool wdog_get_crash_snapshot(crash_snapshot_t *snapshot);

/**
 * @brief 清除崩溃快照
 */
void wdog_clear_crash_snapshot(void);

/**
 * @brief 打印崩溃快照 (调试用)
 */
void wdog_print_crash_snapshot(void);

/**
 * @brief 触发软件复位
 */
void wdog_software_reset(void);

/**
 * @brief 任务监控初始化
 * @note 用于检测主循环是否卡死
 */
void task_monitor_init(void);

/**
 * @brief 任务监控检查 (在定时器中断中调用)
 * @return true 如果检测到任务超时
 */
bool task_monitor_check(void);

/**
 * @brief 标记任务活跃 (在主循环中调用)
 */
void task_monitor_alive(void);

/**
 * @brief 获取任务监控统计
 * @param timeout_count 输出超时次数
 * @param feed_count 输出喂狗次数
 */
void task_monitor_get_stats(uint32_t *timeout_count, uint32_t *feed_count);

/*============================================================================
 * v0.6.2: 检查点追踪 (用于定位死锁位置)
 * 
 * 使用方法:
 *   void some_function(void) {
 *       CHECKPOINT(0x01);  // 进入函数
 *       // ... 可能阻塞的操作 ...
 *       CHECKPOINT(0x02);  // 操作完成
 *   }
 *============================================================================*/

// 检查点ID定义
#define CP_MAIN_LOOP_START      0x10
#define CP_MAIN_LOOP_RF         0x11
#define CP_MAIN_LOOP_USB        0x12
#define CP_MAIN_LOOP_IMU        0x13
#define CP_MAIN_LOOP_FUSION     0x14
#define CP_MAIN_LOOP_END        0x1F

#define CP_RF_TX_START          0x20
#define CP_RF_TX_WAIT           0x21
#define CP_RF_TX_DONE           0x22
#define CP_RF_RX_START          0x23
#define CP_RF_RX_WAIT           0x24

#define CP_IMU_READ_START       0x30
#define CP_IMU_SPI_WAIT         0x31
#define CP_IMU_I2C_WAIT         0x32
#define CP_IMU_READ_DONE        0x33

#define CP_USB_TX_START         0x40
#define CP_USB_TX_WAIT          0x41
#define CP_USB_TX_DONE          0x42

/**
 * @brief 设置当前检查点
 * @param id 检查点ID
 */
void wdog_set_checkpoint(uint8_t id);

/**
 * @brief 获取最后检查点
 * @return 检查点ID
 */
uint8_t wdog_get_checkpoint(void);

// 便捷宏
#define CHECKPOINT(id)  wdog_set_checkpoint(id)

/*============================================================================
 * HardFault处理 (内部使用)
 *============================================================================*/

/**
 * @brief HardFault处理函数 (由启动代码调用)
 */
void HardFault_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __WATCHDOG_H__ */
