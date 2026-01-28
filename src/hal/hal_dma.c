/**
 * @file hal_dma.c
 * @brief DMA 驱动 - 优化传感器读取延迟
 * 
 * 目标: 传感器延迟 < 3ms
 * 
 * 实现策略:
 * 1. IMU 数据就绪中断触发
 * 2. DMA 自动传输数据
 * 3. 传输完成中断处理
 * 4. 零拷贝双缓冲
 */

#include "hal.h"
#include <string.h>

#ifdef CH59X
#include "CH59x_common.h"
#endif

/*============================================================================
 * 配置
 *============================================================================*/

#define DMA_BUFFER_SIZE     64
#define DMA_TIMEOUT_US      2000

/*============================================================================
 * DMA 缓冲区 (双缓冲)
 *============================================================================*/

static uint8_t __attribute__((aligned(4))) dma_buf_a[DMA_BUFFER_SIZE];
static uint8_t __attribute__((aligned(4))) dma_buf_b[DMA_BUFFER_SIZE];
static uint8_t *dma_active_buf = dma_buf_a;
static uint8_t *dma_ready_buf = dma_buf_b;

/*============================================================================
 * 状态管理
 *============================================================================*/

typedef struct {
    volatile bool busy;
    volatile bool complete;
    volatile uint32_t start_time_us;
    volatile uint32_t latency_us;
    void (*callback)(uint8_t *data, uint16_t len, void *ctx);
    void *callback_ctx;
    uint16_t transfer_len;
    
    // 统计
    uint32_t total_transfers;
    uint32_t max_latency_us;
    float avg_latency_us;
} dma_state_t;

static dma_state_t dma_spi = {0};
static dma_state_t dma_i2c = {0};

/*============================================================================
 * 初始化
 *============================================================================*/

void hal_dma_init(void)
{
    memset(&dma_spi, 0, sizeof(dma_spi));
    memset(&dma_i2c, 0, sizeof(dma_i2c));
    
#ifdef CH59X
    // TODO: 实现DMA时钟使能
    // 需要正确的寄存器定义
    // R32_SAFE_ACCESS_SIG = 0x57;
    // R32_SAFE_ACCESS_SIG = 0xA8;
    // R8_SLP_CLK_OFF0 &= ~(1 << 0);
    // R32_SAFE_ACCESS_SIG = 0;
#endif
}

/*============================================================================
 * SPI DMA 传输
 *============================================================================*/

int hal_spi_dma_start(uint8_t cs_pin, const uint8_t *tx_data, uint16_t len,
                       void (*callback)(uint8_t*, uint16_t, void*), void *ctx)
{
    if (dma_spi.busy || len > DMA_BUFFER_SIZE) {
        return -1;
    }
    
    dma_spi.busy = true;
    dma_spi.complete = false;
    dma_spi.callback = callback;
    dma_spi.callback_ctx = ctx;
    dma_spi.transfer_len = len;
    dma_spi.start_time_us = hal_micros();
    
    // 复制发送数据到 DMA 缓冲区
    if (tx_data) {
        memcpy(dma_active_buf, tx_data, len);
    } else {
        memset(dma_active_buf, 0xFF, len);
    }
    
#ifdef CH59X
    // 拉低 CS
    hal_gpio_write(cs_pin, 0);
    
    // TODO: 实现SPI DMA配置
    // 需要正确的寄存器定义
    // R32_SPI0_DMA_BEG = (uint32_t)dma_active_buf;
    // R32_SPI0_DMA_END = (uint32_t)(dma_active_buf + len);
    // R8_SPI0_INTER_EN |= RB_SPI_IE_DMA_END;
    // PFIC_EnableIRQ(SPI0_IRQn);
    // R8_SPI0_CTRL_CFG |= RB_SPI_DMA_ENABLE;
    
    // 暂时模拟完成
    dma_spi.complete = true;
    dma_spi.busy = false;
    if (callback) {
        callback(dma_active_buf, len, ctx);
    }
#else
    // 模拟完成
    dma_spi.complete = true;
    dma_spi.busy = false;
    if (callback) {
        callback(dma_active_buf, len, ctx);
    }
#endif
    
    return 0;
}

