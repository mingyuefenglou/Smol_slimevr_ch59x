/**
 * @file hal_clkin.c
 * @brief 外部时钟输入配置 for IMU
 * 
 * 支持陀螺仪外部时钟输入，用于多传感器同步
 * External clock input support for gyroscope synchronization
 * 
 * CH592 可用引脚 / Available pins on CH592:
 *   - PA9  (TMR0_EXT) - 推荐 / Recommended
 *   - PA10 (TMR1_EXT)
 *   - PA11 (TMR2_EXT)
 * 
 * IMU 时钟规格 / IMU Clock Specifications:
 *   - ICM-42688/45686: 32.768kHz 外部时钟输入
 *   - BMI270: 32.768kHz 外部时钟输入
 */

#include "hal.h"

#ifdef CH59X
#include "CH59x_common.h"
#endif

/*============================================================================
 * 配置 / Configuration
 *============================================================================*/

// 外部时钟输入引脚 / External clock input pin
#define CLKIN_PIN           GPIO_Pin_9      // PA9 (TMR0_EXT)
#define CLKIN_GPIO_PORT     GPIOA
#define CLKIN_TIMER         TMR0

// IMU CLKIN 输出引脚 (MCU -> IMU)
// IMU CLKIN output pin (MCU -> IMU)
#define CLKOUT_PIN          GPIO_Pin_6      // PA6 (PWM)
#define CLKOUT_GPIO_PORT    GPIOA
#define CLKOUT_TIMER        TMR1

// 时钟频率 / Clock frequency
#define CLKIN_FREQ_HZ       32768           // 32.768kHz

/*============================================================================
 * 引脚映射 / Pin Mapping
 *============================================================================*/

/**
 * CH592 外部时钟引脚分配建议
 * CH592 External Clock Pin Assignment Recommendations:
 * 
 * ┌─────────────────────────────────────────────────────────┐
 * │                   CH592 引脚分配                         │
 * ├─────────┬───────────────────────────────────────────────┤
 * │  功能   │  引脚  │  说明                                 │
 * ├─────────┼────────┼─────────────────────────────────────────┤
 * │ CLKOUT  │  PA6   │ MCU 输出 32.768kHz 时钟到 IMU CLKIN    │
 * │ CLKIN   │  PA9   │ 可选: 外部时钟输入 (用于同步)          │
 * │ SPI_CS  │  PA4   │ IMU SPI 片选                          │
 * │ SPI_CLK │  PA12  │ IMU SPI 时钟                          │
 * │ SPI_MOSI│  PA13  │ IMU SPI 数据输出                      │
 * │ SPI_MISO│  PA14  │ IMU SPI 数据输入                      │
 * │ IMU_INT │  PA15  │ IMU 数据就绪中断                      │
 * └─────────┴────────┴─────────────────────────────────────────┘
 * 
 * 硬件连接示意 / Hardware Connection:
 * 
 *   CH592                    IMU (ICM-45686)
 *   ─────                    ───────────────
 *   PA6  (CLKOUT) ────────> CLKIN (Pin 6)
 *   PA4  (CS)     ────────> CS    (Pin 7)
 *   PA12 (SCK)    ────────> SCLK  (Pin 1)
 *   PA13 (MOSI)   ────────> SDI   (Pin 14)
 *   PA14 (MISO)   <──────── SDO   (Pin 2)
 *   PA15 (INT)    <──────── INT1  (Pin 4)
 */

/*============================================================================
 * 实现 / Implementation
 *============================================================================*/

/**
 * @brief 初始化外部时钟输出 (MCU -> IMU)
 * 
 * 使用 PWM 输出生成 32.768kHz 时钟信号
 * Use PWM output to generate 32.768kHz clock signal
 * 
 * @return 0 成功
 */
