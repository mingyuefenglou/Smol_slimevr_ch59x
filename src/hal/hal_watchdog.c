/**
 * @file hal_watchdog.c
 * @brief v0.6.2 看门狗与故障恢复机制实现
 * 
 * 功能:
 * 1. 硬件看门狗 - 检测软件死锁/无限循环
 * 2. HardFault处理 - 崩溃时保存现场到Retained RAM
 * 3. 复位原因检测 - 区分上电/看门狗/软复位
 * 4. 任务监控 - 软件层面检测主循环卡死
 * 
 * 重要: 主循环必须定期调用wdog_feed()，否则系统将复位！
 */

#include "watchdog.h"
#include "hal.h"
#include "event_logger.h"
#include "error_codes.h"
#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"
#include "CH59x_sys.h"
#endif

/*============================================================================
 * 配置
 *============================================================================*/

// Retained RAM区域 (深睡眠后保持)
// 注意: 需要链接脚本配置.retained段
#define RETAINED_SECTION __attribute__((section(".retained")))

/*============================================================================
 * Retained RAM数据 (复位后保持)
 *============================================================================*/

// 崩溃快照 (保存在Retained RAM中)
static crash_snapshot_t RETAINED_SECTION g_crash_snapshot;

// 复位计数器
static uint32_t RETAINED_SECTION g_reset_count;

// 复位原因标记
static uint8_t RETAINED_SECTION g_last_reset_reason;

// 看门狗复位标记 (用于区分看门狗复位和上电复位)
#define WDOG_RESET_MAGIC    0x57444F47  // "WDOG"
static uint32_t RETAINED_SECTION g_wdog_reset_magic;

/*============================================================================
 * 任务监控状态
 *============================================================================*/

static task_monitor_t g_task_monitor = {0};

/*============================================================================
 * 内部函数
 *============================================================================*/

// 保存崩溃快照
static void save_crash_snapshot(uint32_t *stack_frame, uint8_t fault_type)
{
    g_crash_snapshot.magic = CRASH_SNAPSHOT_MAGIC;
    g_crash_snapshot.timestamp_ms = hal_get_tick_ms();
    g_crash_snapshot.fault_type = fault_type;
    
    // 从栈帧恢复寄存器 (Cortex-M/RISC-V异常栈帧)
    if (stack_frame) {
        g_crash_snapshot.r0   = stack_frame[0];
        g_crash_snapshot.r1   = stack_frame[1];
        g_crash_snapshot.r2   = stack_frame[2];
        g_crash_snapshot.r3   = stack_frame[3];
        g_crash_snapshot.r12  = stack_frame[4];
        g_crash_snapshot.lr   = stack_frame[5];
        g_crash_snapshot.pc   = stack_frame[6];
        g_crash_snapshot.xpsr = stack_frame[7];
    }
    
    // 获取当前栈指针
#ifdef CH59X
    __asm volatile ("mv %0, sp" : "=r"(g_crash_snapshot.sp));
#endif
    
    // 故障状态寄存器 (RISC-V没有这些，留空)
    g_crash_snapshot.cfsr = 0;
    g_crash_snapshot.hfsr = 0;
    g_crash_snapshot.bfar = 0;
    g_crash_snapshot.mmfar = 0;
}

/*============================================================================
 * 看门狗API
 *============================================================================*/

