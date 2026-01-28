/**
 * @file hal_i2c.c
 * @brief I2C Hardware Abstraction Layer for CH59X
 * 
 * This implementation uses CH59X's I2C peripheral to provide
 * a unified interface compatible with the SlimeVR sensor drivers.
 */

#include "hal.h"

#ifdef CH59X  // Only compile for CH59X target
#include "CH59x_common.h"
#include "CH59x_gpio.h"
#endif

// I2C pins for CH592
#define I2C_SDA_PIN     GPIO_Pin_12  // PB12
#define I2C_SCL_PIN     GPIO_Pin_13  // PB13

static uint8_t current_addr = 0;

int hal_i2c_init(const hal_i2c_config_t *config)
{
#ifdef CH59X
    // Configure GPIO for I2C
    GPIOB_SetBits(I2C_SDA_PIN | I2C_SCL_PIN);
    GPIOB_ModeCfg(I2C_SDA_PIN | I2C_SCL_PIN, GPIO_ModeOut_PP_5mA);
    
    // Note: CH59X uses software I2C by default
    // For hardware I2C, additional configuration is needed
    
    current_addr = config->addr;
#endif
    return 0;
}

// Software I2C implementation for flexibility
#ifdef CH59X

static inline void i2c_delay(void)
{
    // ~400kHz with 40MHz clock
    __NOP(); __NOP(); __NOP(); __NOP();
    __NOP(); __NOP(); __NOP(); __NOP();
}

static inline void sda_high(void) { GPIOB_SetBits(I2C_SDA_PIN); }
static inline void sda_low(void)  { GPIOB_ResetBits(I2C_SDA_PIN); }
static inline void scl_high(void) { GPIOB_SetBits(I2C_SCL_PIN); }
static inline void scl_low(void)  { GPIOB_ResetBits(I2C_SCL_PIN); }

static inline void sda_input(void)  { GPIOB_ModeCfg(I2C_SDA_PIN, GPIO_ModeIN_PU); }
static inline void sda_output(void) { GPIOB_ModeCfg(I2C_SDA_PIN, GPIO_ModeOut_PP_5mA); }
static inline bool sda_read(void)   { return (GPIOB_ReadPortPin(I2C_SDA_PIN) != 0); }

static void i2c_start(void)
{
    sda_output();
    sda_high();
    scl_high();
    i2c_delay();
    sda_low();
    i2c_delay();
    scl_low();
}

static void i2c_stop(void)
{
    sda_output();
    scl_low();
    sda_low();
    i2c_delay();
    scl_high();
    i2c_delay();
    sda_high();
    i2c_delay();
}

static bool i2c_write_byte(uint8_t byte)
{
    sda_output();
    for (int i = 0; i < 8; i++) {
        if (byte & 0x80)
            sda_high();
        else
            sda_low();
        byte <<= 1;
        i2c_delay();
        scl_high();
        i2c_delay();
        scl_low();
    }
    
    // Read ACK
    sda_input();
    i2c_delay();
    scl_high();
    i2c_delay();
    bool ack = !sda_read();
    scl_low();
    sda_output();
    
    return ack;
}

static uint8_t i2c_read_byte(bool ack)
{
    uint8_t byte = 0;
    sda_input();
    
    for (int i = 0; i < 8; i++) {
        byte <<= 1;
        scl_high();
        i2c_delay();
        if (sda_read())
            byte |= 1;
        scl_low();
        i2c_delay();
    }
    
    // Send ACK/NACK
    sda_output();
    if (ack)
        sda_low();
    else
        sda_high();
    i2c_delay();
    scl_high();
    i2c_delay();
    scl_low();
    sda_high();
    
    return byte;
}

#endif // CH59X

int hal_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, uint16_t len)
{
#ifdef CH59X
    // Write register address
    i2c_start();
    if (!i2c_write_byte(addr << 1)) {
        i2c_stop();
        return -1;
    }
    if (!i2c_write_byte(reg)) {
        i2c_stop();
        return -1;
    }
    
    // Repeated start for read
    i2c_start();
    if (!i2c_write_byte((addr << 1) | 1)) {
        i2c_stop();
        return -1;
    }
    
    // Read data
    for (uint16_t i = 0; i < len; i++) {
        data[i] = i2c_read_byte(i < len - 1);  // ACK all but last
    }
    
    i2c_stop();
    return 0;
#else
    // Stub for non-CH59X builds (testing)
    (void)addr; (void)reg; (void)data; (void)len;
    return -1;
#endif
}

int hal_i2c_write_reg(uint8_t addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
#ifdef CH59X
    i2c_start();
    if (!i2c_write_byte(addr << 1)) {
        i2c_stop();
        return -1;
    }
    if (!i2c_write_byte(reg)) {
        i2c_stop();
        return -1;
    }
    
    for (uint16_t i = 0; i < len; i++) {
        if (!i2c_write_byte(data[i])) {
            i2c_stop();
            return -1;
        }
    }
    
    i2c_stop();
    return 0;
#else
    (void)addr; (void)reg; (void)data; (void)len;
    return -1;
#endif
}
