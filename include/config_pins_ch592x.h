/**
 * @file config_pins_ch592x.h
 * @brief CH592X (28-pin QFN) 引脚定义
 * 
 * CH592X Pin Configuration for SlimeVR Tracker/Receiver
 * 基于 CH592/CH591 数据手册 V1.8
 */

#ifndef __CONFIG_PINS_CH592X_H__
#define __CONFIG_PINS_CH592X_H__

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
 *
 *============================================================================*/

/*============================================================================
 * 引脚功能表
 *============================================================================*/

/*
| Pin | 名称 | 类型 | 主要功能 | 备用功能 |
|-----|------|------|----------|----------|
| 1   | PA11 | I/O  | GPIO     | X32KO, TMR2 |
| 2   | VCC3 | PWR  | 3.3V电源 | - |
| 3   | VDCID| PWR  | DC-DC输入| - |
| 4   | VSW  | PWR  | DC-DC输出| - |
| 5   | VIO33| PWR  | I/O电源  | VDD33 |
| 6   | PA8  | I/O  | GPIO     | RXD1, AIN12 |
| 7   | PA9  | I/O  | GPIO     | TMR0, TXD1, AIN13 |
| 8   | PB11 | I/O  | USB D+   | TMR2_ |
| 9   | PB10 | I/O  | USB D-   | TMR1_ |
| 10  | PB7  | I/O  | GPIO     | TXD0, PWM9 |
| 11  | PB6  | I/O  | GPIO     | RTS, PWM8, AIN9 |
| 12  | PA7  | I/O  | GPIO     | TXD2_, PWM5_, AIN11 |
| 13  | PA6  | I/O  | GPIO     | RXD2_, PWM4_, AIN10 |
| 14  | PA5  | I/O  | GPIO     | TXD3, AIN1 |
| 15  | PA13 | I/O  | SPI SCK  | PWM5, AIN3 |
| 16  | PA14 | I/O  | SPI MOSI | TXD0_, AIN4 |
| 17  | PA15 | I/O  | SPI MISO | RXD0_, AIN5 |
| 18  | PB22 | I/O  | GPIO     | TMR3, RXD2 |
| 19  | PB23 | I/O  | RST/GPIO | TMR0_, TXD2, PWM11 |
| 20  | PB4  | I/O  | GPIO     | RXD0, PWM7 |
| 21  | GND  | PWR  | 接地     | - |
| 22  | X32MO| ANA  | 32MHz晶振| - |
| 23  | X32MI| ANA  | 32MHz晶振| - |
| 24  | VINTA| PWR  | RF VCO   | - |
| 25  | ANT  | ANA  | RF天线   | - |
| 26  | VDCIA| PWR  | RF LDO   | - |
| 27  | VCC3 | PWR  | 3.3V电源 | - |
| 28  | PA10 | I/O  | GPIO     | X32KI, TMR1 |
| PAD | PA4  | I/O  | GPIO     | RXD3, AIN0 |
| PAD | PA12 | I/O  | SPI CS   | SCS, PWM4, AIN2 |
*/

/*============================================================================
 * CH592X Tracker 引脚配置
 *============================================================================*/

#ifdef CONFIG_CH592X_TRACKER

// LED 状态指示灯
#define PIN_LED                 9       // PA9 (Pin 7)
#define PIN_LED_GPIO            GPIO_Pin_9
#define PIN_LED_PORT            GPIOA

// 用户按键 (BOOT)
#define PIN_SW0                 4       // PB4 (Pin 20)
#define PIN_SW0_GPIO            GPIO_Pin_4
#define PIN_SW0_PORT            GPIOB

// IMU SPI 接口
#define PIN_SPI_CS              12      // PA12 (底部焊盘)
#define PIN_SPI_CS_GPIO         GPIO_Pin_12
#define PIN_SPI_CS_PORT         GPIOA
#define PIN_SPI_CLK             13      // PA13 (Pin 15) - 硬件固定
#define PIN_SPI_MOSI            14      // PA14 (Pin 16) - 硬件固定
#define PIN_SPI_MISO            15      // PA15 (Pin 17) - 硬件固定

// IMU 中断
#define PIN_IMU_INT1            11      // PA11 (Pin 1)
#define PIN_IMU_INT1_GPIO       GPIO_Pin_11
#define PIN_IMU_INT1_PORT       GPIOA
#define PIN_IMU_INT2            6       // PB6 (Pin 11) - 可选
#define PIN_IMU_INT2_GPIO       GPIO_Pin_6
#define PIN_IMU_INT2_PORT       GPIOB