void wdog_init(uint32_t timeout_ms)
{
#ifdef CH59X
    // 检查是否是看门狗复位
    if (g_wdog_reset_magic == WDOG_RESET_MAGIC) {
        g_last_reset_reason = RESET_REASON_WATCHDOG;
        g_wdog_reset_magic = 0;  // 清除标记
        g_reset_count++;
        
        // 记录事件
        event_log(EVT_WATCHDOG, NULL, 0);
        LOG_WARN("Watchdog reset detected! Count: %lu", g_reset_count);
    } else {
        // 检查其他复位原因
        // CH59X: R8_GLOB_RST_CFG 包含复位原因
        // 简化处理：假设是上电复位
        g_last_reset_reason = RESET_REASON_POWER_ON;
    }
    
    // 设置看门狗超时
    // CH59X看门狗时钟是32KHz，周期单位是时钟周期
    // timeout_ms * 32 = 周期数
    uint32_t period = (timeout_ms * 32) / 1000;
    if (period < 1) period = 1;
    if (period > 0xFFFF) period = 0xFFFF;
    
    // 配置看门狗
    WDOG_SetPeriod(period);
    
    // 设置看门狗复位标记 (用于下次复位时检测)
    g_wdog_reset_magic = WDOG_RESET_MAGIC;
    
    // 启用看门狗
    WDOG_Enable(ENABLE);
    
    LOG_INFO("Watchdog initialized: %lu ms timeout", timeout_ms);
#else
    (void)timeout_ms;
#endif
}

void wdog_feed(void)
{
#ifdef CH59X
    WDOG_Feed();
    
    // 更新任务监控
    g_task_monitor.last_feed_time = hal_get_tick_ms();
    g_task_monitor.feed_count++;
    g_task_monitor.timeout_count = 0;  // v0.6.2: 重置超时计数
#endif
}

void wdog_disable(void)
{
#ifdef CH59X
    WDOG_Enable(DISABLE);
    g_wdog_reset_magic = 0;  // 清除复位标记
    LOG_WARN("Watchdog disabled!");
#endif
}

reset_reason_t wdog_get_reset_reason(void)
{
    return (reset_reason_t)g_last_reset_reason;
}

const char* wdog_reset_reason_str(reset_reason_t reason)
{
    switch (reason) {
        case RESET_REASON_POWER_ON:   return "POWER_ON";
        case RESET_REASON_SOFTWARE:   return "SOFTWARE";
        case RESET_REASON_WATCHDOG:   return "WATCHDOG";
        case RESET_REASON_HARDFAULT:  return "HARDFAULT";
        case RESET_REASON_LOCKUP:     return "LOCKUP";
        case RESET_REASON_EXTERNAL:   return "EXTERNAL";
        case RESET_REASON_BROWNOUT:   return "BROWNOUT";
        default:                      return "UNKNOWN";
    }
}

bool wdog_has_crash_snapshot(void)
{
    return (g_crash_snapshot.magic == CRASH_SNAPSHOT_MAGIC);
}

bool wdog_get_crash_snapshot(crash_snapshot_t *snapshot)
{
    if (!wdog_has_crash_snapshot()) {
        return false;
    }
    
    memcpy(snapshot, &g_crash_snapshot, sizeof(crash_snapshot_t));
    return true;
}

void wdog_clear_crash_snapshot(void)
{
    g_crash_snapshot.magic = 0;
}

void wdog_print_crash_snapshot(void)
{
    if (!wdog_has_crash_snapshot()) {
        LOG_INFO("No crash snapshot available");
        return;
    }
    
    LOG_ERR("=== CRASH SNAPSHOT ===");
    LOG_ERR("PC:   0x%08lX", g_crash_snapshot.pc);
    LOG_ERR("LR:   0x%08lX", g_crash_snapshot.lr);
    LOG_ERR("SP:   0x%08lX", g_crash_snapshot.sp);
    LOG_ERR("R0-R3: 0x%08lX 0x%08lX 0x%08lX 0x%08lX",
            g_crash_snapshot.r0, g_crash_snapshot.r1,
            g_crash_snapshot.r2, g_crash_snapshot.r3);
    LOG_ERR("Time: %lu ms", g_crash_snapshot.timestamp_ms);
    LOG_ERR("Type: %d", g_crash_snapshot.fault_type);
    LOG_ERR("======================");
}

void wdog_software_reset(void)
{
#ifdef CH59X
    g_last_reset_reason = RESET_REASON_SOFTWARE;
    g_wdog_reset_magic = 0;  // 不是看门狗复位
    
    // CH59X软件复位
    SYS_ResetExecute();
#endif
}

