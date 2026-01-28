/**
 * @file hal_timer.c
 * @brief Timer Hardware Abstraction Layer for CH59X
 * 
 * Provides system tick, millisecond/microsecond timing, and delays.
 * Uses TMR0 as the system tick timer.
 */

#include "hal.h"

#ifdef CH59X
#include "CH59x_common.h"
#include "CH59x_sys.h"
#include "CH59x_timer.h"
#endif

/*============================================================================
 * Configuration
 *============================================================================*/

#define SYSTEM_CLOCK_HZ     60000000UL  // 60MHz system clock
#define TICK_RATE_HZ        1000        // 1ms tick rate
#define TICK_PERIOD         (SYSTEM_CLOCK_HZ / TICK_RATE_HZ)

/*============================================================================
 * Static Variables
 *============================================================================*/

static volatile uint32_t sys_tick_ms = 0;
static volatile uint32_t sys_tick_us_offset = 0;

/*============================================================================
 * Interrupt Handler
 *============================================================================*/

#ifdef CH59X
__INTERRUPT
__HIGH_CODE
void TMR0_IRQHandler(void)
{
    if (TMR0_GetITFlag(TMR0_IT_CYC_END)) {
        TMR0_ClearITFlag(TMR0_IT_CYC_END);
        sys_tick_ms++;
        
        // v0.6.2: 每100ms检查一次任务监控 (检测软件死锁)
        // 这是在中断上下文中运行，即使主循环卡死也能执行
        if ((sys_tick_ms % 100) == 0) {
            extern bool task_monitor_check(void);
            task_monitor_check();
        }
    }
}
#endif

/*============================================================================
 * Public API
 *============================================================================*/

int hal_timer_init(void)
{
#ifdef CH59X
    // Configure TMR0 for 1ms period
    TMR0_TimerInit(TICK_PERIOD);
    TMR0_ITCfg(ENABLE, TMR0_IT_CYC_END);
    PFIC_EnableIRQ(TMR0_IRQn);
    
    sys_tick_ms = 0;
    return 0;
#else
    // Stub for non-CH59X builds
    return 0;
#endif
}

uint32_t hal_millis(void)
{
#ifdef CH59X
    return sys_tick_ms;
#else
    // For testing on PC, use a simple counter
    static uint32_t fake_ms = 0;
    return fake_ms++;
#endif
}

uint32_t hal_micros(void)
{
#ifdef CH59X
    // Get current timer count for sub-millisecond precision
    uint32_t ms = sys_tick_ms;
    uint32_t count = TMR0_GetCurrentCount();
    
    // Convert count to microseconds within current millisecond
    uint32_t us_in_ms = (count * 1000UL) / TICK_PERIOD;
    
    return (ms * 1000UL) + us_in_ms;
#else
    return hal_millis() * 1000;
#endif
}

void hal_delay_ms(uint32_t ms)
{
#ifdef CH59X
    uint32_t start = sys_tick_ms;
    while ((sys_tick_ms - start) < ms) {
        __WFI();  // Wait for interrupt (low power)
    }
#else
    // Stub - in real implementation would use system delay
    (void)ms;
#endif
}

void hal_delay_us(uint32_t us)
{
#ifdef CH59X
    // For short delays, use busy wait
    if (us < 1000) {
        // Approximate cycles per microsecond at 60MHz
        uint32_t cycles = us * 15;  // ~60MHz / 4 cycles per loop
        while (cycles--) {
            __NOP();
        }
    } else {
        // For longer delays, use millisecond delay
        hal_delay_ms(us / 1000);
        // Handle remainder
        uint32_t remainder = us % 1000;
        uint32_t cycles = remainder * 15;
        while (cycles--) {
            __NOP();
        }
    }
#else
    (void)us;
#endif
}

/*============================================================================
 * High Resolution Timer (for sensor timing)
 *============================================================================*/

static volatile uint32_t hr_timer_overflow = 0;

#ifdef CH59X
__INTERRUPT
__HIGH_CODE
void TMR1_IRQHandler(void)
{
    if (TMR1_GetITFlag(TMR1_IT_CYC_END)) {
        TMR1_ClearITFlag(TMR1_IT_CYC_END);
        hr_timer_overflow++;
    }
}
#endif

int hal_hr_timer_init(void)
{
#ifdef CH59X
    // TMR1 as free-running counter for high-resolution timing
    TMR1_TimerInit(0xFFFFFFFF);  // Max period
    TMR1_ITCfg(ENABLE, TMR1_IT_CYC_END);
    PFIC_EnableIRQ(TMR1_IRQn);
    
    hr_timer_overflow = 0;
    return 0;
#else
    return 0;
#endif
}

uint64_t hal_hr_timer_get_ticks(void)
{
#ifdef CH59X
    uint32_t overflow = hr_timer_overflow;
    uint32_t count = TMR1_GetCurrentCount();
    
    // Check for race condition
    if (hr_timer_overflow != overflow) {
        overflow = hr_timer_overflow;
        count = TMR1_GetCurrentCount();
    }
    
    return ((uint64_t)overflow << 32) | count;
#else
    return hal_micros();
#endif
}

uint32_t hal_hr_timer_ticks_to_us(uint64_t ticks)
{
#ifdef CH59X
    // At 60MHz, 60 ticks = 1us
    return (uint32_t)(ticks / 60);
#else
    return (uint32_t)ticks;
#endif
}

/*============================================================================
 * CRC16 Calculation (v0.6.2 统一实现)
 *============================================================================*/

uint16_t hal_crc16(const void *data, uint16_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint16_t crc = 0xFFFF;
    
    for (uint16_t i = 0; i < len; i++) {
        crc ^= p[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
