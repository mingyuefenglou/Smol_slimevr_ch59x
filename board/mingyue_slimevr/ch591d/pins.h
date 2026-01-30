/**
 * @file pins.h
 * @brief CH591D (20-pin QFN) 引脚定义
 * 
 * CH591D Pin Configuration for SlimeVR Tracker
 */

#ifndef __BOARD_CH591D_PINS_H__
#define __BOARD_CH591D_PINS_H__

/*============================================================================
 * CH591D (20-pin QFN) 引脚排列图
 *
 * 重要：原理图存在引脚标签偏移错误！
 * Pin 3 (VIO33) 标签缺失，导致 Pin 6-11 标签全部偏移一位。
 * 以下定义基于芯片手册的正确引脚分配。
 *
 * CH591D 引脚映射：
 * Pin 1  = PA11 (INT1)      Pin 11 = PB4 (SW0/BOOT)
 * Pin 2  = PA10 (CHRG)      Pin 12 = X32MO
 * Pin 3  = VIO33 (3.3V)     Pin 13 = X32MI
 * Pin 4  = VDCID            Pin 14 = VINTA
 * Pin 5  = VSW              Pin 15 = ANT
 * Pin 6  = PA8 (ADC)        Pin 16 = VDCIA
 * Pin 7  = PA9 (LED)        Pin 17 = PA15 (MISO)
 * Pin 8  = PB11 (USB D+)    Pin 18 = PA14 (MOSI)
 * Pin 9  = PB10 (USB D-)    Pin 19 = PA13 (CLK)
 * Pin 10 = PB7 (RST)        Pin 20 = PA12 (CS)
 *============================================================================*/

/*============================================================================
 * ADC 通道映射
 *============================================================================*/
#define ADC_CH_PA8_AIN12        12

#endif /* __BOARD_CH591D_PINS_H__ */

