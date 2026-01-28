/**
 * @file gattprofile.h
 * @brief CH59X BLE GATT Profile Definitions
 */

#ifndef __GATTPROFILE_H__
#define __GATTPROFILE_H__

#include "CONFIG.h"

#ifdef __cplusplus
extern "C" {
#endif

// GATT Status codes
#define SUCCESS                     0x00
#define FAILURE                     0x01
#define INVALIDPARAMETER           0x02
#define ATT_ERR_INVALID_HANDLE      0x01
#define ATT_ERR_READ_NOT_PERMITTED  0x02
#define ATT_ERR_WRITE_NOT_PERMITTED 0x03
#define ATT_ERR_INVALID_VALUE_SIZE  0x0D
#define ATT_ERR_ATTR_NOT_FOUND      0x0A

// GATT Characteristic Properties
#define GATT_PROP_BCAST             0x01
#define GATT_PROP_READ              0x02
#define GATT_PROP_WRITE_NO_RSP      0x04
#define GATT_PROP_WRITE             0x08
#define GATT_PROP_NOTIFY            0x10
#define GATT_PROP_INDICATE          0x20
#define GATT_PROP_AUTHEN            0x40
#define GATT_PROP_EXTENDED          0x80

// GATT Permissions
#define GATT_PERMIT_READ            0x01
#define GATT_PERMIT_WRITE           0x02
#define GATT_PERMIT_AUTHEN_READ     0x04
#define GATT_PERMIT_AUTHEN_WRITE    0x08
#define GATT_PERMIT_ENCRYPT_READ    0x40
#define GATT_PERMIT_ENCRYPT_WRITE   0x80

// Client Characteristic Configuration
#define GATT_CLIENT_CFG_NOTIFY      0x0001
#define GATT_CLIENT_CFG_INDICATE    0x0002

// UUID sizes
#define ATT_BT_UUID_SIZE            2
#define ATT_UUID_SIZE               16

// Maximum encryption key size
#define GATT_MAX_ENCRYPT_KEY_SIZE   16

// Standard UUIDs
extern const uint8_t primaryServiceUUID[];
extern const uint8_t secondaryServiceUUID[];
extern const uint8_t characterUUID[];
extern const uint8_t clientCharCfgUUID[];

#define GATT_PRIMARY_SERVICE_UUID   0x2800
#define GATT_SECONDARY_SERVICE_UUID 0x2801
#define GATT_CHARACTER_UUID         0x2803
#define GATT_CLIENT_CHAR_CFG_UUID   0x2902

// Utility macros
#define LO_UINT16(val)  ((uint8_t)((val) & 0xFF))
#define HI_UINT16(val)  ((uint8_t)(((val) >> 8) & 0xFF))
#define BUILD_UINT16(lo, hi) ((uint16_t)(((uint16_t)(hi) << 8) | (uint16_t)(lo)))

// GATT Attribute structure
typedef struct {
    struct {
        uint8_t len;
        const uint8_t *uuid;
    } type;
    uint8_t permissions;
    uint16_t handle;
    uint8_t *pValue;
} gattAttribute_t;

// GATT Message types
typedef struct {
    uint8_t *pValue;
    uint16_t len;
    uint16_t handle;
} attHandleValueNoti_t;

typedef struct {
    uint8_t *pValue;
    uint16_t len;
    uint16_t handle;
} attHandleValueInd_t;

typedef void *gattMsg_t;

// GATT Service callbacks
typedef uint8_t (*pfnGATTReadAttrCB_t)(uint16_t connHandle, gattAttribute_t *pAttr,
                                        uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                                        uint16_t maxLen, uint8_t method);

typedef uint8_t (*pfnGATTWriteAttrCB_t)(uint16_t connHandle, gattAttribute_t *pAttr,
                                         uint8_t *pValue, uint16_t len, uint16_t offset,
                                         uint8_t method);

typedef struct {
    pfnGATTReadAttrCB_t pfnReadAttrCB;
    pfnGATTWriteAttrCB_t pfnWriteAttrCB;
    void *pfnAuthorizeAttrCB;
} gattServiceCBs_t;

// GATT Service functions
uint8_t GATTServApp_RegisterService(gattAttribute_t *pAttrs, uint16_t numAttrs,
                                     uint8_t encKeySize, const gattServiceCBs_t *pCBs);
uint8_t GATTServApp_DeregisterService(uint16_t handle, gattAttribute_t **ppAttrs);

// GATT Notification/Indication
uint8_t GATT_Notification(uint16_t connHandle, attHandleValueNoti_t *pNoti, uint8_t authenticated);
uint8_t GATT_Indication(uint16_t connHandle, attHandleValueInd_t *pInd, uint8_t authenticated);

// Buffer management
uint8_t *GATT_bm_alloc(uint16_t connHandle, uint8_t opcode, uint16_t size, uint16_t *pSizeAlloc);
void GATT_bm_free(gattMsg_t *pMsg, uint8_t opcode);

// Opcode definitions
#define ATT_HANDLE_VALUE_NOTI   0x1B
#define ATT_HANDLE_VALUE_IND    0x1D

#ifdef __cplusplus
}
#endif

#endif /* __GATTPROFILE_H__ */
