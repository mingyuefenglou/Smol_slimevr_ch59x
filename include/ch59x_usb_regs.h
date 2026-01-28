/**
 * @file ch59x_usb_regs.h
 * @brief CH59X USB 寄存器定义
 * 
 * 补充 CH59x_common.h 中可能缺失的 USB 相关定义
 * 用于 USB HID 和 Bootloader 实现
 */

#ifndef __CH59X_USB_REGS_H__
#define __CH59X_USB_REGS_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * USB 基地址
 *============================================================================*/

#ifndef USB_BASE_ADDR
#define USB_BASE_ADDR           0x40008000
#endif

/*============================================================================
 * USB 控制寄存器
 *============================================================================*/

#ifndef R8_USB_CTRL
#define R8_USB_CTRL             (*((volatile uint8_t *)(USB_BASE_ADDR + 0x00)))
#endif

// USB_CTRL 位定义
#ifndef RB_UC_HOST_MODE
#define RB_UC_HOST_MODE         0x80    // 主机模式 (0=设备)
#endif
#ifndef RB_UC_LOW_SPEED
#define RB_UC_LOW_SPEED         0x40    // 低速模式
#endif
#ifndef RB_UC_DEV_PU_EN
#define RB_UC_DEV_PU_EN         0x20    // 设备上拉使能
#endif
#ifndef RB_UC_SYS_CTRL_MASK
#define RB_UC_SYS_CTRL_MASK     0x18    // 系统控制掩码
#endif
#ifndef RB_UC_INT_BUSY
#define RB_UC_INT_BUSY          0x08    // 中断期间禁用响应
#endif
#ifndef RB_UC_RESET_SIE
#define RB_UC_RESET_SIE         0x04    // SIE 复位
#endif
#ifndef RB_UC_CLR_ALL
#define RB_UC_CLR_ALL           0x02    // 清除所有中断
#endif
#ifndef RB_UC_DMA_EN
#define RB_UC_DMA_EN            0x01    // DMA 使能
#endif

/*============================================================================
 * USB 中断寄存器
 *============================================================================*/

#ifndef R8_USB_INT_EN
#define R8_USB_INT_EN           (*((volatile uint8_t *)(USB_BASE_ADDR + 0x02)))
#endif

// 中断使能位
#ifndef RB_UIE_TRANSFER
#define RB_UIE_TRANSFER         0x02    // 传输完成中断
#endif
#ifndef RB_UIE_BUS_RST
#define RB_UIE_BUS_RST          0x01    // 总线复位中断
#endif
#ifndef RB_UIE_SUSPEND
#define RB_UIE_SUSPEND          0x04    // 挂起中断
#endif
#ifndef RB_UIE_FIFO_OV
#define RB_UIE_FIFO_OV          0x10    // FIFO 溢出中断
#endif
#ifndef RB_UIE_SOF
#define RB_UIE_SOF              0x08    // SOF 中断
#endif
#ifndef RB_UIE_DEV_NAK
#define RB_UIE_DEV_NAK          0x40    // NAK 中断
#endif

/*============================================================================
 * USB 设备地址
 *============================================================================*/

#ifndef R8_USB_DEV_AD
#define R8_USB_DEV_AD           (*((volatile uint8_t *)(USB_BASE_ADDR + 0x03)))
#endif

// 地址掩码
#ifndef RB_UDA_GP_BIT
#define RB_UDA_GP_BIT           0x80    // 通用位
#endif
#ifndef MASK_USB_ADDR
#define MASK_USB_ADDR           0x7F    // 地址掩码
#endif

/*============================================================================
 * USB 中断标志
 *============================================================================*/

#ifndef R8_USB_INT_FG
#define R8_USB_INT_FG           (*((volatile uint8_t *)(USB_BASE_ADDR + 0x06)))
#endif

// 中断标志位
#ifndef RB_UIF_TRANSFER
#define RB_UIF_TRANSFER         0x02    // 传输完成
#endif
#ifndef RB_UIF_BUS_RST
#define RB_UIF_BUS_RST          0x01    // 总线复位
#endif
#ifndef RB_UIF_SUSPEND
#define RB_UIF_SUSPEND          0x04    // 挂起
#endif
#ifndef RB_UIF_FIFO_OV
#define RB_UIF_FIFO_OV          0x10    // FIFO 溢出
#endif

/*============================================================================
 * USB 中断状态
 *============================================================================*/

#ifndef R8_USB_INT_ST
#define R8_USB_INT_ST           (*((volatile uint8_t *)(USB_BASE_ADDR + 0x07)))
#endif

