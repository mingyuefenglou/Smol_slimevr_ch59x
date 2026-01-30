/**
 * @file bootloader_main.c
 * @brief Minimal USB Bootloader for CH592
 * 
 * 功能:
 * - 检查 BOOT 按键 (PB4)
 * - 检查 Flash 应用有效性
 * - 跳转到应用程序 (0x1000)
 * - USB MSC DFU 模式 (如果按键按下或无有效应用)
 * 
 * 大小限制: < 4KB
 */

#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * CH592 寄存器定义 (最小化)
 *============================================================================*/

#define R32_CLK_SYS_CFG     (*((volatile uint32_t *)0x40001008))
#define R32_FLASH_CFG       (*((volatile uint32_t *)0x4000200C))
#define R32_PA_DIR          (*((volatile uint32_t *)0x400010A0))
#define R32_PA_PU           (*((volatile uint32_t *)0x400010A8))
#define R32_PA_PIN          (*((volatile uint32_t *)0x400010B0))
#define R32_PB_DIR          (*((volatile uint32_t *)0x400010C0))
#define R32_PB_PU           (*((volatile uint32_t *)0x400010C8))
#define R32_PB_PIN          (*((volatile uint32_t *)0x400010D0))

/* PFIC 寄存器 */
#define PFIC_BASE           0xE000E000
#define PFIC_CFGR           (*((volatile uint32_t *)(PFIC_BASE + 0x048)))

/* USB 寄存器 */
#define R8_USB_CTRL         (*((volatile uint8_t *)0x40008000))
#define R8_USB_INT_EN       (*((volatile uint8_t *)0x40008002))
#define R8_USB_DEV_AD       (*((volatile uint8_t *)0x40008003))
#define R8_USB_INT_FG       (*((volatile uint8_t *)0x40008006))
#define R8_USB_INT_ST       (*((volatile uint8_t *)0x40008007))
#define R16_USB_RX_LEN      (*((volatile uint16_t *)0x40008008))
#define RB_UC_DMA_EN        0x01
#define RB_UC_INT_BUSY      0x08
#define RB_UC_DEV_PU_EN     0x20
#define RB_USB_ADDR_MASK    0x7F
#define RB_UIF_BUS_RST      0x01
#define RB_UIF_TRANSFER     0x02
#define RB_UIF_SUSPEND      0x04

/* 应用起始地址 */
#define APP_START_ADDR      0x00001000
#define APP_VALID_MAGIC     0x20000020  /* 有效 SP 值的范围检查 */

/* BOOT 按键: PB4 */
#define BOOT_PIN            4

/*============================================================================
 * 延时函数
 *============================================================================*/

static void delay_ms(uint32_t ms)
{
    /* 简单延时，约 60MHz 时钟 */
    volatile uint32_t count = ms * 6000;
    while (count--) {
        __asm volatile ("nop");
    }
}

/*============================================================================
 * 检查 BOOT 按键
 *============================================================================*/

static bool check_boot_button(void)
{
    /* 配置 PB4 为输入上拉 */
    R32_PB_DIR &= ~(1 << BOOT_PIN);  /* 输入模式 */
    R32_PB_PU |= (1 << BOOT_PIN);    /* 上拉 */
    
    delay_ms(10);  /* 等待稳定 */
    
    /* 读取引脚状态，低电平表示按下 */
    return ((R32_PB_PIN & (1 << BOOT_PIN)) == 0);
}

/*============================================================================
 * 检查应用有效性
 *============================================================================*/

static bool check_app_valid(void)
{
    uint32_t *app_vectors = (uint32_t *)APP_START_ADDR;
    
    /* 检查复位向量是否有效 (不是空白 Flash) */
    if (app_vectors[0] == 0xFFFFFFFF) {
        return false;
    }
    
    /* 检查入口点是否在有效范围内 */
    uint32_t entry = app_vectors[0];
    if (entry < APP_START_ADDR || entry > 0x00070000) {
        return false;
    }
    
    return true;
}

/*============================================================================
 * 跳转到应用程序
 *============================================================================*/

static void jump_to_app(void)
{
    /* 禁用所有中断 */
    __asm volatile ("csrci mstatus, 8");
    
    /* 清除待处理中断 */
    PFIC_CFGR = 0;
    
    /* 获取应用入口点 */
    uint32_t *app_vectors = (uint32_t *)APP_START_ADDR;
    uint32_t app_entry = app_vectors[0];
    
    /* 跳转 */
    typedef void (*app_func_t)(void);
    app_func_t app = (app_func_t)app_entry;
    
    __asm volatile (
        "fence.i\n"       /* 清除指令缓存 */
        "jr %0\n"         /* 跳转到应用 */
        :
        : "r" (app_entry)
    );
    
    /* 永远不会到达这里 */
    while (1);
}

/*============================================================================
 * USB MSC Bootloader 模式 (简化版)
 * 
 * 注意: 完整的 USB MSC 实现较大，这里只做最小化占位
 * 实际使用时需要从应用程序中的 usb_bootloader.c 实现
 *============================================================================*/

static void enter_dfu_mode(void)
{
    /* LED 指示灯 (PA9) */
    R32_PA_DIR |= (1 << 9);  /* 输出模式 */
    
    /* 简单的等待循环，闪烁 LED */
    /* 在实际实现中，这里应该是 USB MSC 处理 */
    while (1) {
        /* LED 亮 */
        R32_PA_PIN |= (1 << 9);
        delay_ms(200);
        
        /* LED 灭 */
        R32_PA_PIN &= ~(1 << 9);
        delay_ms(200);
        
        /* 重新检查应用有效性 (可能通过其他方式更新了 Flash) */
        if (check_app_valid() && !check_boot_button()) {
            jump_to_app();
        }
    }
}

/*============================================================================
 * Bootloader 主函数
 *============================================================================*/

void bootloader_main(void)
{
    /* 1. 检查 BOOT 按键 */
    bool button_pressed = check_boot_button();
    
    /* 2. 检查应用有效性 */
    bool app_valid = check_app_valid();
    
    /* 3. 决策 */
    if (!button_pressed && app_valid) {
        /* 正常启动: 跳转到应用 */
        jump_to_app();
    }
    
    /* 4. 进入 DFU 模式 */
    enter_dfu_mode();
}