/*============================================================================
 * I2C DMA 读取
 *============================================================================*/

int hal_i2c_dma_read(uint8_t addr, uint8_t reg, uint16_t len,
                      void (*callback)(uint8_t*, uint16_t, void*), void *ctx)
{
    if (dma_i2c.busy || len > DMA_BUFFER_SIZE) {
        return -1;
    }
    
    dma_i2c.busy = true;
    dma_i2c.complete = false;
    dma_i2c.callback = callback;
    dma_i2c.callback_ctx = ctx;
    dma_i2c.transfer_len = len;
    dma_i2c.start_time_us = hal_micros();
    
#ifdef CH59X
    // CH592 I2C 使用中断驱动方式
    // 发送设备地址和寄存器
    uint32_t timeout;
    
    // TODO: 实现I2C DMA功能
    // 需要正确的寄存器定义，暂时使用普通I2C读取
    // 使用hal_i2c_read_reg作为替代
    return hal_i2c_read_reg(addr, reg, dma_active_buf, len);
#endif
    
    // 计算延迟
    dma_i2c.latency_us = hal_micros() - dma_i2c.start_time_us;
    dma_i2c.total_transfers++;
    
    if (dma_i2c.latency_us > dma_i2c.max_latency_us) {
        dma_i2c.max_latency_us = dma_i2c.latency_us;
    }
    dma_i2c.avg_latency_us = dma_i2c.avg_latency_us * 0.95f + dma_i2c.latency_us * 0.05f;
    
    dma_i2c.complete = true;
    dma_i2c.busy = false;
    
    // 交换缓冲区
    uint8_t *tmp = dma_active_buf;
    dma_active_buf = dma_ready_buf;
    dma_ready_buf = tmp;
    
    if (callback) {
        callback(dma_ready_buf, len, ctx);
    }
    
    return 0;
}

/*============================================================================
 * 状态查询
 *============================================================================*/

bool hal_dma_spi_busy(void)    { return dma_spi.busy; }
bool hal_dma_i2c_busy(void)    { return dma_i2c.busy; }
bool hal_dma_spi_complete(void) { return dma_spi.complete; }
bool hal_dma_i2c_complete(void) { return dma_i2c.complete; }

void hal_dma_get_stats(uint32_t *total, uint32_t *max_us, float *avg_us)
{
    if (total)  *total  = dma_i2c.total_transfers;
    if (max_us) *max_us = dma_i2c.max_latency_us;
    if (avg_us) *avg_us = dma_i2c.avg_latency_us;
}

uint8_t* hal_dma_get_buffer(void)
{
    return dma_ready_buf;
}

/*============================================================================
 * 中断处理
 *============================================================================*/

#ifdef CH59X
void SPI0_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void SPI0_IRQHandler(void)
{
    // TODO: 实现SPI DMA中断处理
    // if (R8_SPI0_INT_FLAG & RB_SPI_IF_DMA_END) {
    // TODO: 实现SPI DMA中断处理
    // if (R8_SPI0_INT_FLAG & RB_SPI_IF_DMA_END) {
    if (0) {  // 暂时禁用
        // R8_SPI0_INT_FLAG = RB_SPI_IF_DMA_END;
        
        // 计算延迟
        dma_spi.latency_us = hal_micros() - dma_spi.start_time_us;
        dma_spi.total_transfers++;
        
        if (dma_spi.latency_us > dma_spi.max_latency_us) {
            dma_spi.max_latency_us = dma_spi.latency_us;
        }
        dma_spi.avg_latency_us = dma_spi.avg_latency_us * 0.95f + dma_spi.latency_us * 0.05f;
        
        dma_spi.complete = true;
        dma_spi.busy = false;
        
        // 拉高 CS
        // hal_gpio_write(cs_pin, 1);  // 需要保存 cs_pin
        
        // 交换缓冲区
        uint8_t *tmp = dma_active_buf;
        dma_active_buf = dma_ready_buf;
        dma_ready_buf = tmp;
        
        if (dma_spi.callback) {
            dma_spi.callback(dma_ready_buf, dma_spi.transfer_len, dma_spi.callback_ctx);
        }
    }
}
#endif