/*============================================================================
 * 任务监控 (软件层面检测)
 *============================================================================*/

void task_monitor_init(void)
{
    g_task_monitor.last_feed_time = hal_get_tick_ms();
    g_task_monitor.feed_count = 0;
    g_task_monitor.timeout_count = 0;
    g_task_monitor.enabled = true;
}

bool task_monitor_check(void)
{
    if (!g_task_monitor.enabled) {
        return false;
    }
    
    uint32_t now = hal_get_tick_ms();
    uint32_t elapsed = now - g_task_monitor.last_feed_time;
    
    // v0.6.2: 分级超时检测 (在定时器中断中调用)
    // 1. 轻微超时 (100ms): 仅记录警告
    // 2. 中度超时 (500ms): 记录错误并计数
    // 3. 严重超时 (1000ms): 触发软复位 (比硬件看门狗更快响应)
    
    if (elapsed > 1000) {
        // 严重超时 - 主循环可能完全卡死
        g_task_monitor.timeout_count++;
        
        // v0.6.2: 记录最后检查点位置
        extern uint8_t wdog_get_checkpoint(void);
        extern const char* wdog_checkpoint_name(uint8_t id);
        uint8_t last_cp = wdog_get_checkpoint();
        
        LOG_ERR("CRITICAL: Main loop deadlock! Elapsed: %lu ms, LastCP: 0x%02X (%s)", 
                elapsed, last_cp, wdog_checkpoint_name(last_cp));
        
        // 保存死锁信息到崩溃快照
        g_crash_snapshot.magic = CRASH_SNAPSHOT_MAGIC;
        g_crash_snapshot.fault_type = 0xDD;  // 0xDD = Deadlock Detected
        g_crash_snapshot.timestamp_ms = now;
        g_crash_snapshot.reset_reason = RESET_REASON_LOCKUP;
        g_crash_snapshot.r0 = last_cp;  // 保存检查点到r0字段
        
        // 记录事件
        event_log(EVT_CRASH, (uint8_t*)&elapsed, 4);
        
        // 触发软件复位 (比等待硬件看门狗更快)
        wdog_software_reset();
        
        return true;
        
    } else if (elapsed > 500) {
        // 中度超时 - 可能有阻塞操作
        g_task_monitor.timeout_count++;
        
        extern uint8_t wdog_get_checkpoint(void);
        uint8_t last_cp = wdog_get_checkpoint();
        
        LOG_WARN("WARNING: Slow main loop! Elapsed: %lu ms, Count: %lu, CP: 0x%02X", 
                 elapsed, g_task_monitor.timeout_count, last_cp);
        
        // 如果连续多次中度超时，也触发复位
        if (g_task_monitor.timeout_count >= 5) {
            LOG_ERR("Too many slow iterations, resetting...");
            wdog_software_reset();
        }
        
        return true;
        
    } else if (elapsed > TASK_MONITOR_TIMEOUT_MS) {
        // 轻微超时 - 仅记录调试信息
        static uint32_t last_warn_time = 0;
        if (now - last_warn_time > 5000) {  // 每5秒最多警告一次
            LOG_DBG("Main loop slow: %lu ms", elapsed);
            last_warn_time = now;
        }
    }
    
    return false;
}

void task_monitor_alive(void)
{
    g_task_monitor.last_feed_time = hal_get_tick_ms();
    g_task_monitor.feed_count++;
    g_task_monitor.timeout_count = 0;  // v0.6.2: 重置超时计数
}

void task_monitor_get_stats(uint32_t *timeout_count, uint32_t *feed_count)
{
    if (timeout_count) *timeout_count = g_task_monitor.timeout_count;
    if (feed_count) *feed_count = g_task_monitor.feed_count;
}

/*============================================================================
 * HardFault处理 (由启动代码调用)
 *============================================================================*/

// 注意: RISC-V的异常处理与ARM Cortex-M不同
// CH59X使用RISC-V架构，异常向量表在启动代码中定义