// Token 类型
#ifndef MASK_UIS_TOKEN
#define MASK_UIS_TOKEN          0x30    // Token 掩码
#endif
#ifndef UIS_TOKEN_OUT
#define UIS_TOKEN_OUT           0x00    // OUT Token
#endif
#ifndef UIS_TOKEN_SOF
#define UIS_TOKEN_SOF           0x10    // SOF Token
#endif
#ifndef UIS_TOKEN_IN
#define UIS_TOKEN_IN            0x20    // IN Token
#endif
#ifndef UIS_TOKEN_SETUP
#define UIS_TOKEN_SETUP         0x30    // SETUP Token
#endif

// 端点掩码
#ifndef MASK_UIS_ENDP
#define MASK_UIS_ENDP           0x0F    // 端点号掩码
#endif

// 其他状态位
#ifndef RB_UIS_SETUP_ACT
#define RB_UIS_SETUP_ACT        0x80    // SETUP 事务
#endif
#ifndef RB_UIS_TOG_OK
#define RB_UIS_TOG_OK           0x40    // DATA0/1 切换正确
#endif

/*============================================================================
 * USB 杂项状态
 *============================================================================*/

#ifndef R8_USB_MIS_ST
#define R8_USB_MIS_ST           (*((volatile uint8_t *)(USB_BASE_ADDR + 0x05)))
#endif

#ifndef RB_UMS_SUSPEND
#define RB_UMS_SUSPEND          0x04    // 挂起状态
#endif
#ifndef RB_UMS_SOF_ACT
#define RB_UMS_SOF_ACT          0x08    // SOF 激活
#endif
#ifndef RB_UMS_SIE_FREE
#define RB_UMS_SIE_FREE         0x20    // SIE 空闲
#endif

/*============================================================================
 * USB 接收长度
 *============================================================================*/

#ifndef R8_USB_RX_LEN
#define R8_USB_RX_LEN           (*((volatile uint8_t *)(USB_BASE_ADDR + 0x04)))
#endif

/*============================================================================
 * 端点 DMA 地址
 *============================================================================*/

#ifndef R16_UEP0_DMA
#define R16_UEP0_DMA            (*((volatile uint16_t *)(USB_BASE_ADDR + 0x10)))
#endif
#ifndef R16_UEP1_DMA
#define R16_UEP1_DMA            (*((volatile uint16_t *)(USB_BASE_ADDR + 0x14)))
#endif
#ifndef R16_UEP2_DMA
#define R16_UEP2_DMA            (*((volatile uint16_t *)(USB_BASE_ADDR + 0x18)))
#endif
#ifndef R16_UEP3_DMA
#define R16_UEP3_DMA            (*((volatile uint16_t *)(USB_BASE_ADDR + 0x1C)))
#endif

/*============================================================================
 * 端点模式配置
 *============================================================================*/

#ifndef R8_UEP4_1_MOD
#define R8_UEP4_1_MOD           (*((volatile uint8_t *)(USB_BASE_ADDR + 0x0C)))
#endif
#ifndef R8_UEP2_3_MOD
#define R8_UEP2_3_MOD           (*((volatile uint8_t *)(USB_BASE_ADDR + 0x0D)))
#endif

// 端点模式位
#ifndef RB_UEP1_TX_EN
#define RB_UEP1_TX_EN           0x40    // EP1 TX 使能
#endif
#ifndef RB_UEP1_RX_EN
#define RB_UEP1_RX_EN           0x80    // EP1 RX 使能
#endif
#ifndef RB_UEP1_BUF_MOD
#define RB_UEP1_BUF_MOD         0x10    // EP1 缓冲模式
#endif
#ifndef RB_UEP4_TX_EN
#define RB_UEP4_TX_EN           0x04    // EP4 TX 使能
#endif
#ifndef RB_UEP4_RX_EN
#define RB_UEP4_RX_EN           0x08    // EP4 RX 使能
#endif
#ifndef RB_UEP2_TX_EN
#define RB_UEP2_TX_EN           0x04
#endif
#ifndef RB_UEP2_RX_EN
#define RB_UEP2_RX_EN           0x08
#endif
#ifndef RB_UEP3_TX_EN
#define RB_UEP3_TX_EN           0x40
#endif
#ifndef RB_UEP3_RX_EN
#define RB_UEP3_RX_EN           0x80
#endif

/*============================================================================
 * 端点控制寄存器
 *============================================================================*/

