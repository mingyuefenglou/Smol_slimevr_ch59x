/**
 * @file pins.h
 * @brief CH592X (28-pin QFN) 引脚定义
 * 
 * CH592X Pin Configuration for SlimeVR Tracker/Receiver
 * 基于 CH592/CH591 数据手册 V1.8
 */

#ifndef __BOARD_CH592X_PINS_H__
#define __BOARD_CH592X_PINS_H__

/*============================================================================
 * CH592X (28-pin QFN) 引脚排列图
 *
 *                    CH592X (28-pin QFN)
 *                   ┌────────────────────┐
 *        PA11/X32KO │1                 28│ PA10/X32KI
 *              VCC3 │2                 27│ VCC3
 *             VDCID │3                 26│ VDCIA
 *               VSW │4                 25│ ANT
 *       VIO33/VDD33 │5                 24│ VINTA
 *          PA8/RXD1 │6                 23│ X32MI
 *     PA9/TMR0/TXD1 │7                 22│ X32MO
 *       PB11/UD+    │8                 21│ GND
 *        PB10/UD-   │9                 20│ PB4/RXD0/PWM7
 *     PB7/TXD0/PWM9 │10                19│ PB23/RST
 *   PB6/RTS/PWM8    │11                18│ PB22/TMR3
 *          PA7/AIN11│12                17│ PA15/MISO/AIN5
 *          PA6/AIN10│13                16│ PA14/MOSI/AIN4
 *          PA5/AIN1 │14                15│ PA13/SCK0/AIN3
 *                   └────────────────────┘
 *                          PA4/AIN0 (底部焊盘)
 *                          PA12/SCS (底部焊盘)
 *============================================================================*/

/*============================================================================
 * ADC 通道映射
 *============================================================================*/
#define ADC_CH_PA4_AIN0         0
#define ADC_CH_PA5_AIN1         1
#define ADC_CH_PA12_AIN2        2
#define ADC_CH_PA13_AIN3        3
#define ADC_CH_PA14_AIN4        4
#define ADC_CH_PA15_AIN5        5
#define ADC_CH_PB6_AIN9         9
#define ADC_CH_PA6_AIN10        10
#define ADC_CH_PA7_AIN11        11
#define ADC_CH_PA8_AIN12        12
#define ADC_CH_PA9_AIN13        13

/*============================================================================
 * PWM 通道映射
 *============================================================================*/
#define PWM_CH_PA6_PWM4         4
#define PWM_CH_PA7_PWM5         5
#define PWM_CH_PB4_PWM7         7
#define PWM_CH_PB6_PWM8         8
#define PWM_CH_PB7_PWM9         9
#define PWM_CH_PB23_PWM11       11

/*============================================================================
 * UART 通道映射
 *============================================================================*/
#define UART0_TX                PB7
#define UART0_RX                PB4
#define UART1_TX                PA9
#define UART1_RX                PA8
#define UART2_TX_               PA7
#define UART2_RX_               PA6
#define UART3_TX                PA5
#define UART3_RX                PA4

/*============================================================================
 * 定时器映射
 *============================================================================*/
#define TMR0_PIN                PA9
#define TMR0_PIN_               PB23
#define TMR1_PIN                PA10
#define TMR1_PIN_               PB10
#define TMR2_PIN                PA11
#define TMR2_PIN_               PB11
#define TMR3_PIN                PB22

#endif /* __BOARD_CH592X_PINS_H__ */

