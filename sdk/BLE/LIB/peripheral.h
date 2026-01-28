/**
 * @file peripheral.h
 * @brief CH59X BLE Peripheral Role Definitions
 */

#ifndef __PERIPHERAL_H__
#define __PERIPHERAL_H__

#include "CONFIG.h"

#ifdef __cplusplus
extern "C" {
#endif

// GAP Role parameters
#define GAPROLE_DEVICE_NAME         0x01
#define GAPROLE_ADVERT_DATA         0x02
#define GAPROLE_SCAN_RSP_DATA       0x03
#define GAPROLE_ADV_INTERVAL_MIN    0x04
#define GAPROLE_ADV_INTERVAL_MAX    0x05
#define GAPROLE_ADVERT_ENABLED      0x06
#define GAPROLE_MIN_CONN_INTERVAL   0x07
#define GAPROLE_MAX_CONN_INTERVAL   0x08
#define GAPROLE_SLAVE_LATENCY       0x09
#define GAPROLE_TIMEOUT_MULTIPLIER  0x0A
#define GAPROLE_CONN_BD_ADDR        0x0B
#define GAPROLE_CONN_INTERVAL       0x0C
#define GAPROLE_CONN_LATENCY        0x0D
#define GAPROLE_CONN_TIMEOUT        0x0E
#define GAPROLE_PARAM_UPDATE_REQ    0x0F

// GAP Advertising types
#define GAP_ADTYPE_FLAGS                    0x01
#define GAP_ADTYPE_16BIT_MORE               0x02
#define GAP_ADTYPE_16BIT_COMPLETE           0x03
#define GAP_ADTYPE_32BIT_MORE               0x04
#define GAP_ADTYPE_32BIT_COMPLETE           0x05
#define GAP_ADTYPE_128BIT_MORE              0x06
#define GAP_ADTYPE_128BIT_COMPLETE          0x07
#define GAP_ADTYPE_LOCAL_NAME_SHORT         0x08
#define GAP_ADTYPE_LOCAL_NAME_COMPLETE      0x09
#define GAP_ADTYPE_POWER_LEVEL              0x0A
#define GAP_ADTYPE_MANUFACTURER_SPECIFIC    0xFF

// GAP Advertising flags
#define GAP_ADTYPE_FLAGS_LIMITED            0x01
#define GAP_ADTYPE_FLAGS_GENERAL            0x02
#define GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED 0x04

// GAP Events
#define GAP_LINK_ESTABLISHED_EVENT          0x01
#define GAP_LINK_TERMINATED_EVENT           0x02
#define GAP_LINK_PARAM_UPDATE_EVENT         0x03
#define GAP_ADV_COMPLETE_EVENT              0x04

// Link event structure
typedef struct {
    uint16_t connectionHandle;
    uint16_t connInterval;
    uint16_t connLatency;
    uint16_t connTimeout;
} gapLinkUpdateEvent_t;

// Peripheral role functions
void GAPRole_PeripheralInit(void);
uint8_t GAPRole_SetParameter(uint16_t param, uint8_t len, void *pValue);
uint8_t GAPRole_GetParameter(uint16_t param, void *pValue);
uint8_t GAPRole_TerminateConnection(void);

// BLE initialization
void CH59x_BLEInit(void);

#ifdef __cplusplus
}
#endif

#endif /* __PERIPHERAL_H__ */