#ifndef R8_UEP0_CTRL
#define R8_UEP0_CTRL            (*((volatile uint8_t *)(USB_BASE_ADDR + 0x22)))
#endif
#ifndef R8_UEP1_CTRL
#define R8_UEP1_CTRL            (*((volatile uint8_t *)(USB_BASE_ADDR + 0x23)))
#endif
#ifndef R8_UEP2_CTRL
#define R8_UEP2_CTRL            (*((volatile uint8_t *)(USB_BASE_ADDR + 0x24)))
#endif
#ifndef R8_UEP3_CTRL
#define R8_UEP3_CTRL            (*((volatile uint8_t *)(USB_BASE_ADDR + 0x25)))
#endif
#ifndef R8_UEP4_CTRL
#define R8_UEP4_CTRL            (*((volatile uint8_t *)(USB_BASE_ADDR + 0x26)))
#endif

// 端点控制位
#ifndef MASK_UEP_T_RES
#define MASK_UEP_T_RES          0x03    // TX 响应掩码
#endif
#ifndef UEP_T_RES_ACK
#define UEP_T_RES_ACK           0x00    // TX ACK
#endif
#ifndef UEP_T_RES_NAK
#define UEP_T_RES_NAK           0x02    // TX NAK
#endif
#ifndef UEP_T_RES_STALL
#define UEP_T_RES_STALL         0x03    // TX STALL
#endif
#ifndef UEP_T_RES_NONE
#define UEP_T_RES_NONE          0x01    // TX 无响应
#endif

#ifndef MASK_UEP_R_RES
#define MASK_UEP_R_RES          0x0C    // RX 响应掩码
#endif
#ifndef UEP_R_RES_ACK
#define UEP_R_RES_ACK           0x00    // RX ACK
#endif
#ifndef UEP_R_RES_NAK
#define UEP_R_RES_NAK           0x08    // RX NAK
#endif
#ifndef UEP_R_RES_STALL
#define UEP_R_RES_STALL         0x0C    // RX STALL
#endif
#ifndef UEP_R_RES_NONE
#define UEP_R_RES_NONE          0x04    // RX 无响应
#endif

#ifndef RB_UEP_AUTO_TOG
#define RB_UEP_AUTO_TOG         0x10    // 自动切换 DATA0/1
#endif
#ifndef RB_UEP_T_TOG
#define RB_UEP_T_TOG            0x40    // TX DATA0/1
#endif
#ifndef RB_UEP_R_TOG
#define RB_UEP_R_TOG            0x80    // RX DATA0/1
#endif

/*============================================================================
 * 端点传输长度
 *============================================================================*/

#ifndef R8_UEP0_T_LEN
#define R8_UEP0_T_LEN           (*((volatile uint8_t *)(USB_BASE_ADDR + 0x20)))
#endif
#ifndef R8_UEP1_T_LEN
#define R8_UEP1_T_LEN           (*((volatile uint8_t *)(USB_BASE_ADDR + 0x21)))
#endif
#ifndef R8_UEP2_T_LEN
#define R8_UEP2_T_LEN           (*((volatile uint8_t *)(USB_BASE_ADDR + 0x28)))
#endif
#ifndef R8_UEP3_T_LEN
#define R8_UEP3_T_LEN           (*((volatile uint8_t *)(USB_BASE_ADDR + 0x29)))
#endif
#ifndef R8_UEP4_T_LEN
#define R8_UEP4_T_LEN           (*((volatile uint8_t *)(USB_BASE_ADDR + 0x2A)))
#endif

/*============================================================================
 * 安全访问 (用于时钟配置)
 *============================================================================*/

#ifndef R8_SAFE_ACCESS_SIG
#define R8_SAFE_ACCESS_SIG      (*((volatile uint8_t *)0x40001040))
#endif
#ifndef SAFE_ACCESS_SIG1
#define SAFE_ACCESS_SIG1        0x57
#endif
#ifndef SAFE_ACCESS_SIG2
#define SAFE_ACCESS_SIG2        0xA8
#endif

#ifndef R8_SLP_CLK_OFF0
#define R8_SLP_CLK_OFF0         (*((volatile uint8_t *)0x40001044))
#endif
#ifndef RB_SLP_CLK_USB
#define RB_SLP_CLK_USB          0x10    // USB 时钟
#endif

/*============================================================================
 * USB 中断号
 *============================================================================*/

#ifndef USB_IRQn
#define USB_IRQn                20      // USB 中断号
#endif

/*============================================================================
 * PFIC (中断控制器)
 *============================================================================*/

// PFIC 函数已在 CH59x_common.h 中声明，这里不再重复定义
// 如果需要内联实现，应该使用 IRQn_Type 类型

#ifdef __cplusplus
}
#endif

#endif /* __CH59X_USB_REGS_H__ */
