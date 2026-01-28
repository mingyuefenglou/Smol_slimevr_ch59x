/**
 * @file CONFIG.h
 * @brief CH59X BLE Configuration
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "CH59x_common.h"

// BLE Configuration
#define BLE_MAC_ADDR_LEN        6
#define BLE_MAX_CONNECTIONS     1
#define BLE_TX_POWER_DBM        0
#define BLE_ADV_INTERVAL_MS     100

// TMOS (Task Management OS) Configuration
#define TMOS_TASK_MAX           8
#define TMOS_MSG_MAX            16

// Memory configuration
#define BLE_MEM_SIZE            4096

#endif /* __CONFIG_H__ */