void __attribute__((weak)) HardFault_Handler(void)
{
    // 获取栈指针
    uint32_t *sp;
#ifdef CH59X
    __asm volatile ("mv %0, sp" : "=r"(sp));
#else
    sp = 0;
#endif
    
    // 保存崩溃快照
    save_crash_snapshot(sp, RESET_REASON_HARDFAULT);
    g_last_reset_reason = RESET_REASON_HARDFAULT;
    
    // 记录事件
    event_log(EVT_CRASH, (const uint8_t *)&g_crash_snapshot.pc, 4);
    
    // 触发软件复位
#ifdef CH59X
    SYS_ResetExecute();
#endif
    
    // 如果复位失败，死循环等待看门狗
    while (1) {
        // 不喂狗，等待硬件看门狗复位
    }
}

/*============================================================================
 * 异常向量表入口 (RISC-V)
 * 注意: 这些需要在启动代码中配置
 *============================================================================*/

// CH59X RISC-V 异常处理
void __attribute__((weak, interrupt("machine"))) SW_Handler(void)
{
    // 软件中断处理
    HardFault_Handler();
}

// 非法指令异常
void __attribute__((weak, interrupt("machine"))) Illegal_Instruction_Handler(void)
{
    save_crash_snapshot(NULL, 0x02);  // 非法指令
    g_last_reset_reason = RESET_REASON_HARDFAULT;
    
#ifdef CH59X
    SYS_ResetExecute();
#endif
    while (1);
}

// 加载访问异常
void __attribute__((weak, interrupt("machine"))) Load_Access_Fault_Handler(void)
{
    save_crash_snapshot(NULL, 0x05);  // 加载访问故障
    g_last_reset_reason = RESET_REASON_HARDFAULT;
    
#ifdef CH59X
    SYS_ResetExecute();
#endif
    while (1);
}

// 存储访问异常
void __attribute__((weak, interrupt("machine"))) Store_Access_Fault_Handler(void)
{
    save_crash_snapshot(NULL, 0x07);  // 存储访问故障
    g_last_reset_reason = RESET_REASON_HARDFAULT;
    
#ifdef CH59X
    SYS_ResetExecute();
#endif
    while (1);
}

/*============================================================================
 * v0.6.2: 检查点追踪 (用于定位死锁位置)
 *============================================================================*/

// 当前检查点 (保存在Retained RAM中，复位后可查看)
static uint8_t RETAINED_SECTION g_last_checkpoint;
static uint32_t RETAINED_SECTION g_checkpoint_timestamp;

void wdog_set_checkpoint(uint8_t id)
{
    g_last_checkpoint = id;
    g_checkpoint_timestamp = hal_get_tick_ms();
}

uint8_t wdog_get_checkpoint(void)
{
    return g_last_checkpoint;
}

// 获取检查点名称 (调试用)
const char* wdog_checkpoint_name(uint8_t id)
{
    switch (id) {
        case 0x10: return "MAIN_LOOP_START";
        case 0x11: return "MAIN_LOOP_RF";
        case 0x12: return "MAIN_LOOP_USB";
        case 0x13: return "MAIN_LOOP_IMU";
        case 0x14: return "MAIN_LOOP_FUSION";
        case 0x1F: return "MAIN_LOOP_END";
        case 0x20: return "RF_TX_START";
        case 0x21: return "RF_TX_WAIT";
        case 0x22: return "RF_TX_DONE";
        case 0x23: return "RF_RX_START";
        case 0x24: return "RF_RX_WAIT";
        case 0x30: return "IMU_READ_START";
        case 0x31: return "IMU_SPI_WAIT";
        case 0x32: return "IMU_I2C_WAIT";
        case 0x33: return "IMU_READ_DONE";
        case 0x40: return "USB_TX_START";
        case 0x41: return "USB_TX_WAIT";
        case 0x42: return "USB_TX_DONE";
        default:   return "UNKNOWN";
    }
}