// 充电检测
#define PIN_CHRG_DET            5       // PA5 (Pin 14)
#define PIN_CHRG_DET_GPIO       GPIO_Pin_5
#define PIN_CHRG_DET_PORT       GPIOA

// 电池 ADC
#define PIN_VBAT_ADC            7       // PA7 (Pin 12)
#define PIN_VBAT_ADC_GPIO       GPIO_Pin_7
#define PIN_VBAT_ADC_PORT       GPIOA
#define PIN_VBAT_ADC_CHANNEL    11      // AIN11

// USB (硬件固定)
#define PIN_USB_DP              11      // PB11 (Pin 8)
#define PIN_USB_DM              10      // PB10 (Pin 9)

#endif /* CONFIG_CH592X_TRACKER */

/*============================================================================
 * CH592X Receiver 引脚配置
 *============================================================================*/

#ifdef CONFIG_CH592X_RECEIVER

// LED 状态指示灯
#define PIN_LED                 9       // PA9 (Pin 7)
#define PIN_LED_GPIO            GPIO_Pin_9
#define PIN_LED_PORT            GPIOA

// 用户按键 (配对按钮)
#define PIN_PAIR_BTN            4       // PB4 (Pin 20)
#define PIN_PAIR_BTN_GPIO       GPIO_Pin_4
#define PIN_PAIR_BTN_PORT       GPIOB

// USB (硬件固定)
#define PIN_USB_DP              11      // PB11 (Pin 8)
#define PIN_USB_DM              10      // PB10 (Pin 9)

// 调试串口
#define PIN_DEBUG_TX            7       // PB7 (Pin 10) - TXD0
#define PIN_DEBUG_TX_GPIO       GPIO_Pin_7
#define PIN_DEBUG_TX_PORT       GPIOB
#define PIN_DEBUG_RX            4       // PB4 (Pin 20) - RXD0
#define PIN_DEBUG_RX_GPIO       GPIO_Pin_4
#define PIN_DEBUG_RX_PORT       GPIOB

// RF 天线 (硬件固定)
#define PIN_RF_ANT              25      // ANT (Pin 25)

#endif /* CONFIG_CH592X_RECEIVER */

/*============================================================================
 * I2C 引脚配置 (备用 IMU 接口)
 *============================================================================*/

// I2C 使用软件模拟或 PB22/PB23
#define PIN_I2C_SCL             22      // PB22 (Pin 18)
#define PIN_I2C_SCL_GPIO        GPIO_Pin_22
#define PIN_I2C_SCL_PORT        GPIOB
#define PIN_I2C_SDA             23      // PB23 (Pin 19)
#define PIN_I2C_SDA_GPIO        GPIO_Pin_23
#define PIN_I2C_SDA_PORT        GPIOB

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

#define PWM_CH_PA6_PWM4         4       // PWM4_ (映射)
#define PWM_CH_PA7_PWM5         5       // PWM5_ (映射)
#define PWM_CH_PB4_PWM7         7
#define PWM_CH_PB6_PWM8         8
#define PWM_CH_PB7_PWM9         9
#define PWM_CH_PB23_PWM11       11

/*============================================================================
 * UART 通道映射
 *============================================================================*/

// UART0 (默认)
#define UART0_TX                PB7     // TXD0
#define UART0_RX                PB4     // RXD0

// UART1
#define UART1_TX                PA9     // TXD1
#define UART1_RX                PA8     // RXD1

// UART2 (映射)
#define UART2_TX_               PA7     // TXD2_
#define UART2_RX_               PA6     // RXD2_

// UART3
#define UART3_TX                PA5     // TXD3
#define UART3_RX                PA4     // RXD3

/*============================================================================
 * 定时器映射
 *============================================================================*/

#define TMR0_PIN                PA9     // TMR0
#define TMR0_PIN_               PB23    // TMR0_ (映射)
#define TMR1_PIN                PA10    // TMR1/X32KI
#define TMR1_PIN_               PB10    // TMR1_ (映射)
#define TMR2_PIN                PA11    // TMR2/X32KO
#define TMR2_PIN_               PB11    // TMR2_ (映射)
#define TMR3_PIN                PB22    // TMR3

#endif /* __CONFIG_PINS_CH592X_H__ */
