/**
 * @file CH59x_usbdev.c
 * @brief CH592 USB Device Driver (Stub Implementation)
 * 
 * This is a minimal stub implementation for compilation.
 * Replace with official WCH SDK implementation for production.
 */

#include "CH59x_usbdev.h"

// Weak implementations - override with actual SDK

__attribute__((weak))
void USB_DeviceInit(void)
{
    // Configure USB clock
    // Enable USB peripheral
    // Configure endpoints
    
    // Set pull-up enable
    R8_UDEV_CTRL = RB_UD_PORT_EN;
    
    // Enable interrupts
    R8_USB_INT_EN = RB_UIE_SUSPEND | RB_UIE_TRANSFER | RB_UIE_BUS_RST;
    
    // Reset device address
    R8_USB_DEV_AD = 0;
}

__attribute__((weak))
void USB_DeviceEnable(void)
{
    R8_USB_CTRL = RB_UC_DEV_PU_EN | RB_UC_INT_BUSY | RB_UC_SYS_CTRL0;
    R8_UDEV_CTRL |= RB_UD_PORT_EN;
}

__attribute__((weak))
void USB_DeviceDisable(void)
{
    R8_UDEV_CTRL &= ~RB_UD_PORT_EN;
    R8_USB_CTRL = RB_UC_RESET_SIE | RB_UC_CLR_ALL;
}

__attribute__((weak))
void USB_SetAddress(uint8_t addr)
{
    R8_USB_DEV_AD = addr & 0x7F;
}

__attribute__((weak))
void USB_EndpointConfig(uint8_t ep, uint8_t type, uint16_t size)
{
    (void)type;
    (void)size;
    
    switch (ep) {
        case 0:
            R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
            break;
        case 1:
            R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
            break;
        case 2:
            R8_UEP2_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
            break;
        default:
            break;
    }
}

__attribute__((weak))
uint8_t USB_EndpointWrite(uint8_t ep, const uint8_t *data, uint8_t len)
{
    uint8_t *buf;
    
    switch (ep) {
        case 0:
            buf = (uint8_t*)R32_UEP0_DMA;
            R8_UEP0_T_LEN = len;
            R8_UEP0_CTRL = (R8_UEP0_CTRL & ~RB_UEP_T_RES0) | UEP_T_RES_ACK;
            break;
        case 1:
            buf = (uint8_t*)R32_UEP1_DMA;
            R8_UEP1_T_LEN = len;
            R8_UEP1_CTRL = (R8_UEP1_CTRL & ~RB_UEP_T_RES0) | UEP_T_RES_ACK;
            break;
        default:
            return 0;
    }
    
    if (data && len > 0) {
        for (uint8_t i = 0; i < len; i++) {
            buf[i] = data[i];
        }
    }
    
    return len;
}

__attribute__((weak))
uint8_t USB_EndpointRead(uint8_t ep, uint8_t *data, uint8_t max_len)
{
    uint16_t len = R16_USB_RX_LEN;
    if (len > max_len) len = max_len;
    
    uint8_t *buf;
    switch (ep) {
        case 0:
            buf = (uint8_t*)R32_UEP0_DMA;
            break;
        default:
            return 0;
    }
    
    if (data && len > 0) {
        for (uint8_t i = 0; i < len; i++) {
            data[i] = buf[i];
        }
    }
    
    return (uint8_t)len;
}

__attribute__((weak))
void USB_EndpointStall(uint8_t ep)
{
    switch (ep) {
        case 0:
            R8_UEP0_CTRL = (R8_UEP0_CTRL & ~(RB_UEP_R_RES0 | RB_UEP_T_RES0)) | 
                           UEP_R_RES_STALL | UEP_T_RES_STALL;
            break;
        case 1:
            R8_UEP1_CTRL = (R8_UEP1_CTRL & ~RB_UEP_T_RES0) | UEP_T_RES_STALL;
            break;
        default:
            break;
    }
}

__attribute__((weak))
void USB_EndpointClearStall(uint8_t ep)
{
    switch (ep) {
        case 0:
            R8_UEP0_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK;
            break;
        case 1:
            R8_UEP1_CTRL = UEP_R_RES_ACK | UEP_T_RES_NAK | RB_UEP_AUTO_TOG;
            break;
        default:
            break;
    }
}

// Default callbacks (weak)
__attribute__((weak)) void USB_OnConfigured(void) {}
__attribute__((weak)) void USB_OnSuspend(void) {}
__attribute__((weak)) int USB_OnClassRequest(USB_SETUP_REQ *setup) { (void)setup; return -1; }
__attribute__((weak)) void USB_OnDataOut(uint8_t ep, const uint8_t *data, uint8_t len) { (void)ep; (void)data; (void)len; }
__attribute__((weak)) void USB_OnDataIn(uint8_t ep) { (void)ep; }