int hal_clkout_init(void)
{
#ifdef CH59X
    // 配置 PA6 为 PWM 输出
    // Configure PA6 as PWM output
    GPIOA_ModeCfg(CLKOUT_PIN, GPIO_ModeOut_PP_5mA);
    
    // 系统时钟 60MHz, 分频得到 32.768kHz
    // System clock 60MHz, divide to get 32.768kHz
    // 60MHz / 32.768kHz = 1831
    // 周期 = 1831, 占空比 50% = 915
    
    uint16_t period = 60000000 / CLKIN_FREQ_HZ;  // ~1831
    uint16_t compare = period / 2;               // 50% duty cycle
    
    // 配置 Timer1 PWM 模式
    TMR1_PWMInit(High_Level, PWM_Times_1);
    TMR1_PWMCycleCfg(period);
    TMR1_Disable();
    
    // 配置占空比
    // 注意: CH592 Timer PWM 需要特殊配置
    R16_TMR1_CNT_END = period - 1;
    R16_TMR1_FIFO = compare;
    
    TMR1_PWMActDataWidth(compare);
    TMR1_PWMEnable();
    
    return 0;
#else
    return -1;
#endif
}

/**
 * @brief 停止外部时钟输出
 */
void hal_clkout_stop(void)
{
#ifdef CH59X
    TMR1_PWMDisable();
    GPIOA_ModeCfg(CLKOUT_PIN, GPIO_ModeIN_Floating);
#endif
}

/**
 * @brief 初始化外部时钟输入 (可选, 用于同步)
 * 
 * @return 0 成功
 */
int hal_clkin_init(void)
{
#ifdef CH59X
    // 配置 PA9 为外部时钟输入
    GPIOA_ModeCfg(CLKIN_PIN, GPIO_ModeIN_Floating);
    
    // 配置 Timer0 外部计数模式
    TMR0_CapInit(Edge_To_Edge);
    TMR0_CAPTimeoutCfg(0xFFFFFFFF);  // 无超时
    
    return 0;
#else
    return -1;
#endif
}

/**
 * @brief 获取外部时钟计数
 * 
 * 用于验证时钟是否正常工作
 * Used to verify clock is working
 */
uint32_t hal_clkin_get_count(void)
{
#ifdef CH59X
    return TMR0_GetCurrentCount();
#else
    return 0;
#endif
}

/*============================================================================
 * IMU 时钟配置 / IMU Clock Configuration
 *============================================================================*/

/**
 * @brief 配置 ICM-45686 使用外部时钟
 * 
 * 需要写入 ICM-45686 寄存器:
 *   REG_INTF_CONFIG1 (0x4D) = 0x91
 *   - RTC_MODE = 1 (使用外部时钟)
 *   - CLKIN_SEL = 1 (选择 CLKIN 引脚)
 */
void icm45686_use_external_clock(void)
{
    // 这个函数应该在 icm45686.c 中实现
    // 示例寄存器配置:
    // hal_spi_write_reg(0x4D, 0x91);  // INTF_CONFIG1
}

/**
 * @brief 配置 BMI270 使用外部时钟
 * 
 * 需要写入 BMI270 寄存器:
 *   CMD (0x7E) = 0xB6 (软复位)
 *   然后配置 INIT_CTRL 和时钟源
 */
void bmi270_use_external_clock(void)
{
    // 这个函数应该在 bmi270.c 中实现
    // BMI270 外部时钟配置较复杂
}

/*============================================================================
 * 硬件设计建议 / Hardware Design Recommendations
 *============================================================================*/

/**
 * 外部时钟电路设计 / External Clock Circuit Design:
 * 
 *   CH592 PA6 ──┬── 100Ω ──┬── IMU CLKIN
 *               │          │
 *               │         ┴ 22pF (可选滤波)
 *               │         ─
 *               └── 1kΩ ──┴── (测试点)
 * 
 * 注意事项 / Notes:
 * 1. 保持走线短 (< 2cm)
 * 2. 远离高频干扰源
 * 3. 可添加小电阻 (100Ω) 阻尼振铃
 * 4. 32.768kHz 时钟精度影响传感器采样精度
 * 
 * 多传感器同步 / Multi-sensor Synchronization:
 * 
 *   CH592 PA6 ──┬── IMU1 CLKIN (Tracker 1)
 *               ├── IMU2 CLKIN (Tracker 2)
 *               └── IMUn CLKIN (Tracker n)
 * 
 * 使用共同时钟可使多个追踪器数据同步采样
 */
